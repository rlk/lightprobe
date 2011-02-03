// GL-SPHERE Copyright (C) 2011 Robert Kooima
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

#include <math.h>
#include <assert.h>
#include <stddef.h>
#include <GL/glew.h>

#include "gl-sphere.h"

//------------------------------------------------------------------------------

struct vert
{
    GLfloat globe_pos[3];
    GLfloat chart_pos[2];
    GLfloat polar_pos[2];
};

struct quad
{
    GLshort a;
    GLshort b;
    GLshort c;
    GLshort d;
};

struct line
{
    GLshort a;
    GLshort b;
};

typedef struct vert vert;
typedef struct quad quad;
typedef struct line line;

//------------------------------------------------------------------------------

// Set the currently-bound vertex array buffer object to a spherical mesh with R
// rows and C columns. Compute vertex positions for globe, chart, and polar
// projections.

static void init_vert(int r, int c)
{
    vert *v;
    int   i;
    int   j;

    if ((v = (vert *) glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY)))
    {
        for     (i = 0; i <= r; i++)
            for (j = 0; j <= c; j++, v++)
            {
                const double x = (double) j / (double) c;
                const double y = (double) i / (double) r;

                const double p =       M_PI * y - M_PI_2;
                const double t = 2.0 * M_PI * x - M_PI;

                v->globe_pos[0] = (GLfloat) (sin(t) * cos(p));
                v->globe_pos[1] = (GLfloat) (         sin(p));
                v->globe_pos[2] = (GLfloat) (cos(t) * cos(p));

                v->chart_pos[0] = (GLfloat) (1 - x);
                v->chart_pos[1] = (GLfloat) (    y);

                v->polar_pos[0] = (GLfloat) (sin(t) * (2.0 - 2.0 * y));
                v->polar_pos[1] = (GLfloat) (cos(t) * (2.0 - 2.0 * y));
            }

        glUnmapBuffer(GL_ARRAY_BUFFER);
    }
}

// Set the currently-bound vertex element array buffer object to a spherical
// mesh with R rows and C columns of quads.

static void init_quad(int r, int c)
{
    quad *q;
    int   i;
    int   j;

    if ((q = (quad *) glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY)))
    {
        for     (i = 0; i < r; i++)
            for (j = 0; j < c; j++, q++)
            {
                q->a = (GLushort) ((i    ) * (c + 1) + (j    ));
                q->b = (GLushort) ((i + 1) * (c + 1) + (j    ));
                q->c = (GLushort) ((i + 1) * (c + 1) + (j + 1));
                q->d = (GLushort) ((i    ) * (c + 1) + (j + 1));
            }

        glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
    }
}

// Set the currently-bound vertex element array buffer object to the indices of
// three line loops: the equator, the prime meridian, and the 90 degree meridian
// of a sphere with R rows and C columns.

static void init_line(int r, int c)
{
    line *l;
    int   i;
    int   j;

    if ((l = (line *) glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY)))
    {
        const int j2 = c / 2;
        const int j4 = c / 4;

        for (j = 0; j < c; j++, l += 1)
        {
            l[0].a = (GLushort) ((r / 2) * (c + 1) + (j    ));
            l[0].b = (GLushort) ((r / 2) * (c + 1) + (j + 1));
        }
        for (i = 0; i < r; i++, l += 4)
        {
            l[0].a = (GLushort) ((i    ) * (c + 1)          );
            l[0].b = (GLushort) ((i + 1) * (c + 1)          );
            l[1].a = (GLushort) ((i    ) * (c + 1)      + j4);
            l[1].b = (GLushort) ((i + 1) * (c + 1)      + j4);
            l[2].a = (GLushort) ((i    ) * (c + 1) + j2     );
            l[2].b = (GLushort) ((i + 1) * (c + 1) + j2     );
            l[3].a = (GLushort) ((i    ) * (c + 1) + j2 + j4);
            l[3].b = (GLushort) ((i + 1) * (c + 1) + j2 + j4);
        }

        glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
    }
}

// Initialize the OpenGL resources needed for rendering a spherical mesh with R
// rows and C columns.

void gl_init_sphere(gl_sphere *p, int r, int c)
{
    p->r = (GLsizei) r;
    p->c = (GLsizei) c;

    // Generate vertex buffers.

    glGenBuffers(1, &p->vert_buf);
    glGenBuffers(1, &p->quad_buf);
    glGenBuffers(1, &p->line_buf);

    // Compute the buffer sizes.

    const GLsizei vn = sizeof (vert) * (r + 1) * (c + 1);
    const GLsizei qn = sizeof (quad) * (r    ) * (c    );
    const GLsizei ln = sizeof (line) * (4 * r + c);

    // Initialize the vertex buffer.

    glBindBuffer(GL_ARRAY_BUFFER,         p->vert_buf);
    glBufferData(GL_ARRAY_BUFFER,         vn, 0, GL_STATIC_DRAW);
    init_vert(r, c);

    // Compute the face elements.

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, p->quad_buf);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, qn, 0, GL_STATIC_DRAW);
    init_quad(r, c);

    // Compute the line elements.

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, p->line_buf);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, ln, 0, GL_STATIC_DRAW);
    init_line(r, c);

    // Don't leak the array buffer state.

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER,         0);
}

// Delete the spherical mesh vertex buffers.

void gl_free_sphere(gl_sphere *p)
{
    glDeleteBuffers(1, &p->line_buf);
    glDeleteBuffers(1, &p->quad_buf);
    glDeleteBuffers(1, &p->vert_buf);
}

//------------------------------------------------------------------------------

// Render vertex buffer VB with element buffer EB. MODE and NUM give the
// primitive type and element count. SIZE and OFF give the vertex position
// vector length and offset within the vertex buffer.

static void draw(GLuint eb, GLenum  mode, GLsizei num,
                 GLuint vb, GLsizei size, const GLvoid *off)
{
    const GLsizei stride = (GLsizei) sizeof (vert);

    glBindBuffer(GL_ARRAY_BUFFER,         vb);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, eb);
    {
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_NORMAL_ARRAY);
        {
            glVertexPointer(size, GL_FLOAT, stride, off);
            glNormalPointer(      GL_FLOAT, stride, 0);
            glDrawElements(mode, num, GL_UNSIGNED_SHORT, 0);
        }
        glDisableClientState(GL_NORMAL_ARRAY);
        glDisableClientState(GL_VERTEX_ARRAY);
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER,         0);
}

// Render the sphere using filled quads or lines, projecting it as a 3D globe,
// 2D chart, or 2D polar map. Choose the paramaters for the desired output and
// let the above draw function do the work.

void gl_fill_sphere(const gl_sphere *p, int m)
{
    static const GLsizei s[3] = { 3, 2, 2 };
    static const GLvoid *o[3] = {
        (GLvoid *) offsetof (vert, globe_pos),
        (GLvoid *) offsetof (vert, chart_pos),
        (GLvoid *) offsetof (vert, polar_pos),
    };

    assert(0 <= m && m < 3);

    draw(p->quad_buf, GL_QUADS, 4 * p->r * p->c, p->vert_buf, s[m], o[m]);
}

void gl_line_sphere(const gl_sphere *p, int m)
{
    static const GLsizei s[3] = { 3, 2, 2 };
    static const GLvoid *o[3] = {
        (GLvoid *) offsetof (vert, globe_pos),
        (GLvoid *) offsetof (vert, chart_pos),
        (GLvoid *) offsetof (vert, polar_pos),
    };

    assert(0 <= m && m < 3);

    draw(p->line_buf, GL_LINES, 8 * p->r + 2 * p->c, p->vert_buf, s[m], o[m]);
}
//------------------------------------------------------------------------------
