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

#ifndef GL_PROGRAM_H
#define GL_PROGRAM_H

//------------------------------------------------------------------------------

struct gl_program
{
    GLuint program;
    GLuint vshader;
    GLuint fshader;
};

typedef struct gl_program gl_program;

//------------------------------------------------------------------------------

GLuint gl_load_vshader(const unsigned char *, unsigned int);
GLuint gl_load_fshader(const unsigned char *, unsigned int);

GLuint gl_load_program(GLuint, GLuint);

//------------------------------------------------------------------------------

void gl_init_program(gl_program *, const unsigned char *, unsigned int,
                                   const unsigned char *, unsigned int);
void gl_free_program(gl_program *);

//------------------------------------------------------------------------------

void gl_uniform1i(const gl_program *, const GLchar *, GLint);
void gl_uniform1f(const gl_program *, const GLchar *, GLfloat);
void gl_uniform2f(const gl_program *, const GLchar *, GLfloat, GLfloat);

#define GLUNIFORM1I(p, s, a)    glUniform1i(glGetUniformLocation(p, s), a);
#define GLUNIFORM2I(p, s, a, b) glUniform2i(glGetUniformLocation(p, s), a, b);
#define GLUNIFORM1F(p, s, a)    glUniform1f(glGetUniformLocation(p, s), a);
#define GLUNIFORM2F(p, s, a, b) glUniform2f(glGetUniformLocation(p, s), a, b);

//------------------------------------------------------------------------------

#endif
