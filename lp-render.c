/* LIGHTPROBE Copyright (C) 2010 Robert Kooima                                */
/*                                                                            */
/* This program is free software: you can redistribute it and/or modify it    */
/* under the terms of the GNU General Public License as published by the Free */
/* Software Foundation, either version 3 of the License, or (at your option)  */
/* any later version.                                                         */
/*                                                                            */
/* This program is distributed in the hope that it will be useful, but        */
/* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY */
/* or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License   */
/* for more details.                                                          */

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tiffio.h>
#include <GL/glew.h>

#include "lp-render.h"

/*----------------------------------------------------------------------------*/

#define UNIFORM1I(p, s, i)    glUniform1i(glGetUniformLocation(p, s), i)
#define UNIFORM1F(p, s, f)    glUniform1f(glGetUniformLocation(p, s), f)
#define UNIFORM2F(p, s, a, b) glUniform2f(glGetUniformLocation(p, s), a, b)

/*----------------------------------------------------------------------------*/

#define CIRCLE_VERT "lp-circle.vert"
#define CIRCLE_FRAG "lp-circle.frag"
#define SPHERE_VERT "lp-sphere.vert"
#define SPHERE_FRAG "lp-sphere.frag"

#define LP_MAX_IMAGE 8

/*----------------------------------------------------------------------------*/

struct image
{
    GLuint texture;
    int    w;
    int    h;
    int    flags;
    float  values[LP_MAX_VALUE];
};

typedef struct image image;

struct lightprobe
{
    GLuint  pro_circle;
    GLuint  pro_sphere;

    GLuint  vbo_sphere;
    GLuint  ebo_sphere;

    GLsizei num_sphere;

    image images[LP_MAX_IMAGE];
};

/*----------------------------------------------------------------------------*/
/* Determine the proper OpenGL interal format, external format, and data type */
/* for an image with c channels and b bits per channel.  Punt to c=4 b=8.     */

static GLenum internal_form(int b, int c)
{
    if      (b == 32)
    {
        if      (c == 1) return GL_R32F;
        else if (c == 2) return GL_RG32F;
        else if (c == 3) return GL_RGB32F;
        else             return GL_RGBA32F;
    }
    else if (b == 16)
    {
        if      (c == 1) return GL_LUMINANCE16;
        else if (c == 2) return GL_LUMINANCE16_ALPHA16;
        else if (c == 3) return GL_RGB16;
        else             return GL_RGBA16;
    }
    else
    {
        if      (c == 1) return GL_LUMINANCE;
        else if (c == 2) return GL_LUMINANCE_ALPHA;
        else if (c == 3) return GL_RGB;
        else             return GL_RGBA;
    }
}

static GLenum external_form(int c)
{
    if      (c == 1) return GL_LUMINANCE;
    else if (c == 2) return GL_LUMINANCE_ALPHA;
    else if (c == 3) return GL_RGB;
    else             return GL_RGBA;
}

static GLenum external_type(int b)
{
    if      (b == 32) return GL_FLOAT;
    else if (b == 16) return GL_UNSIGNED_SHORT;
    else              return GL_UNSIGNED_BYTE;
}

/*----------------------------------------------------------------------------*/

/* Load the contents of a TIFF image to a newly-allocated buffer.  Return the */
/* buffer and its configuration.                                              */

static void *tifread(const char *path, int *w, int *h, int *c, int *b)
{
    TIFF *T = 0;
    void *p = 0;

    TIFFSetWarningHandler(0);
    
    if ((T = TIFFOpen(path, "r")))
    {
        uint32 i, s = (uint32) TIFFScanlineSize(T);
        uint32 W, H;
        uint16 B, C;

        TIFFGetField(T, TIFFTAG_IMAGEWIDTH,      &W);
        TIFFGetField(T, TIFFTAG_IMAGELENGTH,     &H);
        TIFFGetField(T, TIFFTAG_BITSPERSAMPLE,   &B);
        TIFFGetField(T, TIFFTAG_SAMPLESPERPIXEL, &C);

        if ((p = malloc(H * s)))
        {         
            for (i = 0; i < H; ++i)
                TIFFReadScanline(T, (uint8 *) p + i * s, i, 0);

            *w = (int) W;
            *h = (int) H;
            *b = (int) B;
            *c = (int) C;
        }
        TIFFClose(T);
    }
    return p;
}

/* Write the contents of a buffer to a 4-channel 32-bit floating point TIFF   */
/* image.                                                                     */
#if 0
static void tifwrite(const char *path, int w, int h, void *p)
{
    TIFF *T = 0;
    
    TIFFSetWarningHandler(0);

    if ((T = TIFFOpen(path, "w")))
    {
        uint32 i, s;
        
        TIFFSetField(T, TIFFTAG_IMAGEWIDTH,      w);
        TIFFSetField(T, TIFFTAG_IMAGELENGTH,     h);
        TIFFSetField(T, TIFFTAG_BITSPERSAMPLE,  32);
        TIFFSetField(T, TIFFTAG_SAMPLESPERPIXEL, 4);
        
        TIFFSetField(T, TIFFTAG_PHOTOMETRIC,  PHOTOMETRIC_RGB);
        TIFFSetField(T, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_IEEEFP);
        TIFFSetField(T, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
/*      TIFFSetField(T, TIFFTAG_ICCPROFILE,   sizeof (sRGB), sRGB); */

        s = (uint32) TIFFScanlineSize(T);

        for (i = 0; i < h; ++i)
            TIFFWriteScanline(T, (uint8 *) p + i * s, i, 0);

        TIFFClose(T);
    }
}
#endif

/* Load the named TIFF image into a 32-bit floating point OpenGL rectangular  */
/* texture. Release the image buffer after loading, and return the texture    */
/* object.                                                                    */

static GLuint load_texture(const char *path, int *w, int *h)
{
    const GLenum T = GL_TEXTURE_RECTANGLE_ARB;

    GLuint o = 0;
    void  *p = 0;

    int c;
    int b;

    if ((p = tifread(path, w, h, &c, &b)))
    {
        GLenum i = internal_form(b, c);
        GLenum e = external_form(c);
        GLenum t = external_type(b);

        glGenTextures(1, &o);
        glBindTexture(T,  o);

        glTexImage2D(T, 0, i, *w, *h, 0, e, t, p);

        glTexParameteri(T, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(T, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(T, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
        glTexParameteri(T, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);

        free(p);
    }
    return o;
}

/*----------------------------------------------------------------------------*/

static char *load_txt(const char *name)
{
    /* Load the named file into a newly-allocated buffer. */

    FILE *fp = 0;
    void  *p = 0;
    size_t n = 0;

    if ((fp = fopen(name, "rb")))
    {
        if (fseek(fp, 0, SEEK_END) == 0)
        {
            if ((n = (size_t) ftell(fp)))
            {
                if (fseek(fp, 0, SEEK_SET) == 0)
                {
                    if ((p = calloc(n + 1, 1)))
                    {
                        fread(p, 1, n, fp);
                    }
                }
            }
        }
        fclose(fp);
    }
    return p;
}

/*----------------------------------------------------------------------------*/

static int check_shader_log(GLuint shader, const char *path)
{
    GLchar *p = 0;
    GLint   s = 0;
    GLint   n = 0;

    /* Check the shader compile status.  If failed, print the log. */

    glGetShaderiv(shader, GL_COMPILE_STATUS,  &s);
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &n);

    if (s == 0)
    {
        if ((p = (GLchar *) calloc(n + 1, 1)))
        {
            glGetShaderInfoLog(shader, n, NULL, p);

            fprintf(stderr, "%s: OpenGL %s Shader Error:\n%s", path,
                   (shader == GL_VERTEX_SHADER) ? "Vertex" : "Fragment", p);
            free(p);
        }
        return 0;
    }
    return 1;
}

static int check_program_log(GLuint program)
{
    GLchar *p = 0;
    GLint   s = 0;
    GLint   n = 0;

    /* Check the program link status.  If failed, print the log. */

    glGetProgramiv(program, GL_LINK_STATUS,     &s);
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &n);

    if (s == 0)
    {
        if ((p = (GLchar *) calloc(n + 1, 1)))
        {
            glGetProgramInfoLog(program, n, NULL, p);

            fprintf(stderr, "OpenGL Program Error:\n%s", p);
            free(p);
        }
        return 0;
    }
    return 1;
}

/*----------------------------------------------------------------------------*/

/* Prepare a GLSL shader, taking it from source coude to usable GL object.    */
/* Receive the shader's type and source path.  Load and compile it.  Report   */
/* any errors in the log, and return 0 on failure.                            */

static GLuint load_shader(GLenum type, const char *path)
{
    /* Load the named shader source file. */

    GLchar *text;

    if ((text = load_txt(path)))
    {
        /* Compile a new shader with the given source. */

        GLuint shader = glCreateShader(type);

        glShaderSource (shader, 1, (const GLchar **) &text, NULL);
        glCompileShader(shader);

        free(text);

        /* If the shader is valid, return it.  Else, delete it. */

        if (check_shader_log(shader, path))
            return shader;
        else
            glDeleteShader(shader);
    }
    return 0;
}

/* Prepare a GLSL program, taking it all the way from source code to usable   */
/* GL object.  Receive the path names of vertex and fragment program source   */
/* files.  Read, compile, and link these into a GLSL program object, checking */
/* logs and reporting any errors.  Clean up and return 0 on failure.          */

static GLuint load_program(const char *path_vert, const char *path_frag)
{
    /* Load the shaders. */

    GLuint shader_vert = load_shader(GL_VERTEX_SHADER,   path_vert);
    GLuint shader_frag = load_shader(GL_FRAGMENT_SHADER, path_frag);

    /* Link them to a new program object. */

    if (shader_vert && shader_frag)
    {
        GLuint program = glCreateProgram();

        glAttachShader(program, shader_vert);
        glAttachShader(program, shader_frag);

        glLinkProgram(program);

        /* If the program is valid, return it.  Else, delete it. */

        if (check_program_log(program))
            return program;
        else
            glDeleteProgram(program);
    }

    /* Fail. */

    glDeleteShader(shader_frag);
    glDeleteShader(shader_vert);

    return 0;
}

/* Delete the given GLSL program object.  Delete any shaders attached to it.  */

static void free_program(GLuint program)
{
    GLuint  shaders[2];
    GLsizei count;

    glGetAttachedShaders(program, 2, &count, shaders);

    if (count > 0) glDeleteShader(shaders[0]);
    if (count > 1) glDeleteShader(shaders[1]);

    glDeleteProgram(program);
}

/*----------------------------------------------------------------------------*/

static GLsizei make_sphere(GLuint vbo, GLuint ebo)
{
    const GLsizei r = 16;
    const GLsizei c = 32;

    const GLsizei vsize = 3 * sizeof (GLfloat) * (r + 1) * (c + 1);
    const GLsizei esize = 4 * sizeof (GLshort) * (r    ) * (c    );

    GLfloat *v = 0;
    GLshort *e = 0;

    glBindBuffer(GL_ARRAY_BUFFER,         vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);

    glBufferData(GL_ARRAY_BUFFER,         vsize, 0, GL_STATIC_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, esize, 0, GL_STATIC_DRAW);

    v = glMapBuffer(GL_ARRAY_BUFFER,         GL_WRITE_ONLY);
    e = glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);

    if (v && e)
    {
        GLsizei i;
        GLsizei j;
        GLsizei k;

        /* Compute the vertex positions. */

        for (k = 0, i = 0; i <= r; i++)
            for    (j = 0; j <= c; j++, k++)
            {
                const double p = (      M_PI * i) / r - M_PI_2;
                const double t = (2.0 * M_PI * j) / c - M_PI;

                v[k * 3 + 0] = (GLfloat) (sin(t) * cos(p));
                v[k * 3 + 1] = (GLfloat) (         sin(p));
                v[k * 3 + 2] = (GLfloat) (cos(t) * cos(p));
            }

        /* Compute the face indices. */

        for (k = 0, i = 0; i <  r; i++)
            for    (j = 0; j <  c; j++, k++)
            {
                e[k * 4 + 0] = (GLushort) ((i + 0) * (c + 1) + (j + 0));
                e[k * 4 + 1] = (GLushort) ((i + 0) * (c + 1) + (j + 1));
                e[k * 4 + 2] = (GLushort) ((i + 1) * (c + 1) + (j + 1));
                e[k * 4 + 3] = (GLushort) ((i + 1) * (c + 1) + (j + 0));
            }
    }

    glUnmapBuffer(GL_ARRAY_BUFFER);
    glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);

    return r * c * 4;
}

static void gl_init(lightprobe *L)
{
    L->pro_circle = load_program(CIRCLE_VERT, CIRCLE_FRAG);
    L->pro_sphere = load_program(SPHERE_VERT, SPHERE_FRAG);

    glGenBuffers(1, &L->vbo_sphere);
    glGenBuffers(1, &L->ebo_sphere);

    L->num_sphere = make_sphere(L->vbo_sphere, L->ebo_sphere);
}

static void gl_free(lightprobe *L)
{
    free_program(L->pro_circle);
    free_program(L->pro_sphere);

    glDeleteBuffers(1, &L->vbo_sphere);
    glDeleteBuffers(1, &L->ebo_sphere);
}

/*----------------------------------------------------------------------------*/
/* Allocate and initialize a new, empty lightprobe object. Initialize all GL  */
/* state needed to operate upon the input and render the output.              */
/*                                                                            */
/* GLEW initialization has to go somewhere, and it might as well go here. It  */
/* would be ugly to require the user to call it, and it shouldn't hurt to do  */
/* it multiple times.                                                         */

lightprobe *lp_init()
{
    lightprobe *L = 0;

    glewInit();

    if ((L = (lightprobe *) calloc (1, sizeof (lightprobe))))
        gl_init(L);

    return L;
}

/* Bounce the GLSL state of the given lightprobe. This is useful during GLSL  */
/* development                                                                */

void lp_tilt(lightprobe *L)
{
    gl_free(L);
    gl_init(L);
}

/* Release a lightprobe object and all state associated with it.  Unload any  */
/* images and delete all OpenGL state.                                        */

void lp_free(lightprobe *L)
{
    int i;

    assert(L);

    for (i = 0; i < LP_MAX_IMAGE; i++)
        lp_del_image(L, i);

    gl_free(L);
    free(L);
}

/*----------------------------------------------------------------------------*/

int lp_export_cube(lightprobe *L, const char *path)
{
    printf("Export Cube Map %s\n", path);
    return 1;
}

int lp_export_dome(lightprobe *L, const char *path)
{
    printf("Export Dome Map %s\n", path);
    return 1;
}

int lp_export_sphere(lightprobe *L, const char *path)
{
    printf("Export Sphere Map %s\n", path);
    return 1;
}

/*----------------------------------------------------------------------------*/

int lp_add_image(lightprobe *L, const char *path)
{
    GLuint o = 0;
    int    i = 0;
    int    w = 0;
    int    h = 0;

    assert(L);
    assert(path);

    /* Load the texture. */

    if ((o = load_texture(path, &w, &h)))
    {
        /* Find the lightprobe's first unused image slot. */

        for (i = 0; i < LP_MAX_IMAGE; i++)
            if (L->images[i].flags == 0)
            {
                /* Store the texture. */

                L->images[i].texture = o;
                L->images[i].w       = w;
                L->images[i].h       = h;
                L->images[i].flags   = LP_FLAG_LOADED;

                /* Set some default values. */

                L->images[i].values[LP_CIRCLE_X]      = w / 2;
                L->images[i].values[LP_CIRCLE_Y]      = h / 2;
                L->images[i].values[LP_CIRCLE_RADIUS] = h / 3;

                /* Succeed. */

                return i;
            }

        /* Fail. */

        glDeleteTextures(1, &o);
    }
    return -1;
}

void lp_del_image(lightprobe *L, int i)
{
    assert(L);
    assert(0 <= i && i < LP_MAX_IMAGE);

    /* Release the image. */

    if (L->images[i].flags & LP_FLAG_LOADED)
    {
        glDeleteTextures(1, &L->images[i].texture);
        memset(L->images + i, 0, sizeof (image));
    }
}

/*----------------------------------------------------------------------------*/

int lp_get_image_width(lightprobe *L, int i)
{
    assert(L);
    assert(0 <= i && i < LP_MAX_IMAGE);
    return L->images[i].w;
}

int lp_get_image_height(lightprobe *L, int i)
{
    assert(L);
    assert(0 <= i && i < LP_MAX_IMAGE);
    return L->images[i].h;
}

/*----------------------------------------------------------------------------*/

/* The image bit field represents various togglable options. The following    */
/* functions provide bit-wise operations for the manipulation of these.       */

void lp_set_image_flags(lightprobe *L, int i, int f)
{
    assert(L);
    assert(0 <= i && i < LP_MAX_IMAGE);
    L->images[i].flags |=  f;
}

void lp_clr_image_flags(lightprobe *L, int i, int f)
{
    assert(L);
    assert(0 <= i && i < LP_MAX_IMAGE);
    L->images[i].flags &= ~f;
}

int  lp_get_image_flags(lightprobe *L, int i)
{
    assert(L);
    assert(0 <= i && i < LP_MAX_IMAGE);
    return L->images[i].flags;
}

/*----------------------------------------------------------------------------*/

void  lp_set_image_value(lightprobe *L, int i, int k, float v)
{
    assert(L);
    assert(0 <= i && i < LP_MAX_IMAGE);
    assert(0 <= k && k < LP_MAX_VALUE);
    L->images[i].values[k] = v;
}

float lp_get_image_value(lightprobe *L, int i, int k)
{
    assert(L);
    assert(0 <= i && i < LP_MAX_IMAGE);
    assert(0 <= k && k < LP_MAX_VALUE);
    return L->images[i].values[k];
}

/*----------------------------------------------------------------------------*/

/* Set up circle rendering to a viewport of width W and height H. Set the     */
/* uniform values for exposure E.                                             */

static void render_circle_setup(lightprobe *L, int w, int h, float e)
{
    /* Configure and clear the viewport. */

    glViewport(0, 0, w, h);

    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT |
            GL_DEPTH_BUFFER_BIT);

    /* Load the matrices. */

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, w, h, 0, 0, 1);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    /* Ready the program. */

    glUseProgram(L->pro_circle);

    UNIFORM1I(L->pro_circle, "image",    0);
    UNIFORM1F(L->pro_circle, "exposure", e);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
}

/* Render the image at index i scaled to zoom level Z, translated to panning  */
/* position X Y.                                                              */

static void render_circle_image(image *c, GLuint p, int w, int h,
                                float x, float y, float z)
{
    int X = -(c->w * z - w) * x;
    int Y = -(c->h * z - h) * y;

    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, c->texture);

    UNIFORM1F(p, "circle_r", c->values[LP_CIRCLE_RADIUS]);
    UNIFORM2F(p, "circle_p", c->values[LP_CIRCLE_X],
                             c->values[LP_CIRCLE_Y]);

    glPushMatrix();
    {
        glTranslated(X, Y, 0.0);
        glScaled    (z, z, 1.0);

        glBegin(GL_QUADS);
        {
            glVertex2i(0,    0);
            glVertex2i(c->w, 0);
            glVertex2i(c->w, c->h);
            glVertex2i(0,    c->h);
        }
        glEnd();
    }
    glPopMatrix();
}

/* Render all loaded, visible images to a viewport with width W and height H. */
/* F gives rendering option flags. X and Y give normalized pan positions.     */
/* E is exposure, and Z is zoom.                                              */

void lp_render_circle(lightprobe *L, int f, int w, int h,
                      float x, float y, float e, float z)
{
    int i;

    assert(L);

    render_circle_setup(L, w, h, e);

    for (i = 0; i < LP_MAX_IMAGE; i++)
        if ((L->images[i].flags & LP_FLAG_LOADED) != 0 &&
            (L->images[i].flags & LP_FLAG_HIDDEN) == 0)
            render_circle_image(L->images + i, L->pro_circle, w, h, x, y, z);
}

/*----------------------------------------------------------------------------*/

static void render_sphere_setup(lightprobe *L, int w, int h,
                                float x, float y, float e, float z)
{
    const GLdouble H = 0.5 * ((GLdouble) w / (GLdouble) h) / z;
    const GLdouble V = 0.5 * (                        1.0) / z;

    /* Configure and clear the viewport. */

    glViewport(0, 0, w, h);

    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT |
            GL_DEPTH_BUFFER_BIT);

    /* Load the matrices. */

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-H, +H, -V, +V, 1, 10);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslated(0.0, 0.0, -2.0);
    glRotated(360.0 * x - 180.0, 0.0, 1.0, 0.0);
    glRotated(180.0 * y -  90.0, 1.0, 0.0, 0.0);

    /* Bind the array buffers. */

    glBindBuffer(GL_ARRAY_BUFFER,         L->vbo_sphere);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, L->ebo_sphere);
    
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, 0);

    /* Ready the program. */

    glUseProgram(L->pro_sphere);

    UNIFORM1I(L->pro_sphere, "image",    0);
    UNIFORM1F(L->pro_sphere, "exposure", e);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

static void render_sphere_image(image *c, GLuint p, GLsizei n)
{
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, c->texture);

    UNIFORM1F(p, "sphere_e", c->values[LP_SPHERE_ELEVATION]);
    UNIFORM1F(p, "sphere_a", c->values[LP_SPHERE_AZIMUTH]);
    UNIFORM1F(p, "sphere_r", c->values[LP_SPHERE_ROLL]);
    UNIFORM1F(p, "circle_r", c->values[LP_CIRCLE_RADIUS]);
    UNIFORM2F(p, "circle_p", c->values[LP_CIRCLE_X],
                             c->values[LP_CIRCLE_Y]);

    glDrawElements(GL_QUADS, n, GL_UNSIGNED_SHORT, 0);
}

void lp_render_sphere(lightprobe *L, int f, int w, int h,
                      float x, float y, float e, float z)
{
    int i;

    assert(L);

    render_sphere_setup(L, w, h, x, y, e, z);

    for (i = 0; i < LP_MAX_IMAGE; i++)
        if ((L->images[i].flags & LP_FLAG_LOADED) != 0 &&
            (L->images[i].flags & LP_FLAG_HIDDEN) == 0)
            render_sphere_image(L->images + i, L->pro_sphere, L->num_sphere);
}

/*----------------------------------------------------------------------------*/
