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
    OpenGL utility classes header
*/

#ifndef GPUART_GL_UTILS_HEADER
#define GPUART_GL_UTILS_HEADER

#include <cassert>
#include <nanogui/nanogui.h>
#include <map>
#include <memory>

#include "math_types.h"


namespace gpuart
{

    namespace GL
    {
        /// Wrapper of an OpenGL object; movable, non-copyable
        template<void Deleter(GLuint)>
        class Wrapper
        {
            GLuint GLobj;

        public:

            explicit operator bool() const { return GLobj > 0; }

            Wrapper(): GLobj(0) { }
            Wrapper(GLuint obj): GLobj(obj) { }

            Wrapper(const Wrapper &)             = delete;
            Wrapper & operator=(const Wrapper &) = delete;

            Wrapper(Wrapper &&w)
            {
                Deleter(GLobj);
                GLobj = w.GLobj;
                w.GLobj = 0; // Subsequent deletion of OpenGL object named '0' does nothing
            }

            Wrapper & operator=(Wrapper &&w)
            {
                Deleter(GLobj);
                GLobj = w.GLobj;
                w.GLobj = 0; // Subsequent deletion of OpenGL object named '0' does nothing
                return *this;
            }

            ~Wrapper() { Deleter(GLobj); }

            GLuint &Get() { return GLobj; }

            GLuint GetConst() const { return GLobj; }

            void Delete() { Deleter(GLobj); GLobj = 0; }
        };

        /// Movable, non-copyable
        class EnableVertexAttribArray
        {
            GLint GLattrib;

        public:

            EnableVertexAttribArray() = delete;
            EnableVertexAttribArray(const EnableVertexAttribArray &)             = delete;
            EnableVertexAttribArray & operator=(const EnableVertexAttribArray &) = delete;
            EnableVertexAttribArray(EnableVertexAttribArray &&)                  = default;
            EnableVertexAttribArray & operator=(EnableVertexAttribArray &&)      = default;


            EnableVertexAttribArray(GLint attrib): GLattrib(attrib)
            {
                glEnableVertexAttribArray(GLattrib);
            }

            ~EnableVertexAttribArray()
            {
                glDisableVertexAttribArray(GLattrib);
            }

            GLint Get() const { return GLattrib; }
        };

        /// Movable, non-copyable
        class Buffer
        {
            static void Deleter(GLuint obj) { glDeleteBuffers(1, &obj); }
            Wrapper<Deleter> GLbuffer;

        public:

            explicit operator bool() const { return static_cast<bool>(GLbuffer); }

            Buffer() = default;

            Buffer(const Buffer &)             = delete;
            Buffer & operator=(const Buffer &) = delete;
            Buffer(Buffer &&)                  = default;
            Buffer & operator=(Buffer &&)      = default;

            Buffer(GLenum target,
                   const void *data,
                   GLsizei size,
                   GLenum usage)
            {
                glGenBuffers(1, &GLbuffer.Get());
                glBindBuffer(target, GLbuffer.Get());
                glBufferData(target, size, data, usage);
            }

            GLuint Get() const { return GLbuffer.GetConst(); }
        };

        /// Movable, non-copyable
        class Texture
        {
            static void Deleter(GLuint obj) { glDeleteTextures(1, &obj); }
            Wrapper<Deleter> GLtexture;

        public:

            explicit operator bool() const { return static_cast<bool>(GLtexture); }

            Texture() = default;

            Texture(const Texture &)             = delete;
            Texture & operator=(const Texture &) = delete;
            Texture(Texture &&)                  = default;
            Texture & operator=(Texture &&)      = default;

            /// Initializes a 2D texture
            Texture(GLint internalFormat, GLsizei width, GLsizei height,
                    GLenum format, GLenum type, const GLvoid *data, bool interpolated = false)
            {
                glGenTextures(1, &GLtexture.Get());
                glBindTexture(GL_TEXTURE_2D, GLtexture.Get());
                glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, type, data);

                GLint interpolation = GL_NEAREST;
                if (interpolated)
                    interpolation = GL_LINEAR;
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, interpolation);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, interpolation);
            }

            /// Initializes a buffer texture
            Texture(GLenum internalFormat, const Buffer &buffer)
            {
                glGenTextures(1, &GLtexture.Get());
                glBindTexture(GL_TEXTURE_BUFFER, GLtexture.Get());
                glTexBuffer(GL_TEXTURE_BUFFER, internalFormat, buffer.Get());
            }

            GLuint Get() const { return GLtexture.GetConst(); }
        };

        /// Movable, non-copyable
        class Shader
        {
            static void Deleter(GLuint obj) { glDeleteShader(obj); }
            Wrapper<Deleter> GLshader;

            std::unique_ptr<char[]> InfoLog;

        public:

            explicit operator bool() const { return static_cast<bool>(GLshader); }

            Shader() = default;

            Shader(const Shader &)             = delete;
            Shader & operator=(const Shader &) = delete;
            Shader(Shader &&)                  = default;
            Shader & operator=(Shader &&)      = default;

            Shader(GLenum type, const char *srcFileName);

            const char *GetInfoLog() const { return InfoLog.get(); }

            GLuint Get() const { return GLshader.GetConst(); }
        };

        /// Movable, non-copyable
        class VertexArrayObj
        {
            static void Deleter(GLuint obj) { glDeleteVertexArrays(1, &obj); }
            Wrapper<Deleter> GLvao;

        public:

            explicit operator bool() const { return static_cast<bool>(GLvao); }

            VertexArrayObj() = default;
            void Init()
            {
                GLvao.Delete();
                glGenVertexArrays(1, &GLvao.Get());
            }

            VertexArrayObj(const VertexArrayObj &)            = delete;
            VertexArrayObj& operator=(const VertexArrayObj &) = delete;
            VertexArrayObj(VertexArrayObj &&)                 = default;
            VertexArrayObj& operator=(VertexArrayObj &&)      = default;

            void Bind() { glBindVertexArray(GLvao.Get()); }

        };

        /// Movable, non-copyable
        class Program
        {
            static void Deleter(GLuint obj) { glDeleteProgram(obj); }
            Wrapper<Deleter> GLprogram;

            std::map<const char *, GLint> Uniforms;
            std::map<const char *, GLint> Attributes;

            std::unique_ptr<char[]> InfoLog;

            VertexArrayObj VAO;

        public:

            explicit operator bool() const { return static_cast<bool>(GLprogram); }

            Program() = default;

            Program(const Program &)             = delete;
            Program & operator=(const Program &) = delete;
            Program(Program &&)                  = default;
            Program & operator=(Program &&)      = default;

            Program(std::initializer_list<const Shader*> shaders,
                    std::initializer_list<const char *> uniforms,
                    std::initializer_list<const char *> attributes);

            const char *GetInfoLog() const { return InfoLog.get(); }

            void SetUniform1i(const char *uniform, GLint value)
            {
                assert(Uniforms.find(uniform) != Uniforms.end());
                glUniform1i(Uniforms[uniform], value);
            }

            void SetUniform1ui(const char *uniform, GLuint value)
            {
                assert(Uniforms.find(uniform) != Uniforms.end());
                glUniform1ui(Uniforms[uniform], value);
            }

            void SetUniform1f(const char *uniform, GLfloat f)
            {
                assert(Uniforms.find(uniform) != Uniforms.end());
                glUniform1f(Uniforms[uniform], f);
            }

            void SetUniform2f(const char *uniform, GLfloat f0, GLfloat f1)
            {
                assert(Uniforms.find(uniform) != Uniforms.end());
                glUniform2f(Uniforms[uniform], f0, f1);
            }

            void SetUniform3f(const char *uniform, const Vec3f &v)
            {
                SetUniform3f(uniform, v.x, v.y, v.z);
            }

            void SetUniform3f(const char *uniform, GLfloat f0, GLfloat f1, GLfloat f2)
            {
                assert(Uniforms.find(uniform) != Uniforms.end());
                glUniform3f(Uniforms[uniform], f0, f1, f2);
            }

            void SetUniform4f(const char *uniform, GLfloat f0, GLfloat f1, GLfloat f2, GLfloat f3)
            {
                assert(Uniforms.find(uniform) != Uniforms.end());
                glUniform4f(Uniforms[uniform], f0, f1, f2, f3);
            }

            void SetUniform4f(const char *uniform, const Vec3f &v, GLfloat f)
            {
                SetUniform4f(uniform, v.x, v.y, v.z, f);
            }

            GLint GetUniform(const char *uniform)     const { return Uniforms.find(uniform)->second; }
            GLint GetAttribute(const char *attribute) const { return Attributes.find(attribute)->second; }

            void Use()
            {
                VAO.Bind();
                glUseProgram(GLprogram.Get());
            }

            void Unbind() { glUseProgram(0); }
        };

        /// Movable, non-copyable
        class Framebuffer
        {
            static void Deleter(GLuint obj) { glDeleteFramebuffers(1, &obj); }
            Wrapper<Deleter> GLframebuffer;

            size_t NumAttachedTextures;
            GLint PrevBuf;

        public:

            explicit operator bool() const { return static_cast<bool>(GLframebuffer); }

            Framebuffer() = default;

            Framebuffer(const Framebuffer &)             = delete;
            Framebuffer & operator=(const Framebuffer &) = delete;
            Framebuffer(Framebuffer &&)                  = default;
            Framebuffer & operator=(Framebuffer &&)      = default;

            Framebuffer(std::initializer_list<Texture*> attachedTextures);

            void Bind();

            /// Binds the framebuffer that was bound prior to calling Bind()
            void Unbind()
            {
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, PrevBuf);
            }
        };

        class FramebufferBinder
        {
            Framebuffer &FB;
        public:

            FramebufferBinder(Framebuffer &fb): FB(fb)
            {
                FB.Bind();
            }

            ~FramebufferBinder()
            {
                FB.Unbind();
            }

            FramebufferBinder() = delete;
            FramebufferBinder(const FramebufferBinder &)             = delete;
            FramebufferBinder & operator=(const FramebufferBinder &) = delete;
            FramebufferBinder(FramebufferBinder &&)                  = delete;
            FramebufferBinder & operator=(FramebufferBinder &&)      = delete;
        };

        namespace Utils
        {
            /// Returns 'false' on failure
            bool DrawFullscreenQuad(GLint vertexPosAttrib);
        }

        bool Init();
    }
}

#endif // GPUART_GL_UTILS_HEADER
