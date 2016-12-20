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
    OpenGL utility classes implementation
*/


#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <string.h>
#include <vector>

#include "core.h"
#include "gl_utils.h"


static struct
{
    gpuart::GL::Buffer vertices;
    gpuart::GL::Buffer elements;
} FullscreenQuad; ///< Array buffers

const gpuart::GL::Buffer &GetFullscreenQuadVertices()
{
    return FullscreenQuad.vertices;
}

const gpuart::GL::Buffer &GetFullscreenQuadElements()
{
    return FullscreenQuad.elements;
}

static
std::unique_ptr<GLchar[]> ReadTextFile(const char *filename,
                                       /// Receives file length
                                       GLint *length)
{
    std::unique_ptr<GLchar[]> contents;

    std::ifstream file(filename, std::ios_base::in | std::ios_base::binary);
    if (file.fail())
        return nullptr;

    file.seekg(0, std::ios_base::end);
    *length = (GLint)file.tellg();

    file.seekg(0, std::ios_base::beg);
    contents.reset(new char[*length]);
    file.read(contents.get(), *length);

    return contents;
}

void ShowInfoLog(
    GLuint object,
    PFNGLGETSHADERIVPROC glGet__iv,
    PFNGLGETSHADERINFOLOGPROC glGet__InfoLog
)
{
    GLint logLength;

    glGet__iv(object, GL_INFO_LOG_LENGTH, &logLength);
    std::unique_ptr<char[]> log(new char[logLength]);
    glGet__InfoLog(object, logLength, nullptr, log.get());
    std::cout << log.get() << std::endl;
}

GLuint CreateShader(GLenum type, const char *filename)
{
    GLint length;
    std::unique_ptr<GLchar[]> source = ReadTextFile(filename, &length);
    GLuint shader;
    GLint shader_ok;

    if (!source)
        return 0;

    shader = glCreateShader(type);
    glShaderSource(shader, 1, (const GLchar**)&source, &length);

    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &shader_ok);
    if (!shader_ok)
    {
        std::cerr << "Failed to compile " << filename << ", log:\n";
        ShowInfoLog(shader, glGetShaderiv, glGetShaderInfoLog);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint CreateProgram(const std::initializer_list<GLuint> &shaders)
{
    GLint success;
    GLuint program = glCreateProgram();
    for (GLuint shader: shaders)
        glAttachShader(program, shader);

    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        std::cout << "Failed to link GLSL program, log:\n";
        ShowInfoLog(program, glGetProgramiv, glGetProgramInfoLog);
        glDeleteProgram(program);
        return 0;
    }
    else
        return program;
}

bool gpuart::GL::Init()
{
    const GLfloat vertexData[] = { -1, -1,
                                   1, -1,
                                   1, 1,
                                   -1, 1 };

    const GLuint elementData[] = { 0, 1, 2, 3 };

    FullscreenQuad.vertices = gpuart::GL::Buffer(
                                GL_ARRAY_BUFFER,
                                vertexData,
                                sizeof(vertexData),
                                GL_STATIC_DRAW);

    FullscreenQuad.elements = gpuart::GL::Buffer(
                                GL_ELEMENT_ARRAY_BUFFER,
                                elementData,
                                sizeof(elementData),
                                GL_STATIC_DRAW);

    return (FullscreenQuad.vertices && FullscreenQuad.elements);
}

gpuart::GL::Shader::Shader(GLenum type, const char *srcFileName)
{
    GLint srcLength;
    std::unique_ptr<GLchar[]> source = ReadTextFile(srcFileName, &srcLength);

    if (!source)
        GLshader = 0;
    else
    {
        GLshader.Get() = glCreateShader(type);
        glShaderSource(GLshader.Get(), 1, (const GLchar**)&source, &srcLength);

        glCompileShader(GLshader.Get());
        GLint success;
        glGetShaderiv(GLshader.Get(), GL_COMPILE_STATUS, &success);
        if (!success)
        {
            GLint logLength;
            glGetShaderiv(GLshader.Get(), GL_INFO_LOG_LENGTH, &logLength);
            InfoLog.reset(new char[logLength]);
            glGetShaderInfoLog(GLshader.Get(), logLength, nullptr, InfoLog.get());
            GLshader.Delete();
        }
    }
}

gpuart::GL::Program::Program(std::initializer_list<const Shader*> shaders,
                             std::initializer_list<const char *> uniforms,
                             std::initializer_list<const char *> attributes)
{
    VAO.Init();

    GLint success;
    GLprogram.Get() = glCreateProgram();
    for (auto *shader: shaders)
        glAttachShader(GLprogram.Get(), shader->Get());

    glLinkProgram(GLprogram.Get());

    glGetProgramiv(GLprogram.Get(), GL_LINK_STATUS, &success);

    if (!success)
    {
        GLint logLength;
        glGetProgramiv(GLprogram.Get(), GL_INFO_LOG_LENGTH, &logLength);
        InfoLog.reset(new char[logLength]);
        glGetProgramInfoLog(GLprogram.Get(), logLength, nullptr, InfoLog.get());
        GLprogram.Delete();
    }
    else
    {
        for (auto uniform: uniforms)
            if (-1 == (Uniforms[uniform] = glGetUniformLocation(GLprogram.Get(), uniform)))
            {
                GLprogram.Delete();
                return;
            }

        for (auto attribute: attributes)
            if (-1 == (Attributes[attribute] = glGetAttribLocation(GLprogram.Get(), attribute)))
            {
                GLprogram.Delete();
                return;
            }
    }
}

/// Returns 'false' on failure
bool gpuart::GL::Utils::DrawFullscreenQuad(GLint vertexPosAttrib)
{
    if (!GetFullscreenQuadVertices().Get() ||
        !GetFullscreenQuadElements().Get())
    {
        return false;
    }

    glBindBuffer(GL_ARRAY_BUFFER, GetFullscreenQuadVertices().Get());

    gpuart::GL::EnableVertexAttribArray enPosition(vertexPosAttrib);

    glVertexAttribPointer(
        enPosition.Get(),
        2,  // 2 components (x, y) per attribute value
        GL_FLOAT,
        GL_FALSE,
        sizeof(GLfloat)*2, // 2 components
        (GLvoid *)0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, GetFullscreenQuadElements().Get());

    glDrawElements(GL_TRIANGLE_FAN,
                   4,
                   // type of 'elements' array passed to CreateBuffer(GL_ELEMENT_ARRAY_BUFFER, ...
                   GL_UNSIGNED_INT,
                   (GLvoid *)0);

    return true;
}

gpuart::GL::Framebuffer::Framebuffer(std::initializer_list<Texture*> attachedTextures)
{
    PrevBuf = 0;

    GLint maxColorAttachments;
    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &maxColorAttachments);
    assert((int)attachedTextures.size() <= maxColorAttachments);

    GLint prevBuf;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevBuf);

    glGenFramebuffers(1, &GLframebuffer.Get());
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, GLframebuffer.Get());

    std::vector<GLenum> attachments(attachedTextures.size());

    for (size_t i = 0; i < attachedTextures.size(); i++)
    {
        glFramebufferTexture(GL_DRAW_FRAMEBUFFER,
                             GL_COLOR_ATTACHMENT0 + i,
                             (GLuint)(*(attachedTextures.begin() + i))->Get(), 0);

        attachments[i] = GL_COLOR_ATTACHMENT0 + i;
    }

    glDrawBuffers(attachments.size(), attachments.data());

    GLenum fbStatus = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
    if (fbStatus != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cerr << "Could not create FBO, status: " << fbStatus << std::endl;
        GLframebuffer.Delete();
    }

    NumAttachedTextures = attachedTextures.size();

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prevBuf);
}

void gpuart::GL::Framebuffer::Bind()
{
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &PrevBuf);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, GLframebuffer.Get());
}
