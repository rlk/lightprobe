// GL-PROGRAM Copyright (C) 2010 Robert Kooima
//
// This program is free software: you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
// details.

#include <GL/glew.h>
#include <stdlib.h>
#include <stdio.h>

#include "gl-program.h"

//------------------------------------------------------------------------------

// Greetings, Programs!

//------------------------------------------------------------------------------

// Check the shader compile status. If failed, print the log.

static int check_shader_log(GLuint shader, const unsigned char *glsl)
{
    const char *type = (shader == GL_VERTEX_SHADER) ? "vertex" : "fragment";

    GLchar *p = 0;
    GLint   s = 0;
    GLint   n = 0;

    glGetShaderiv(shader, GL_COMPILE_STATUS,  &s);
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &n);

    if (s == 0)
    {
        if ((p = (GLchar *) calloc(n + 1, 1)))
        {
            glGetShaderInfoLog(shader, n, NULL, p);

            fprintf(stderr, "OpenGL %s shader error:\n%s\n%s", type, p, glsl);
            free(p);
        }
        return 0;
    }
    return 1;
}

// Check the program link status. If failed, print the log.

static int check_program_log(GLuint program)
{
    GLchar *p = 0;
    GLint   s = 0;
    GLint   n = 0;

    glGetProgramiv(program, GL_LINK_STATUS,     &s);
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &n);

    if (s == 0)
    {
        if ((p = (GLchar *) calloc(n + 1, 1)))
        {
            glGetProgramInfoLog(program, n, NULL, p);

            fprintf(stderr, "OpenGL program linker error:\n%s", p);
            free(p);
        }
        return 0;
    }
    return 1;
}

//------------------------------------------------------------------------------

// Prepare a GLSL shader, taking it from source code to usable GL object.
// Report any errors in the log, and return 0 on failure.

static GLuint load_shader(GLenum type, const unsigned char *str,
                                             unsigned int   len)
{
    // Compile a new shader with the given source.

    GLuint shader = glCreateShader(type);

    glShaderSource (shader, 1, (const GLchar **) &str,
                               (const GLint   *) &len);
    glCompileShader(shader);

    // If the shader is valid, return it. Else, delete it.

    if (check_shader_log(shader, str))
        return shader;
    else
        glDeleteShader(shader);

    return 0;
}

// Short-cut the shader type parameter to load_shader.

GLuint gl_load_vshader(const unsigned char *str, unsigned int len)
{
    return load_shader(GL_VERTEX_SHADER,   str, len);
}

GLuint gl_load_fshader(const unsigned char *str, unsigned int len)
{
    return load_shader(GL_FRAGMENT_SHADER, str, len);
}

// Receive vertex and fragment shader objects. Link these into a GLSL program
// object, checking logs and reporting any errors. Return 0 on failure.

GLuint gl_load_program(GLuint vert_shader, GLuint frag_shader)
{
    if (vert_shader && frag_shader)
    {
        // Link a new program with the given shaders.

        GLuint program = glCreateProgram();

        glAttachShader(program, vert_shader);
        glAttachShader(program, frag_shader);

        glLinkProgram(program);

        // If the program is valid, return it.  Else, delete it.

        if (check_program_log(program))
            return program;
        else
            glDeleteProgram(program);
    }
    return 0;
}

//------------------------------------------------------------------------------

void gl_init_program(gl_program *P,
                     const unsigned char *vstr, unsigned int vlen,
                     const unsigned char *fstr, unsigned int flen)
{
    P->vshader = gl_load_vshader(vstr, vlen);
    P->fshader = gl_load_fshader(fstr, flen);

    P->program = gl_load_program(P->vshader, P->fshader);
}

void gl_free_program(gl_program *P)
{
    glDeleteProgram(P->program);

    glDeleteShader(P->vshader);
    glDeleteShader(P->fshader);

    P->program = 0;
    P->vshader = 0;
    P->fshader = 0;
}

//------------------------------------------------------------------------------

void gl_uniform1i(const gl_program *P, const GLchar *name, GLint a)
{
    glUniform1i(glGetUniformLocation(P->program, name), a);
}

void gl_uniform1f(const gl_program *P, const GLchar *name, GLfloat a)
{
    glUniform1f(glGetUniformLocation(P->program, name), a);
}

void gl_uniform2f(const gl_program *P, const GLchar *name, GLfloat a, GLfloat b)
{
    glUniform2f(glGetUniformLocation(P->program, name), a, b);
}

//------------------------------------------------------------------------------
