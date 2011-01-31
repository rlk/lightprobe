#include <stddef.h>

/*----------------------------------------------------------------------------*/

struct gl_vertex
{
    GLfloat globe_pos[3];
    GLfloat chart_pos[2];
    GLfloat polar_pos[2];
};

struct gl_sphere
{
    GLsizei r;
    GLsizei c;

    GLuint  vert_buf;
    GLuint  fill_buf;
    GLuint  line_buf;
};

/*----------------------------------------------------------------------------*/

/* Initialize the vertex buffers for a sphere. Compute a normal and tangent   */
/* vector for each vertex, noting that position is trivially derived from     */
/* normal.                                                                    */

static void make_sphere(GLuint vb, GLuint fb, GLuint lb, GLsizei r, GLsizei c)
{
    GLfloat *v = 0;
    GLshort *f = 0;
    GLshort *l = 0;

    GLsizei i;
    GLsizei j;

    /* Compute the buffer sizes. */

    const GLsizei vn = 8 * sizeof (GLfloat) * (r + 1) * (c + 1);
    const GLsizei fn = 4 * sizeof (GLshort) * (    r * c);
    const GLsizei ln = 2 * sizeof (GLshort) * (4 * r + c);

    /* Compute the vertex normal, tangent, and spherical coordinate. */

    glBindBuffer(GL_ARRAY_BUFFER, vb);
    glBufferData(GL_ARRAY_BUFFER, vn, 0, GL_STATIC_DRAW);

    if ((v = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY)))
    {
        for     (i = 0; i <= r; i++)
            for (j = 0; j <= c; j++)
            {
                const double p = (      M_PI * i) / r - M_PI_2;
                const double t = (2.0 * M_PI * j) / c - M_PI;

                *v++ =  (GLfloat) (sin(t) * cos(p));
                *v++ =  (GLfloat) (         sin(p));
                *v++ =  (GLfloat) (cos(t) * cos(p));

                *v++ =  (GLfloat) cos(t);
                *v++ =  (GLfloat)   0.0f;
                *v++ = -(GLfloat) sin(t);

                *v++ =  (GLfloat) j / (GLfloat) c;
                *v++ =  (GLfloat) i / (GLfloat) r;
            }
        glUnmapBuffer(GL_ARRAY_BUFFER);
    }

    /* Compute the face indices. */

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, fb);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, fn, 0, GL_STATIC_DRAW);

    if ((f = glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY)))
    {
        for     (i = 0; i <  r; i++)
            for (j = 0; j <  c; j++)
            {
                *f++ = (GLushort) ((i    ) * (c + 1) + (j    ));
                *f++ = (GLushort) ((i + 1) * (c + 1) + (j    ));
                *f++ = (GLushort) ((i + 1) * (c + 1) + (j + 1));
                *f++ = (GLushort) ((i    ) * (c + 1) + (j + 1));
            }
        glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
    }

    /* Compute the line indices. */

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, lb);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, ln, 0, GL_STATIC_DRAW);

    if ((l = glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY)))
    {
        const int j2 = c / 2;
        const int j4 = c / 4;

        for (j = 0; j < c; j++)
        {
            *l++ = (GLushort) ((r / 2) * (c + 1) + (j    ));
            *l++ = (GLushort) ((r / 2) * (c + 1) + (j + 1));
        }
        for (i = 0; i < r; i++)
        {
            *l++ = (GLushort) ((i    ) * (c + 1)          );
            *l++ = (GLushort) ((i + 1) * (c + 1)          );
            *l++ = (GLushort) ((i    ) * (c + 1)      + j4);
            *l++ = (GLushort) ((i + 1) * (c + 1)      + j4);
            *l++ = (GLushort) ((i    ) * (c + 1) + j2     );
            *l++ = (GLushort) ((i + 1) * (c + 1) + j2     );
            *l++ = (GLushort) ((i    ) * (c + 1) + j2 + j4);
            *l++ = (GLushort) ((i + 1) * (c + 1) + j2 + j4);
        }
        glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
    }
}

/*----------------------------------------------------------------------------*/

static void draw(GLuint vb, GLsize size, GLsizei off,
                 GLuint eb, GLenum mode, GLsizei num)
{
    const GLsizei stride = (GLsizei) sizeof (gl_vertex);

    glBindBuffer(GL_ARRAY_BUFFER,         vb);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, eb);
    {
        glEnableClientState(GL_VERTEX_ARRAY);
        {
            glVertexPointer(size, GL_FLOAT, stride, (const GLvoid *) off);
            glDrawElements(mode, num, GL_UNSIGNED_SHORT, 0);
        }
        glDisableClientState(GL_VERTEX_ARRAY);
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER,         0);
}

static void gl_fill_globe(const gl_sphere *p)
{
    draw(p->vert_buf, 3, (GLsizei) offsetof (gl_vertex, globe_pos)
         p->fill_buf, GL_QUADS, 4 * p->r * p->c);
}

static void gl_fill_chart(const gl_sphere *p)
{
    draw(p->vert_buf, 2, (GLsizei) offsetof (gl_vertex, chart_pos)
         p->fill_buf, GL_QUADS, 4 * p->r * p->c);
}

static void gl_fill_polar(const gl_sphere *p)
{
    draw(p->vert_buf, 2, (GLsizei) offsetof (gl_vertex, polar_pos)
         p->fill_buf, GL_QUADS, 4 * p->r * p->c);
}

static void gl_line_globe(const gl_sphere *p)
{
    draw(p->vert_buf, 3, (GLsizei) offsetof (gl_vertex, globe_pos)
         p->line_buf, GL_LINES, 8 * p->r + 2 * p->c);
}

static void gl_line_chart(const gl_sphere *p)
{
    draw(p->vert_buf, 2, (GLsizei) offsetof (gl_vertex, chart_pos)
         p->line_buf, GL_LINES, 8 * p->r + 2 * p->c);
}

static void gl_line_polar(const gl_sphere *p)
{
    draw(p->vert_buf, 2, (GLsizei) offsetof (gl_vertex, polar_pos)
         p->line_buf, GL_LINES, 8 * p->r + 2 * p->c);
}

/*----------------------------------------------------------------------------*/
