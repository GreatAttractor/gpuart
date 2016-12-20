/*
GPU-Assisted Ray Tracer
Copyright (C) 2016 Filip Szczerek <ga.software@yahoo.com>

This file is part of gpuart.

Gpuart is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Gpuart is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with gpuart.  If not, see <http://www.gnu.org/licenses/>.

File description:
    Renderer implementation
*/


#include <algorithm>
#include <cassert>
#include <chrono>
#include <initializer_list>
#include <iomanip>
#include <iostream>
#include <memory>

#include "bvh.h"
#include "math_types.h"
#include "renderer.h"
#include "utils.h"


using gpuart::Utils::TimeElapsed;


/* If set to 1, textures storing 3D vectors (ray directions, intersection coords, normals)
   will be RGBA32F instead of RGB32F. Required for Intel HD Graphics 5500 (Broadwell GT2) +
   Mesa 11.1.0 (git-525f3c2), because it cannot create a framebuffer with RGB32F color attachments. */
#define USE_RGBA_VECTOR_TEXTURES 1


#define PI 3.1415926f

/// Values correspond with identifiers used in shaders
namespace Uniforms
{
    const char *numPathsPerPixel = "NumPathsPerPixel";

    const char *rstart        = "RStart";
    const char *rdir          = "RDir";
    const char *primitive     = "Primitive";

    const char *bvh           = "BVH";

    const char *pos           = "Pos";

    const char *bottomLeft    = "BottomLeft";
    const char *deltaHorz     = "DeltaHorz";
    const char *deltaVert     = "DeltaVert";

    const char *sunDirAlt     = "SunDirAlt";
    const char *sunDirectLightingEnabled = "SunDirectLightingEnabled";

    const char *userSphere      = "UserSphere";
    const char *userSphereEm    = "UserSphereEm";
    const char *userSphereFlags = "UserSphereFlags";

    const char *radiance     = "Radiance";
    const char *prevRadiance = "PrevRadiance";
    const char *randSeed     = "RandSeed";
    const char *pixelSize    = "PixelSize";
    const char *cameraPos    = "CameraPos";
}

/// Values correspond with identifiers used in shaders
namespace Attributes
{
    const char *position = "Position";
}

gpuart::GL::Texture gpuart::Renderer::CreateTextureVec3(unsigned width, unsigned height, const GLvoid *data, bool interpolated) const
{
    GLenum internalFormat, format;
#if USE_RGBA_VECTOR_TEXTURES
    internalFormat = GL_RGBA32F;
    format = GL_RGBA;
#else
    internalFormat = GL_RGB32F;
    format = GL_RGB;
#endif

    return gpuart::GL::Texture(internalFormat, width, height, format, GL_FLOAT, data, interpolated);
}

static
bool CreateShader(gpuart::GL::Shader &shader, GLenum type, const char *srcFileName)
{
    shader = gpuart::GL::Shader(type, srcFileName);
    if (!shader)
    {
        if (shader.GetInfoLog())
            std::cerr << "Error compiling \"" << srcFileName << "\":\n"
                      << shader.GetInfoLog() << std::endl;

        return false;
    }
    else return true;
}

static
bool CreateProgram(gpuart::GL::Program &program,
                   std::initializer_list<const gpuart::GL::Shader*> shaders,
                   std::initializer_list<const char *> uniforms,
                   std::initializer_list<const char *> attributes)
{
    program = gpuart::GL::Program(shaders, uniforms, attributes);
    if (!program)
    {
        if (program.GetInfoLog())
            std::cerr << "Error creating program:\n"
                      << program.GetInfoLog() << std::endl;

        return false;
    }
    else return true;
}

/// Returns 'false' on failure
bool gpuart::Renderer::SetCamera(const Camera &cam)
{
    CurrentCamera = cam;

    float aspect = (float)Viewport.width/Viewport.height;

    // 'cam.Up' projected onto the plane orthogonal to 'cam.Dir'
    Vec3f up = ((cam.Dir ^ cam.Up) ^ cam.Dir).normalized();
    Vec3f target = cam.Pos + cam.Dir.normalized() * cam.ScreenDist;
    // Screen center to right edge
    Vec3f a = (cam.Dir.normalized() ^ up) * cam.ScreenDist * aspect * std::tan(cam.FovY/2 * PI/180);
    // Screen center to top edge
    Vec3f b = up * a.length() / aspect;


    SetDefaultGLState();
    GL::FramebufferBinder fb(CamInitFBO);
    gpuart::GL::Program &prog = Programs.cameraInit;
    prog.Use();

    prog.SetUniform3f(Uniforms::pos, cam.Pos);
    prog.SetUniform3f(Uniforms::bottomLeft, target - a - b);
    prog.SetUniform3f(Uniforms::deltaHorz, 2*a);
    prog.SetUniform3f(Uniforms::deltaVert, 2*b);

    if (!gpuart::GL::Utils::DrawFullscreenQuad(prog.GetAttribute(Attributes::position)))
        return false;

    ResetPathTracing();

    return true;
}

/// Returns 'false' on failure
bool gpuart::Renderer::InitPerPixelTextures()
{
    assert(Viewport.width > 0);
    assert(Viewport.height > 0);

    Rays.initial.start = CreateTextureVec3(Viewport.width, Viewport.height, nullptr);
    Rays.initial.dir = CreateTextureVec3(Viewport.width, Viewport.height, nullptr);

    // Order of output textures below corresponds with 'layout(location)'
    // of outputs in 'Shaders.cameraInit'
    CamInitFBO = GL::Framebuffer({ &Rays.initial.start,
                                   &Rays.initial.dir });
    if (!CamInitFBO)
        return false;

    for (auto i: {0, 1})
    {
        PathTracing.accumulator[i] = gpuart::GL::Texture(GL_RGBA32F, Viewport.width, Viewport.height,
                                                         GL_RGBA, GL_FLOAT, nullptr, false);
        PathTracing.accumFBO[i] = gpuart::GL::Framebuffer({ &PathTracing.accumulator[i] });
        if (!PathTracing.accumFBO[i])
            return false;
    }

    return SetCamera(CurrentCamera);
}

/** Use GetIsOK() to verify successful initialization.
    gpuart::GL::Init() has to be called prior to calling this constructor. */
gpuart::Renderer::Renderer(unsigned viewportWidth, unsigned viewportHeight, const gpuart::Camera &camera)
{
    IsOK = false;

    Lighting.Sun.azimuth = PI;
    Lighting.Sun.altitude = PI/4;
    Lighting.Sun.directLightingEnabled = true;

    UserSphere.pos = Vec3f(0, 0, 0);
    UserSphere.emittance = 0;
    UserSphere.radius = 0;
    UserSphere.flags = 0;

    PathTracing.pathsPerPixel = 5;
    PathTracing.pathsPerPass = PathTracing.pathsPerPixel;
    PathTracing.numPathsRendered = 0;
    PathTracing.selector = 0;


    if (!CreateShader(Shaders.Primitive.disc, GL_FRAGMENT_SHADER, "shaders/disc.glsl"))
        return;

    if (!CreateShader(Shaders.Primitive.sphere, GL_FRAGMENT_SHADER, "shaders/sphere.glsl"))
        return;

    if (!CreateShader(Shaders.Primitive.triangle, GL_FRAGMENT_SHADER, "shaders/triangle.glsl"))
        return;

    if (!CreateShader(Shaders.Primitive.cone, GL_FRAGMENT_SHADER, "shaders/cone.glsl"))
        return;

    if (!CreateShader(Shaders.Calc.intersection, GL_FRAGMENT_SHADER, "shaders/intersection.glsl"))
        return;

    if (!CreateShader(Shaders.Calc.bvhIntersection, GL_FRAGMENT_SHADER, "shaders/bvh_intersection.glsl"))
        return;

    if (!CreateShader(Shaders.Calc.sky, GL_FRAGMENT_SHADER, "shaders/sky.glsl"))
        return;

    if (!CreateShader(Shaders.RenderingStage.directLighting, GL_FRAGMENT_SHADER, "shaders/direct_lighting.glsl"))
        return;

    if (!CreateShader(Shaders.RenderingStage.pathTracing, GL_FRAGMENT_SHADER, "shaders/path_tracing.glsl"))
        return;

    if (!CreateShader(Shaders.RenderingStage.ptracingNormalize, GL_FRAGMENT_SHADER, "shaders/pt_normalize.glsl"))
        return;

    if (!CreateShader(Shaders.common, GL_FRAGMENT_SHADER, "shaders/common.glsl"))
        return;

    if (!CreateShader(Shaders.noise, GL_FRAGMENT_SHADER, "shaders/noise.glsl"))
        return;

    if (!CreateShader(Shaders.cameraInit, GL_FRAGMENT_SHADER, "shaders/cam_init.glsl"))
        return;

    if (!CreateShader(Shaders.vertex, GL_VERTEX_SHADER, "shaders/vertex.glsl"))
        return;

    if (!CreateProgram(Programs.directLighting,

                      { &Shaders.Primitive.sphere,
                        &Shaders.Primitive.disc,
                        &Shaders.Primitive.triangle,
                        &Shaders.Primitive.cone,

                        &Shaders.Calc.intersection,
                        &Shaders.Calc.sky,
                        &Shaders.Calc.bvhIntersection,

                        &Shaders.RenderingStage.directLighting,

                        &Shaders.common,
                        &Shaders.vertex },

                      { Uniforms::rstart,
                        Uniforms::rdir,

                        Uniforms::sunDirAlt,
                        Uniforms::sunDirectLightingEnabled,

                        Uniforms::bvh,

                        Uniforms::userSphere,
                        Uniforms::userSphereFlags },


                      { Attributes::position }))
    {
        return;
    }

    if (!CreateProgram(Programs.pathTracing,

                      { &Shaders.Primitive.sphere,
                        &Shaders.Primitive.disc,
                        &Shaders.Primitive.triangle,
                        &Shaders.Primitive.cone,

                        &Shaders.Calc.bvhIntersection,
                        &Shaders.Calc.intersection,
                        &Shaders.Calc.sky,

                        &Shaders.RenderingStage.pathTracing,

                        &Shaders.common,
                        &Shaders.noise,
                        &Shaders.vertex },

                      { Uniforms::numPathsPerPixel,

                        Uniforms::rstart,
                        Uniforms::rdir,

                        Uniforms::sunDirAlt,
                        Uniforms::sunDirectLightingEnabled,

                        Uniforms::bvh,

                        Uniforms::prevRadiance,
                        Uniforms::randSeed,
                        Uniforms::pixelSize,
                        Uniforms::cameraPos,

                        Uniforms::userSphere,
                        Uniforms::userSphereEm,
                        Uniforms::userSphereFlags },

                      { Attributes::position }))
    {
        return;
    }

    if (!CreateProgram(Programs.cameraInit,

                       { &Shaders.cameraInit,
                         &Shaders.vertex },

                       { Uniforms::pos,
                         Uniforms::bottomLeft,
                         Uniforms::deltaHorz,
                         Uniforms::deltaVert },

                       { Attributes::position }))
    {
        return;
    }

    if (!CreateProgram(Programs.ptracingNormalize,

                       { &Shaders.RenderingStage.ptracingNormalize,
                         &Shaders.vertex },

                       { Uniforms::radiance,
                         Uniforms::numPathsPerPixel },

                       { Attributes::position }))
    {
        return;
    }

    CurrentCamera = camera;

    Viewport.width = viewportWidth;
    Viewport.height = viewportHeight;

    if (!InitPerPixelTextures())
        return;

    IsOK = true;
}

void gpuart::Renderer::RenderDirectLighting()
{
    assert(IsOK);

    SetDefaultGLState();

    gpuart::GL::Program &prog = Programs.directLighting;
    prog.Use();

    GLenum texIdx = 0;
    glActiveTexture(GL_TEXTURE0 + texIdx);
    glBindTexture(GL_TEXTURE_2D, Rays.initial.dir.Get());
    prog.SetUniform1i(Uniforms::rdir, texIdx);
    texIdx++;

    glActiveTexture(GL_TEXTURE0 + texIdx);
    glBindTexture(GL_TEXTURE_2D, Rays.initial.start.Get());
    prog.SetUniform1i(Uniforms::rstart, texIdx);
    texIdx++;

    prog.SetUniform4f(Uniforms::sunDirAlt, GetSunDirection(), GetSunAltitude());
    prog.SetUniform1i(Uniforms::sunDirectLightingEnabled, IsSunDirectLightingEnabled());

    prog.SetUniform4f(Uniforms::userSphere, UserSphere.pos, UserSphere.radius);
    prog.SetUniform1ui(Uniforms::userSphereFlags, UserSphere.flags);

    glActiveTexture(GL_TEXTURE0 + texIdx);
    glBindTexture(GL_TEXTURE_BUFFER, BVH.tex.Get());
    prog.SetUniform1i(Uniforms::bvh, texIdx);
    texIdx++;

    gpuart::GL::Utils::DrawFullscreenQuad(prog.GetAttribute(Attributes::position));
}

/// Returns 'false' on failure
bool gpuart::Renderer::UpdateViewportSize(unsigned width, unsigned height)
{
    assert(width > 0);
    assert(height > 0);

    Viewport.width = width;
    Viewport.height = height;
    if (!InitPerPixelTextures())
    {
        IsOK = false;
    }
    return IsOK;
}


struct ByteCount
{
    size_t count;
    explicit ByteCount(size_t count): count(count) { }
};

std::ostream& operator <<(std::ostream &os, const ByteCount &bc)
{
    os << std::fixed << std::setprecision(1);
    if (bc.count < 1UL<<10)
        os << bc.count << " B";
    else if (bc.count < 1UL<<20)
        os << (float)bc.count / (1UL<<10) << " KiB";
    else if (bc.count < 1UL<<30)
        os << (float)bc.count / (1UL<<20) << " MiB";
    else
        os << (float)bc.count / (1UL<<30) << " GiB";

    return os;
}

/** May change the order of elements in 'primitives'. After calling this method,
    contents of 'primitives' are no longer used. */
void gpuart::Renderer::SetPrimitives(std::vector<Primitive*> &primitives, bool printInfo)
{
    std::chrono::high_resolution_clock::time_point tstart;
    if (printInfo)
    {
        std::cout << "Constructing BVH tree of " << primitives.size() << " primitives... "; std::cout.flush();
        tstart = std::chrono::high_resolution_clock::now();
    }

    BVH.tree = gpuart::BoundingVolumesHierarchy(primitives, 1024, 2);

    if (printInfo)
    {
        std::cout << "done (" << TimeElapsed(tstart) << ")." << std::endl;
        std::cout << "Compiling BVH tree... "; std::cout.flush();
        tstart = std::chrono::high_resolution_clock::now();
    }

    gpuart::Primitive::Data compiledTree;
    BVH.tree.Compile(compiledTree);

    if (printInfo)
        std::cout << "done (" << TimeElapsed(tstart) << ").\n";

    // Uncomment the following line only for debugging (lots of output):
    // std::cout << "\n\n\n"; gpuart::BoundingVolumesHierarchy::Print(compiledTree, std::cout);

    BVH.buf = gpuart::GL::Buffer(GL_TEXTURE_BUFFER,
                                 compiledTree.data(), (GLsizei)(compiledTree.size() * sizeof(decltype(compiledTree)::value_type)),
                                 GL_STATIC_DRAW);
    BVH.tex = gpuart::GL::Texture(GL_RGBA32F, BVH.buf);

    if (printInfo)
        std::cout << "Compiled tree occupies " << ByteCount(compiledTree.size() * sizeof(decltype(compiledTree)::value_type)) << "." << std::endl;
}

/// Cleans up the state after NanoGUI
void gpuart::Renderer::SetDefaultGLState()
{
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_CULL_FACE);
}

void gpuart::Renderer::ResetPathTracing()
{
    PathTracing.selector = 0;
    PathTracing.numPathsRendered = 0;

    SetDefaultGLState();
    PathTracing.accumFBO[0].Bind();
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    PathTracing.accumFBO[0].Unbind();
}

void gpuart::Renderer::RestartPathTracing(unsigned pathsPerPass, unsigned pathsPerPixel)
{
    if (pathsPerPass > pathsPerPixel)
        pathsPerPass = pathsPerPixel;

    PathTracing.pathsPerPixel = pathsPerPixel;
    PathTracing.pathsPerPass = pathsPerPass;

    ResetPathTracing();
}

/// Returns number of rendered paths per pixel
unsigned gpuart::Renderer::RenderPathTracingPass()
{
    assert(IsOK);

    GLenum texIdx;
    unsigned pathsToRender = 0;
    SetDefaultGLState();

    /* If the ratio of framebuffer size to "96 DPI-equivalent window size" is not 1
       (which may happen on some OSes under high-DPI displays), NanoGUI may reset
       the GL viewport to a size not exactly equal to the framebuffer size, which we
       pass using Renderer::UpdateViewportSize().

       Need to restore it here, otherwise our full-screen quad will not have expected
       coordinate ranges and path tracing accumulation will be incorrect. */
    glViewport(0, 0, Viewport.width, Viewport.height);

    unsigned src = PathTracing.selector;
    unsigned dest = src^1;

    if (PathTracing.numPathsRendered < PathTracing.pathsPerPixel)
    {
        // 1) Render a single path tracing pass to one of the "ping-ponged" accumulation textures

        PathTracing.lastDest = dest;

        PathTracing.accumFBO[dest].Bind();

        gpuart::GL::Program &prog = Programs.pathTracing;

        prog.Use();

        texIdx = 0;
        glActiveTexture(GL_TEXTURE0 + texIdx);
        glBindTexture(GL_TEXTURE_2D, Rays.initial.dir.Get());
        prog.SetUniform1i(Uniforms::rdir, texIdx);
        texIdx++;

        glActiveTexture(GL_TEXTURE0 + texIdx);
        glBindTexture(GL_TEXTURE_2D, Rays.initial.start.Get());
        prog.SetUniform1i(Uniforms::rstart, texIdx);
        texIdx++;

        glActiveTexture(GL_TEXTURE0 + texIdx);
        glBindTexture(GL_TEXTURE_BUFFER, BVH.tex.Get());
        prog.SetUniform1i(Uniforms::bvh, texIdx);
        texIdx++;

        glActiveTexture(GL_TEXTURE0 + texIdx);
        glBindTexture(GL_TEXTURE_2D, PathTracing.accumulator[src].Get());
        prog.SetUniform1i(Uniforms::prevRadiance, texIdx);
        texIdx++;

        pathsToRender = std::min(PathTracing.pathsPerPass,
                                 PathTracing.pathsPerPixel - PathTracing.numPathsRendered);

        prog.SetUniform1i(Uniforms::numPathsPerPixel, pathsToRender);

        // Calculate pixel size in world space
        prog.SetUniform1f(Uniforms::pixelSize,
                          2 * CurrentCamera.ScreenDist * std::tan(CurrentCamera.FovY/2 * PI/180) / Viewport.height);
        prog.SetUniform3f(Uniforms::cameraPos, CurrentCamera.Pos);

        prog.SetUniform4f(Uniforms::sunDirAlt, GetSunDirection(), GetSunAltitude());
        prog.SetUniform1i(Uniforms::sunDirectLightingEnabled, IsSunDirectLightingEnabled());

        prog.SetUniform4f(Uniforms::userSphere, UserSphere.pos, UserSphere.radius);
        prog.SetUniform3f(Uniforms::userSphereEm, Vec3f(1, 1, 1) * UserSphere.emittance);

        prog.SetUniform1ui(Uniforms::userSphereFlags, UserSphere.flags);

        std::uniform_real_distribution<float> distr(0, 1);
        prog.SetUniform4f(Uniforms::randSeed, distr(RndGen),
                                              distr(RndGen),
                                              distr(RndGen),
                                              distr(RndGen));

        gpuart::GL::Utils::DrawFullscreenQuad(prog.GetAttribute(Attributes::position));

        PathTracing.numPathsRendered += pathsToRender;

        PathTracing.accumFBO[dest].Unbind();

        // Switch the accumulators
        PathTracing.selector = PathTracing.selector ^ 1;
    }

    // 2) Render the normalized output of accumulated path tracing passes to screen

    // Make sure we render to the default (on-screen) framebuffer
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    Programs.ptracingNormalize.Use();

    texIdx = 0;
    glActiveTexture(GL_TEXTURE0 + texIdx);
    glBindTexture(GL_TEXTURE_2D, PathTracing.accumulator[PathTracing.lastDest].Get());
    Programs.ptracingNormalize.SetUniform1i(Uniforms::radiance, texIdx);
    texIdx++;

    Programs.ptracingNormalize.SetUniform1i(Uniforms::numPathsPerPixel, PathTracing.numPathsRendered);

    gpuart::GL::Utils::DrawFullscreenQuad(Programs.ptracingNormalize.GetAttribute(Attributes::position));

    return PathTracing.numPathsRendered;
}
