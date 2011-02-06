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

#include <GL/glew.h>
#include <stdlib.h>
#include <stdio.h>

#include "gl-framebuffer.h"

//------------------------------------------------------------------------------

static void test_framebuffer()
{
    switch (glCheckFramebufferStatus(GL_FRAMEBUFFER))
    {
    case GL_FRAMEBUFFER_COMPLETE:
        break; 
    case GL_FRAMEBUFFER_UNDEFINED:
        fprintf(stderr, "Framebuffer undefined\n");                     break;
    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
        fprintf(stderr, "Framebuffer incomplete attachment\n");         break;
    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
        fprintf(stderr, "Framebuffer incomplete missing attachment\n"); break;
    case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
        fprintf(stderr, "Framebuffer incomplete draw buffer\n");        break;
    case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
        fprintf(stderr, "Framebuffer incomplete read buffer\n");        break;
    case GL_FRAMEBUFFER_UNSUPPORTED:
        fprintf(stderr, "Framebuffer unsupported\n");                   break;
    case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
        fprintf(stderr, "Framebuffer incomplete multisample\n");        break;
    default:
        fprintf(stderr, "Framebuffer error\n");                         break;
    }
}

//------------------------------------------------------------------------------

static GLenum in(int c)
{
    static const GLenum i[] = { 0, GL_R32F, GL_RG32F, GL_RGB32F, GL_RGBA32F };
    return i[c];
}

static GLenum ex(int c)
{
    static const GLenum e[] = { 0, GL_R,    GL_RG,    GL_RGB,    GL_RGBA    };
    return e[c];
}

static void size_color(GLenum  T, GLuint  o,
                       GLsizei w, GLsizei h, GLsizei c)
{
    glBindTexture(T, o);

    glTexImage2D(T, 0, in(c), w, h, 0, ex(c), GL_FLOAT, NULL);

    glTexParameteri(T, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(T, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(T, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
    glTexParameteri(T, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);
}

static void size_depth(GLenum  T, GLuint  o,
                       GLsizei w, GLsizei h)
{
    glBindTexture(T, o);

    glTexImage2D(T, 0, GL_DEPTH_COMPONENT24, w, h, 0,
                 GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, NULL);

    glTexParameteri(T, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(T, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(T, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
    glTexParameteri(T, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);
}

//------------------------------------------------------------------------------

void gl_size_framebuffer(gl_framebuffer *F, GLsizei w, GLsizei h, GLsizei c)
{
    if (F->w != w || F->h != h || F->c != c)
    {
        GLenum target = GL_TEXTURE_RECTANGLE_ARB;
    
        size_color(target, F->color, w, h, c);
//      size_depth(target, F->depth, w, h);
    
        F->w = w;
        F->h = h;
        F->c = c;

        glBindFramebuffer(GL_FRAMEBUFFER, F->frame);
        {
            glFramebufferTexture2D(GL_FRAMEBUFFER,
                                   GL_COLOR_ATTACHMENT0, target, F->color, 0);
//          glFramebufferTexture2D(GL_FRAMEBUFFER,
//                                 GL_DEPTH_ATTACHMENT,  target, F->depth, 0);
            test_framebuffer();
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
}

void gl_init_framebuffer(gl_framebuffer *F, GLsizei w, GLsizei h, GLsizei c)
{
    glGenFramebuffers(1, &F->frame);
    glGenTextures    (1, &F->color);
//  glGenTextures    (1, &F->depth);

    F->w = 0;
    F->h = 0;
    F->c = 0;

    if (w && h && c) gl_size_framebuffer(F, w, h, c);
}

void gl_free_framebuffer(gl_framebuffer *F)
{
//  glDeleteTextures    (1, &F->depth);
    glDeleteTextures    (1, &F->color);
    glDeleteFramebuffers(1, &F->frame);
}

void *gl_copy_framebuffer(gl_framebuffer *F, GLint c)
{
    GLubyte *p = 0;

    glBindFramebuffer(GL_FRAMEBUFFER, F->frame);
    {
        if ((p = (GLubyte *) malloc(F->w * F->h * c * sizeof (GLfloat))))
        {
            glReadBuffer(GL_FRONT);
            glReadPixels(0, 0, F->w, F->h, ex(c), GL_FLOAT, p);
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return p;
}

//------------------------------------------------------------------------------
