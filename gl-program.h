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

GLuint gl_load_vshader(const unsigned char *, unsigned int);
GLuint gl_load_fshader(const unsigned char *, unsigned int);

GLuint gl_load_program(GLuint, GLuint);

//------------------------------------------------------------------------------

GLuint gl_init_program(const unsigned char *, unsigned int,
                       const unsigned char *, unsigned int);
void   gl_free_program(GLuint);

//------------------------------------------------------------------------------

void gl_uniform1i(GLuint, const char *, GLint);
void gl_uniform1f(GLuint, const char *, GLfloat);
void gl_uniform2f(GLuint, const char *, GLfloat, GLfloat);

//------------------------------------------------------------------------------

#endif
