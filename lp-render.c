// LIGHTPROBE Copyright (C) 2010 Robert Kooima
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

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tiffio.h>
#include <GL/glew.h>

#include "lp-render.h"
#include "gl-sync.h"
#include "gl-sphere.h"
#include "gl-program.h"
#include "gl-framebuffer.h"

//------------------------------------------------------------------------------

#define LP_MAX_IMAGE 8

#define SPHERE_R 32
#define SPHERE_C 64

//------------------------------------------------------------------------------

struct image
{
    GLuint texture;
    int    w;
    int    h;
    float  values[LP_MAX_VALUE];
};

typedef struct image image;

//------------------------------------------------------------------------------

struct lightprobe
{
    // OpenGL support.

    gl_framebuffer acc;
    gl_program     circle;
    gl_program     sblend;
    gl_program     sfinal;
    gl_sphere      sphere;

    GLuint colormap;

    // Source images.

    image images[LP_MAX_IMAGE];
    int   select;
};

//------------------------------------------------------------------------------
// Determine the proper OpenGL interal format, external format, and data type
// for an image with c channels and b bits per channel.  Punt to c=4 b=8.

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

//------------------------------------------------------------------------------

#include "srgb.h"

// Write the contents of an array buffers to a multi-page 32-bit floating point
// TIFF image.

static void tifwriten(const char *path, int w, int h, int c, int n, void **p)
{
    TIFF *T = 0;
    
    TIFFSetWarningHandler(0);

    if ((T = TIFFOpen(path, "w")))
    {
        uint32 k, i, s;
        
        for (k = 0; k < n; ++k)
        {
            TIFFSetField(T, TIFFTAG_IMAGEWIDTH,      w);
            TIFFSetField(T, TIFFTAG_IMAGELENGTH,     h);
            TIFFSetField(T, TIFFTAG_BITSPERSAMPLE,  32);
            TIFFSetField(T, TIFFTAG_SAMPLESPERPIXEL, c);
        
            TIFFSetField(T, TIFFTAG_PHOTOMETRIC,  PHOTOMETRIC_RGB);
            TIFFSetField(T, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_IEEEFP);
            TIFFSetField(T, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
            TIFFSetField(T, TIFFTAG_ICCPROFILE,   sRGB_icc_len, sRGB_icc);

            s = (uint32) TIFFScanlineSize(T);

            for (i = 0; i < h; ++i)
                TIFFWriteScanline(T, (uint8 *) p[k] + (h - i - 1) * s, i, 0);

            TIFFWriteDirectory(T);
        }
        TIFFClose(T);
    }
}

// Write the contents of a buffer to a 32-bit floating point TIFF image.

static void tifwrite(const char *path, int w, int h, int c, void *p)
{
    TIFF *T = 0;
    
    TIFFSetWarningHandler(0);

    if ((T = TIFFOpen(path, "w")))
    {
        uint32 i, s;
        
        TIFFSetField(T, TIFFTAG_IMAGEWIDTH,      w);
        TIFFSetField(T, TIFFTAG_IMAGELENGTH,     h);
        TIFFSetField(T, TIFFTAG_BITSPERSAMPLE,  32);
        TIFFSetField(T, TIFFTAG_SAMPLESPERPIXEL, c);
        
        TIFFSetField(T, TIFFTAG_PHOTOMETRIC,  PHOTOMETRIC_RGB);
        TIFFSetField(T, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_IEEEFP);
        TIFFSetField(T, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
        TIFFSetField(T, TIFFTAG_ICCPROFILE,   sRGB_icc_len, sRGB_icc);

        s = (uint32) TIFFScanlineSize(T);

        for (i = 0; i < h; ++i)
            TIFFWriteScanline(T, (uint8 *) p + (h - i - 1) * s, i, 0);

        TIFFClose(T);
    }
}

// Load the contents of a TIFF image to a newly-allocated buffer. Return the
// buffer and its configuration.

static void *tifread(const char *path, int n, int *w, int *h, int *c, int *b)
{
    TIFF *T = 0;
    void *p = 0;

    TIFFSetWarningHandler(0);
    
    if ((T = TIFFOpen(path, "r")))
    {
        if ((n == 0) || TIFFSetDirectory(T, n))
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
        }
        TIFFClose(T);
    }
    return p;
}

// Load the named TIFF image into a 32-bit floating point OpenGL rectangular
// texture. Release the image buffer after loading, and return the texture
// object.

unsigned int lp_load_texture(const char *path, int *w, int *h)
{
    const GLenum T = GL_TEXTURE_RECTANGLE_ARB;

    GLuint o = 0;
    void  *p = 0;

    int c;
    int b;

    if ((p = tifread(path, 0, w, h, &c, &b)))
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
    return (unsigned int) o;
}

static GLuint gl_init_colormap(void)
{
    static const GLubyte p[8][3] = {
        { 0x00, 0x00, 0x00 }, //  0 K
        { 0x00, 0x00, 0xFF }, //  1 B
        { 0xFF, 0x00, 0x00 }, //  2 R
        { 0xFF, 0x00, 0xFF }, //  3 M
        { 0x00, 0xFF, 0x00 }, //  4 G
        { 0x00, 0xFF, 0xFF }, //  5 C
        { 0xFF, 0xFF, 0x00 }, //  6 Y
        { 0xFF, 0xFF, 0xFF }, //  7 W
    };
    GLenum T = GL_TEXTURE_1D;
    GLuint o;

    glGenTextures(1,&o);
    glBindTexture(T, o);
    glTexImage1D (T, 0, GL_RGB, 8, 0, GL_RGB, GL_UNSIGNED_BYTE, p);

    glTexParameteri(T, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(T, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(T, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);

    return o;
}

static void gl_free_colormap(GLuint o)
{
    glDeleteTextures(1, &o);
}

//------------------------------------------------------------------------------

#include "lp-circle-vs.h"
#include "lp-circle-fs.h"
#include "lp-sphere-vs.h"
#include "lp-sblend-fs.h"
#include "lp-sfinal-fs.h"

static void gl_init(lightprobe *L)
{
    gl_init_framebuffer(&L->acc, 0, 0);

    gl_init_program(&L->circle, lp_circle_vs_glsl, lp_circle_vs_glsl_len,
                                lp_circle_fs_glsl, lp_circle_fs_glsl_len);
    gl_init_program(&L->sblend, lp_sphere_vs_glsl, lp_sphere_vs_glsl_len,
                                lp_sblend_fs_glsl, lp_sblend_fs_glsl_len);
    gl_init_program(&L->sfinal, lp_sphere_vs_glsl, lp_sphere_vs_glsl_len,
                                lp_sfinal_fs_glsl, lp_sfinal_fs_glsl_len);

    gl_init_sphere(&L->sphere, SPHERE_R, SPHERE_C);

    L->colormap = gl_init_colormap();
}

static void gl_free(lightprobe *L)
{
    gl_free_colormap(L->colormap);

    gl_free_sphere(&L->sphere);

    gl_free_program(&L->sfinal);
    gl_free_program(&L->sblend);
    gl_free_program(&L->circle);

    gl_free_framebuffer(&L->acc);
}

//------------------------------------------------------------------------------
// Allocate and initialize a new, empty lightprobe object. Initialize all GL
// state needed to operate upon the input and render the output.
//
// GLEW initialization has to go somewhere, and it might as well go here. It
// would be ugly to require the user to call it, and it shouldn't hurt to do it
// multiple times.

lightprobe *lp_init()
{
    lightprobe *L = 0;

    glewInit();
    sync(1);

    if ((L = (lightprobe *) calloc (1, sizeof (lightprobe))))
        gl_init(L);

    return L;
}

// Bounce the GLSL state of the given lightprobe. This is useful during GLSL
// development

void lp_tilt(lightprobe *L)
{
    gl_free(L);
    gl_init(L);
}

// Release a lightprobe object and all state associated with it. Unload any
// images and delete all OpenGL state.

void lp_free(lightprobe *L)
{
    int i;

    assert(L);

    for (i = 0; i < LP_MAX_IMAGE; i++)
        lp_del_image(L, i);

    gl_free(L);
    free(L);
}

//------------------------------------------------------------------------------

int lp_add_image(lightprobe *L, const char *path)
{
    GLuint o = 0;
    int    i = 0;
    int    w = 0;
    int    h = 0;

    assert(L);
    assert(path);

    // Load the texture.

    if ((o = lp_load_texture(path, &w, &h)))
    {
        // Find the lightprobe's first unused image slot.

        for (i = 0; i < LP_MAX_IMAGE; i++)
            if (L->images[i].texture == 0)
            {
                // Store the texture.

                L->images[i].texture = o;
                L->images[i].w       = w;
                L->images[i].h       = h;

                // Set some default values.

                L->images[i].values[LP_CIRCLE_X]      = w / 2;
                L->images[i].values[LP_CIRCLE_Y]      = h / 2;
                L->images[i].values[LP_CIRCLE_RADIUS] = h / 3;

                // Succeed.

                return i;
            }

        // Fail.

        glDeleteTextures(1, &o);
    }
    return -1;
}

void lp_del_image(lightprobe *L, int i)
{
    int j;

    assert(L);
    assert(0 <= i && i < LP_MAX_IMAGE);

    // Release the image.

    if (L->images[i].texture)
    {
        glDeleteTextures(1, &L->images[i].texture);
        memset(L->images + i, 0, sizeof (image));
    }

    // Select another image, if possible.

    if (L->select == i)
        for (j = 0; j < LP_MAX_IMAGE; j++)
            lp_sel_image(L, j);
}

void lp_sel_image(lightprobe *L, int i)
{
    assert(L);
    assert(0 <= i && i < LP_MAX_IMAGE);

    if (L->images[i].texture)
        L->select = i;
}

//------------------------------------------------------------------------------

int lp_get_width(lightprobe *L)
{
    assert(L);
    return (L->images[L->select].texture) ? L->images[L->select].w : 0;
}

int lp_get_height(lightprobe *L)
{
    assert(L);
    return (L->images[L->select].texture) ? L->images[L->select].h : 0;
}

float lp_get_value(lightprobe *L, int k)
{
    assert(L);
    assert(0 <= k && k < LP_MAX_VALUE);
    return (L->images[L->select].texture) ? L->images[L->select].values[k] : 0;
}

void  lp_set_value(lightprobe *L, int k, float v)
{
    assert(L);
    assert(0 <= k && k < LP_MAX_VALUE);
    if (L->images[L->select].texture)
        L->images[L->select].values[k] = v;
}

//------------------------------------------------------------------------------

// Set up circle rendering to a viewport of width W and height H. Set the
// uniform values for exposure E.

static void draw_circle_setup(lightprobe *L, int w, int h, float e)
{
    // Configure and clear the viewport.

    glViewport(0, 0, w, h);

    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT |
            GL_DEPTH_BUFFER_BIT);

    // Load the matrices.

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, w, h, 0, 0, 1);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Ready the program.

    glUseProgram(L->circle.program);

    gl_uniform1i(&L->circle, "image",  0);
    gl_uniform1f(&L->circle, "expo_n", e);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
}

// Render the image at index i scaled to zoom level Z, translated to panning
// position X Y.

static void draw_circle_image(lightprobe *L, image *c,
                              int w, int h, float x, float y, float z)
{
    int X = -(c->w * z - w) * x;
    int Y = -(c->h * z - h) * y;

    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, c->texture);

    gl_uniform1f(&L->circle, "circle_r", c->values[LP_CIRCLE_RADIUS]);
    gl_uniform2f(&L->circle, "circle_p", c->values[LP_CIRCLE_X],
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

// Render all loaded, visible images to a viewport with width W and height H.
// F gives rendering option flags. X and Y give normalized pan positions.
// E is exposure, and Z is zoom.

void lp_render_circle(lightprobe *L, int f, int w, int h,
                      float x, float y, float e, float z)
{
    assert(L);

    if (L->images[L->select].texture)
    {
        gl_size_framebuffer(&L->acc, w, h);

        draw_circle_setup(L, w, h, e);
        draw_circle_image(L, L->images + L->select, w, h, x, y, z);
    }
}

//------------------------------------------------------------------------------

static void transform_globe(GLsizei w, GLsizei h,
                            GLfloat x, GLfloat y, GLfloat z)
{
    const GLdouble H = 0.1 * w / h / z;
    const GLdouble V = 0.1         / z;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-H, +H, -V, +V, 0.1, 5.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glRotated(180 * y -  90, 1, 0, 0);
    glRotated(360 * x - 180, 0, 1, 0);
}

static void transform_chart(void)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho( 0, 1,  0, 1, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

static void transform_polar(void)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1, 1, -1, 1, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

static void transform_cube(GLint i)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-0.5, +0.5, -0.5, +0.5, 0.5, 5.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glScaled(-1.0, 1.0, 1.0);

    switch (i)
    {
    case 0: glRotated(+90, 0, 1, 0);                          break;
    case 1: glRotated(-90, 0, 1, 0);                          break;
    case 2: glRotated(-90, 1, 0, 0); glRotated(180, 0, 1, 0); break;
    case 3: glRotated(+90, 1, 0, 0); glRotated(180, 0, 1, 0); break;
    case 4: glRotated(180, 0, 1, 0);                          break;
    }
}

//------------------------------------------------------------------------------

// Render the given lightprobe image to the accumulation buffer with a blend
// function of one-one. Use the image's unwrapped per-pixel quality as the alpha
// value, and write pre-multiplied color. The result is a weighted sum of
// images, with the total weight in the alpha channel.

static void draw_sblend(lightprobe *L, image *I, int m)
{
    glUseProgram(L->sblend.program);

    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, I->texture);

    gl_uniform1i(&L->sblend, "image",    0);
    gl_uniform1i(&L->sblend, "colormap", 1);
    gl_uniform1f(&L->sblend, "circle_r", I->values[LP_CIRCLE_RADIUS]);
    gl_uniform2f(&L->sblend, "circle_p", I->values[LP_CIRCLE_X],
                                         I->values[LP_CIRCLE_Y]);
    glMatrixMode(GL_TEXTURE);
    {
        glLoadIdentity();
        glRotatef(I->values[LP_SPHERE_ROLL],       0.0f, 0.0f, 1.0f);
        glRotatef(I->values[LP_SPHERE_ELEVATION], -1.0f, 0.0f, 0.0f);
        glRotatef(I->values[LP_SPHERE_AZIMUTH],    0.0f, 1.0f, 0.0f);
    }
    glMatrixMode(GL_MODELVIEW);

    glBlendFunc(GL_ONE, GL_ONE);
    gl_fill_sphere(&L->sphere, m);
}

// Render the accumulation buffer to the output buffer. Divide the RGB color by
// its alpha value, normalizing the weighted sum that resulted from the
// accumulation of images previously, and giving the final quality-blended blend
// of inputs. If we're rendering to the screen, then apply an exposure mapping.

static void draw_sfinal(lightprobe *L, int m, GLfloat e)
{
    glUseProgram(L->sfinal.program);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_1D, L->colormap);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, L->acc.color);

    gl_uniform1i(&L->sfinal, "image",    0);
    gl_uniform1i(&L->sfinal, "colormap", 1);
    gl_uniform1f(&L->sfinal, "expo_n",   e);
    gl_uniform1f(&L->sfinal, "expo_k",   e ? 1 : 0);

    glBlendFunc(GL_ONE, GL_ZERO);
    gl_fill_sphere(&L->sphere, m);
}

static void draw_sphere_grid(lightprobe *L, int m)
{
    glUseProgram(0);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);

    glColor4d(0.0, 0.0, 0.0, 0.2);
    glColor4d(1.0, 1.0, 1.0, 0.5);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    {
        glLineWidth(0.5f);
        gl_fill_sphere(&L->sphere, m);
        glLineWidth(2.0f);
        gl_line_sphere(&L->sphere, m);
    }
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glColor4d(1.0, 1.0, 1.0, 1.0);
}

static void draw_sphere(lightprobe *L, gl_framebuffer *F, int f, int m, float e)
{
    glEnable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    // Render any/all images to the accumulation buffer.

    glBindFramebuffer(GL_FRAMEBUFFER, L->acc.frame);
    glClear(GL_COLOR_BUFFER_BIT);

    if (f & LP_RENDER_ALL)
    {
        int i;
        for (i = 0; i < LP_MAX_IMAGE; i++)
            if (L->images[i].texture)
                draw_sblend(L, L->images + i, m);
    }
    else
        draw_sblend(L, L->images + L->select, m);

    // Map the accumulation buffer to the output buffer.

    glBindFramebuffer(GL_FRAMEBUFFER, F ? F->frame : 0);
    glClear(GL_COLOR_BUFFER_BIT);

    draw_sfinal(L, m, e);

    // Draw the grid.

    if (f & LP_RENDER_GRID)
        draw_sphere_grid(L, m);
}

//------------------------------------------------------------------------------

void lp_render_sphere(lightprobe *L, int f, int w, int h,
                      float x, float y, float e, float z)
{
    assert(L);

    transform_polar();
//  transform_chart();
//  transform_globe(w, h, x, y, z);
    
    // Update the program and buffer caches.  Render the sphere.

    gl_size_framebuffer(&L->acc, w, h);
    glViewport(0, 0, w, h);

//  draw_sphere(L, 0, f, GL_SPHERE_GLOBE, e);
//  draw_sphere(L, 0, f, GL_SPHERE_CHART, e);
    draw_sphere(L, 0, f, GL_SPHERE_POLAR, e);
}

//----------------------------------------------------------------------------
/*
static void transform_pos_x(void)
{
    glScaled(-1.0, 1.0, 1.0);
    glRotated(+90.0, 0.0, 1.0, 0.0);
}

static void transform_neg_x(void)
{
    glScaled(-1.0, 1.0, 1.0);
    glRotated(-90.0, 0.0, 1.0, 0.0);
}

static void transform_pos_y(void)
{
    glScaled(-1.0, 1.0, 1.0);
    glRotated(-90.0, 1.0, 0.0, 0.0); 
    glRotated(180.0, 0.0, 1.0, 0.0);
}

static void transform_neg_y(void)
{
    glScaled(-1.0, 1.0, 1.0);
    glRotated(+90.0, 1.0, 0.0, 0.0);
    glRotated(180.0, 0.0, 1.0, 0.0); 
}

static void transform_pos_z(void)
{
    glScaled(-1.0, 1.0, 1.0);
    glRotated(180.0, 0.0, 1.0, 0.0);
}

static void transform_neg_z(void)
{
    glScaled(-1.0, 1.0, 1.0);
}

typedef void (*transform_func)(void);
*/
//------------------------------------------------------------------------------

void lp_export_cube(lightprobe *L, const char *path, int s, int f)
{
/*
    transform_func transform[6] = {
        transform_pos_x,
        transform_neg_x,
        transform_pos_y,
        transform_neg_y,
        transform_pos_z,
        transform_neg_z
    };

    gl_framebuffer export;
    void *pixels[6];
    int i;

    // Update the program and buffer caches.

    gl_size_framebuffer(&L->acc, s, s);
    gl_init_framebuffer(&export, s, s);
    {
        GLfloat d = 0.25;

        glViewport(0, 0, s, s);

        for (i = 0; i < 6; ++i)
        {
            // Apply the view transformation and render the sphere.

            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glFrustum(-d, +d, -d, +d, d, 5.0);

            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();
            transform[i]();

            glDisable(GL_CULL_FACE);
            draw_sphere(L, &export, f, GL_SPHERE_GLOBE, 0);

            // Copy the output to a buffer.

            pixels[i] = gl_copy_framebuffer(&export, f & LP_RENDER_ALPHA);
        }
    }
    gl_free_framebuffer(&export);

    // Write the buffers to a file and release them.

    if (f & LP_RENDER_ALPHA)
        tifwriten(path, s, s, 4, 6, pixels);
    else
        tifwriten(path, s, s, 3, 6, pixels);

    for (i = 0; i < 6; ++i)
        free(pixels[i]);
*/
}
//------------------------------------------------------------------------------

static void lp_export(lightprobe *L,
                      const char *path, int m, int w, int h, int f)
{
    gl_framebuffer export;
    void          *pixels;

    // Initialize the export framebuffer.

    gl_size_framebuffer(&L->acc, w, h);
    gl_init_framebuffer(&export, w, h);
    {
        // Render the sphere.

        glViewport(0, 0, w, h);
        draw_sphere(L, &export, f, m, 0);

        // Copy the output to a buffer and write the buffer to a file.

        if ((pixels = gl_copy_framebuffer(&export, f & LP_RENDER_ALPHA)))
        {
            if (f & LP_RENDER_ALPHA)
                tifwrite(path, w, h, 4, pixels);
            else
                tifwrite(path, w, h, 3, pixels);

            free(pixels);
        }
    }
    gl_free_framebuffer(&export);
}

void lp_export_dome(lightprobe *L, const char *path, int s, int f)
{
    transform_polar();
    lp_export(L, path, GL_SPHERE_POLAR, s, s, f);
}

void lp_export_rect(lightprobe *L, const char *path, int s, int f)
{
    transform_chart();
    lp_export(L, path, GL_SPHERE_CHART, 2 * s, s, f);
}

//------------------------------------------------------------------------------
