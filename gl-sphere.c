#include <math.h>
#include <stddef.h>
#include <GL/glew.h>

#include "gl-sphere.h"

/*----------------------------------------------------------------------------*/

struct gl_sphere
{
    GLsizei r;
    GLsizei c;

    GLuint  vert_buf;
    GLuint  quad_buf;
    GLuint  line_buf;
};

/*----------------------------------------------------------------------------*/

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

/*----------------------------------------------------------------------------*/

/* Initialize the currently-bound vertex array buffer object with a spherical */
/* mesh with R rows and C columns. Compute vertex positions for globe, chart, */
/* and polar projection rendering.                                            */

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

                v->chart_pos[0] = (GLfloat) x;
                v->chart_pos[1] = (GLfloat) y;

                v->polar_pos[0] = (GLfloat) (cos(t) * (2.0 - 2.0 * y));
                v->polar_pos[1] = (GLfloat) (sin(t) * (2.0 - 2.0 * y));
            }

        glUnmapBuffer(GL_ARRAY_BUFFER);
    }
}

/* Initialize the currently-bound vertex element array buffer object with a   */
/* spherical mesh with R rows and C columns of quads.                         */

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

/* Initialize the currently-bound vertex element array buffer object with the */
/* indices of three line loops: the equator and the 0 and 90 degree meridians */
/* of a sphere with R rows and C columns.                                     */

static void init_line(int r, int c)
{
    line *l;
    int   i;
    int   j;

    if ((l = (line *) glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY)))
    {
        const int j2 = c / 2;
        const int j4 = c / 4;

        for (j = 0; j < c; j++, l++)
        {
            l->a = (GLushort) ((r / 2) * (c + 1) + (j    ));
            l->b = (GLushort) ((r / 2) * (c + 1) + (j + 1));
        }
        for (i = 0; i < r; i++, l++)
        {
            l->a = (GLushort) ((i    ) * (c + 1)          );
            l->b = (GLushort) ((i + 1) * (c + 1)          );
            l->a = (GLushort) ((i    ) * (c + 1)      + j4);
            l->b = (GLushort) ((i + 1) * (c + 1)      + j4);
            l->a = (GLushort) ((i    ) * (c + 1) + j2     );
            l->b = (GLushort) ((i + 1) * (c + 1) + j2     );
            l->a = (GLushort) ((i    ) * (c + 1) + j2 + j4);
            l->b = (GLushort) ((i + 1) * (c + 1) + j2 + j4);
        }

        glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
    }
}

/* Initialize all OpenGL resources for rendering a sphere mesh with R rows    */
/* and C columns.                                                             */

void gl_init_sphere(gl_sphere *p, int r, int c)
{
    glGenBuffers(1, &p->vert_buf);
    glGenBuffers(1, &p->quad_buf);
    glGenBuffers(1, &p->line_buf);

    p->r = (GLsizei) r;
    p->c = (GLsizei) c;

    /* Compute the buffer sizes. */

    const GLsizei vn = sizeof (vert) * (r + 1) * (c + 1);
    const GLsizei qn = sizeof (quad) * (r    ) * (c    );
    const GLsizei ln = sizeof (line) * (4 * r + c);

    /* Initialize the vertex buffer */

    glBindBuffer(GL_ARRAY_BUFFER,         p->vert_buf);
    glBufferData(GL_ARRAY_BUFFER,         vn, 0, GL_STATIC_DRAW);
    init_vert(r, c);

    /* Compute the face elements. */

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, p->quad_buf);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, qn, 0, GL_STATIC_DRAW);
    init_quad(r, c);

    /* Compute the line elements. */

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, p->line_buf);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, ln, 0, GL_STATIC_DRAW);
    init_line(r, c);

    /* Revert the state. */

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER,         0);
}

void gl_free_sphere(gl_sphere *p)
{
    glDeleteBuffers(1, &p->line_buf);
    glDeleteBuffers(1, &p->quad_buf);
    glDeleteBuffers(1, &p->vert_buf);
}

/*----------------------------------------------------------------------------*/

/* Render the vertex buffer VB and element buffer EB. SIZE and OFF give the   */
/* number of vertex position components and offset within the vertex buffer.  */
/* MODE and NUM give the type of primative and total element count.           */

static void draw(GLuint eb, GLenum  mode, GLsizei num,
                 GLuint vb, GLsizei size, GLvoid *off)
{
    const GLsizei stride = (GLsizei) sizeof (vert);

    glBindBuffer(GL_ARRAY_BUFFER,         vb);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, eb);
    {
        glEnableClientState(GL_VERTEX_ARRAY);
        {
            glVertexPointer(size, GL_FLOAT, stride, off);
            glDrawElements(mode, num, GL_UNSIGNED_SHORT, 0);
        }
        glDisableClientState(GL_VERTEX_ARRAY);
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER,         0);
}

/* Render the sphere using filled quads or axis-aligned lines, projecting it  */
/* as a 3D globe, 2D chart, or 2D polar map. Call the draw function with the  */
/* position component count and struct offset to the attribute giving the     */
/* requested projection. Include an element buffer, mode, and count for the   */
/* desired primitive type.                                                    */

void gl_fill_globe(const gl_sphere *p)
{
    draw(p->quad_buf, GL_QUADS, 4 * p->r * p->c,
         p->vert_buf, 3, (GLvoid *) offsetof (vert, globe_pos));
}

void gl_fill_chart(const gl_sphere *p)
{
    draw(p->quad_buf, GL_QUADS, 4 * p->r * p->c,
         p->vert_buf, 2, (GLvoid *) offsetof (vert, chart_pos));
}

void gl_fill_polar(const gl_sphere *p)
{
    draw(p->quad_buf, GL_QUADS, 4 * p->r * p->c,
         p->vert_buf, 2, (GLvoid *) offsetof (vert, polar_pos));
}

void gl_line_globe(const gl_sphere *p)
{
    draw(p->line_buf, GL_LINES, 8 * p->r + 2 * p->c,
         p->vert_buf, 3, (GLvoid *) offsetof (vert, globe_pos));
}

void gl_line_chart(const gl_sphere *p)
{
    draw(p->line_buf, GL_LINES, 8 * p->r + 2 * p->c,
         p->vert_buf, 2, (GLvoid *) offsetof (vert, chart_pos));
}

void gl_line_polar(const gl_sphere *p)
{
    draw(p->line_buf, GL_LINES, 8 * p->r + 2 * p->c,
         p->vert_buf, 2, (GLvoid *) offsetof (vert, polar_pos));
}

/*----------------------------------------------------------------------------*/
