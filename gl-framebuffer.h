// GL-FRAMEBUFFER Copyright (C) 2010 Robert Kooima
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

#ifndef GL_FRAMEBUFFER_H
#define GL_FRAMEBUFFER_H

//------------------------------------------------------------------------------

struct gl_framebuffer
{
    GLsizei w;
    GLsizei h;
    GLuint  frame;
    GLuint  color;
    GLuint  depth;
};

typedef struct gl_framebuffer gl_framebuffer;

//------------------------------------------------------------------------------

void  gl_size_framebuffer(gl_framebuffer *, GLsizei, GLsizei);
void  gl_init_framebuffer(gl_framebuffer *, GLsizei, GLsizei);
void  gl_free_framebuffer(gl_framebuffer *);
void *gl_copy_framebuffer(gl_framebuffer *, GLint);

//------------------------------------------------------------------------------

#endif
