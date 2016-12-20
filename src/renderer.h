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
    Renderer header
*/

#ifndef GPUART_RENDERER_HEADER
#define GPUART_RENDERER_HEADER

#include <nanogui/nanogui.h>
#include <memory>
#include <random>

#include "bvh.h"
#include "core.h"
#include "gl_utils.h"
#include "math_types.h"


namespace gpuart
{
    class Camera
    {
    public:
        Vec3f Pos; ///< Camera position
        Vec3f Dir; ///< Viewing direction
        Vec3f Up;  ///< Camera's "up" direction
        float FovY; ///< Vertical field of view in degrees

        /// Pos-screen distance; camera rays originate at the screen
        float ScreenDist;
    };

    class Renderer
    {
        bool IsOK;

        /// Ray arrays (textures)
        struct RayTex_t
        {
            GL::Texture start; ///< Ray's origin
            GL::Texture dir;   ///< Ray's direction
        };

        struct
        {
            RayTex_t initial;
            RayTex_t data[2];
            unsigned current; /// Indicates current element in 'data' (0 or 1)
        } Rays;

        GL::Framebuffer FBO[2];

        struct
        {
            GL::Texture tex;
            GL::Buffer buf;
            BoundingVolumesHierarchy tree;
        } BVH;

        struct
        {
            unsigned selector; ///< Indicates the source & destination in 'accumulator'
            GL::Texture accumulator[2];
            GL::Framebuffer accumFBO[2];
            unsigned lastDest;

            /// Number or paths rendered since the last call to RestartPathTracing()
            unsigned numPathsRendered;

            unsigned pathsPerPixel;
            unsigned pathsPerPass; ///< less than or equal to 'pathsPerPixel'
        } PathTracing;

        struct
        {
            struct
            {
                GL::Shader sphere, disc, triangle, cone;
            } Primitive;

            struct
            {
                GL::Shader intersection;
                GL::Shader bvhIntersection;
                GL::Shader sky;
            } Calc;

            struct
            {
                GL::Shader directLighting,
                           pathTracing,
                           ptracingNormalize;
            } RenderingStage;

            GL::Shader cameraInit;
            GL::Shader common;
            GL::Shader vertex;
            GL::Shader noise;
        } Shaders;

        struct
        {
            //GL::Program intersections;
            GL::Program directLighting;
            GL::Program pathTracing;
            GL::Program ptracingNormalize;
            GL::Program cameraInit;
        } Programs;

        struct
        {
            /// Direction towards the Sun
            struct
            {
                float azimuth; ///< 0 to 2*pi
                float altitude; ///< 0 to pi/2

                bool directLightingEnabled;
            } Sun;

        } Lighting;

        enum UserSphereFlags: uint32_t
        {
            EM_NONZERO = 1U << 0, ///< non-zero emittance
            SPECULAR   = 1U << 1,
            FUZZY      = 1U << 2  ///< fuzzy specular reflection
        };

        struct
        {
            Vec3f pos;
            float radius;
            float emittance;

            uint32_t flags;
        } UserSphere;

        Camera CurrentCamera;
        GL::Framebuffer CamInitFBO; ///< Used for initializing camera rays in a shader

        struct
        {
            unsigned width, height;
        } Viewport;

        std::mt19937 RndGen;

        GL::Texture CreateTextureVec3(unsigned width, unsigned height, const GLvoid *data, bool interpolated = false) const;

        /// Returns 'false' on failure
        bool InitPerPixelTextures();

        /// Cleans up the state after NanoGUI
        void SetDefaultGLState();

        Vec3f GetSunDirection() const
        {
            return Vec3f(1, 0, 0).vroty(-Lighting.Sun.altitude)
                                 .vrotz(Lighting.Sun.azimuth);
        }

        void ResetPathTracing();

    public:
        /** Use GetIsOK() to verify successful initialization.
            gpuart::GL::Init() has to be called prior to calling this constructor. */
        Renderer(unsigned viewportWidth, unsigned viewportHeight, const Camera &camera);

        /** May change the order of elements in 'primitives'. After calling this method,
            contents of 'primitives' are no longer used. */
        void SetPrimitives(std::vector<Primitive*> &primitives, bool printInfo);

        /// Returns 'false' on failure
        bool UpdateViewportSize(unsigned width, unsigned height);

        /// Returns 'false' on failure
        bool SetCamera(const Camera &cam);

        /// Sets Sun's azimuth (0 to 2*pi)
        void SetSunAzimuth(float azimuth)
        {
            Lighting.Sun.azimuth = azimuth;
            ResetPathTracing();
        }
        float GetSunAzimuth() const { return Lighting.Sun.azimuth; }

        /// Sets Sun's altitude (0 to pi/2)
        void SetSunAltitude(float altitude)
        {
            Lighting.Sun.altitude = altitude;
            ResetPathTracing();
        }
        float GetSunAltitude() const { return Lighting.Sun.altitude; }

        void SetSunDirectLighting(bool enabled = true)
        {
            Lighting.Sun.directLightingEnabled = enabled;
            ResetPathTracing();
        }

        bool IsSunDirectLightingEnabled() const { return Lighting.Sun.directLightingEnabled; }

        /// Use radius=0 to effectively disable the user-controlled sphere
        void SetUserSphere(const Vec3f &pos, float radius, float emittance)
        {
            UserSphere.pos = pos;
            UserSphere.radius = radius;
            UserSphere.emittance = emittance;

            if (emittance > 0)
                UserSphere.flags |= UserSphereFlags::EM_NONZERO;
            else
                UserSphere.flags &= ~UserSphereFlags::EM_NONZERO;

            ResetPathTracing();
        }

        void SetUserSphereSpecular(bool specular)
        {
            if (specular)
                UserSphere.flags |= UserSphereFlags::SPECULAR;
            else
                UserSphere.flags &= ~UserSphereFlags::SPECULAR;

            ResetPathTracing();
        }

        void SetUserSphereFuzzy(bool fuzzy)
        {
            if (fuzzy)
                UserSphere.flags |= UserSphereFlags::FUZZY;
            else
                UserSphere.flags &= ~UserSphereFlags::FUZZY;

            ResetPathTracing();
        }

        /// Use radius=0 to effectively disable the user-controlled sphere
        void SetUserSphereRadius(float radius)
        {
            UserSphere.radius = radius;
            ResetPathTracing();
        }

        void SetUserSpherePos(const Vec3f &pos)
        {
            UserSphere.pos = pos;
            ResetPathTracing();
        }

        Vec3f GetUserSpherePos()        const { return UserSphere.pos; }
        float GetUserSphereRadius()     const { return UserSphere.radius; }
        float GetUserSphereEmittance() const  { return UserSphere.emittance; }

        void SetUserSphereEmittance(float em)
        {
            UserSphere.emittance = em;
            if (em > 0)
                UserSphere.flags |= UserSphereFlags::EM_NONZERO;
            else
                UserSphere.flags &= ~UserSphereFlags::EM_NONZERO;

            ResetPathTracing();
        }

        void RenderDirectLighting();

        void RestartPathTracing(unsigned pathsPerPass, unsigned pathsPerPixel);

        /// Returns number of rendered paths per pixel
        unsigned RenderPathTracingPass();

        unsigned GetPathsPerPixel() const { return PathTracing.pathsPerPixel; }

        bool GetIsOK() const { return IsOK; }

    };
}

#endif // GPUART_RENDERER_HEADER
