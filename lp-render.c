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

    gl_framebuffer tmp;
    gl_framebuffer acc;
    gl_program     circle;
    gl_program     sglobe;
    gl_program     schart;
    gl_program     spolar;
    gl_program     sblend;
    gl_program     sfinal;
    gl_sphere      sphere;

    GLuint colormap;

    // Source images.

    image images[LP_MAX_IMAGE];
    int   select;
};

//------------------------------------------------------------------------------

static double min(double a, double b)
{
    return (a < b) ? a : b;
}

static double max(double a, double b)
{
    return (a > b) ? a : b;
}

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

 /*
static void mkbuf(GLint   i, GLsizei s,
                  GLubyte r, GLubyte g, GLubyte b, GLubyte a)
{
    GLubyte *p;
    GLubyte *q;

    if ((p = q = (GLubyte *) malloc(4 * s * s)))
    {
        while (q < p + 4 * s * s)
        {
            *q++ = r;
            *q++ = g;
            *q++ = b;
            *q++ = a;
        }
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, i, GL_RGBA,
                     s, s, 0, GL_RGBA, GL_UNSIGNED_BYTE, p);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, i, GL_RGBA,
                     s, s, 0, GL_RGBA, GL_UNSIGNED_BYTE, p);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, i, GL_RGBA,
                     s, s, 0, GL_RGBA, GL_UNSIGNED_BYTE, p);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, i, GL_RGBA,
                     s, s, 0, GL_RGBA, GL_UNSIGNED_BYTE, p);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, i, GL_RGBA,
                     s, s, 0, GL_RGBA, GL_UNSIGNED_BYTE, p);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, i, GL_RGBA,
                     s, s, 0, GL_RGBA, GL_UNSIGNED_BYTE, p);
        free(p);
    }
}
 */
  /*
static void mkbuf(GLint   i, GLsizei s,
                  GLubyte r, GLubyte g, GLubyte b, GLubyte a)
{
    GLubyte *p;
    GLubyte *q;

    if ((p = q = (GLubyte *) malloc(4 * s * s)))
    {
        while (q < p + 4 * s * s)
        {
            *q++ = r;
            *q++ = g;
            *q++ = b;
            *q++ = a;
        }
        glTexImage2D(GL_TEXTURE_2D, i, GL_RGBA, s, s, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, p);
        free(p);
    }
}

static GLuint gl_init_colormap(void)
{
//  GLenum T = GL_TEXTURE_CUBE_MAP;
    GLenum T = GL_TEXTURE_2D;
    GLuint o;

    glGenTextures(1,&o);
    glBindTexture(T, o);

    glTexParameteri(T, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(T, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    mkbuf(0, 1 << 7, 0xFF, 0xFF, 0xFF, 0xFF);
    mkbuf(1, 1 << 6, 0xFF, 0xFF, 0x00, 0xDF);
    mkbuf(2, 1 << 5, 0x00, 0xFF, 0xFF, 0xBF);
    mkbuf(3, 1 << 4, 0x00, 0xFF, 0x00, 0x9F);
    mkbuf(4, 1 << 3, 0xFF, 0x00, 0xFF, 0x7F);
    mkbuf(5, 1 << 2, 0xFF, 0x00, 0x00, 0x5F);
    mkbuf(6, 1 << 1, 0x00, 0x00, 0xFF, 0x3F);
    mkbuf(7, 1 << 0, 0x00, 0x00, 0x00, 0x1F);

    return o;
}
  */
static void gl_free_colormap(GLuint o)
{
    glDeleteTextures(1, &o);
}

static void gl_fill_screen(void)
{
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glBegin(GL_QUADS);
    {
        glNormal3d( 0.0,  0.0,  1.0);
        glVertex3d(-1.0, -1.0,  0.0);
        glVertex3d(+1.0, -1.0,  0.0);
        glVertex3d(+1.0, +1.0,  0.0);
        glVertex3d(-1.0, +1.0,  0.0);
    }
    glEnd();

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

//------------------------------------------------------------------------------

#include "lp-circle-vs.h"
#include "lp-circle-fs.h"
#include "lp-sphere-vs.h"
#include "lp-sglobe-fs.h"
#include "lp-schart-fs.h"
#include "lp-spolar-fs.h"
#include "lp-sblend-fs.h"
#include "lp-sfinal-fs.h"

static void gl_init(lightprobe *L)
{
    gl_init_framebuffer(&L->tmp, 0, 0, 2);
    gl_init_framebuffer(&L->acc, 0, 0, 4);

    gl_init_program(&L->circle, lp_circle_vs_glsl, lp_circle_vs_glsl_len,
                                lp_circle_fs_glsl, lp_circle_fs_glsl_len);
    gl_init_program(&L->sglobe, lp_sphere_vs_glsl, lp_sphere_vs_glsl_len,
                                lp_sglobe_fs_glsl, lp_sglobe_fs_glsl_len);
    gl_init_program(&L->schart, lp_sphere_vs_glsl, lp_sphere_vs_glsl_len,
                                lp_schart_fs_glsl, lp_schart_fs_glsl_len);
    gl_init_program(&L->spolar, lp_sphere_vs_glsl, lp_sphere_vs_glsl_len,
                                lp_spolar_fs_glsl, lp_spolar_fs_glsl_len);
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
    gl_free_program(&L->schart);
    gl_free_program(&L->sglobe);
    gl_free_program(&L->circle);

    gl_free_framebuffer(&L->acc);
    gl_free_framebuffer(&L->tmp);
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

static void proj_image(int vx, int vy, int vw, int vh, int ww, int wh)
{
    glOrtho(vx, vx + ww, vy + wh, vy, 0, 1);
}

static void proj_globe(int vx, int vy, int vw, int vh, int ww, int wh)
{
    double z = (double) ww / (double) vw;

    glFrustum(-0.5 * z * ww / wh,
              +0.5 * z * ww / wh,
              -0.5 * z,
              +0.5 * z, 0.25, 10.0);
}

static void proj_polar(int vx, int vy, int vw, int vh, int ww, int wh)
{
    glOrtho(vx, vx + ww, vy + wh, vy, 0, 1);
}

static void proj_chart(int vx, int vy, int vw, int vh, int ww, int wh)
{
    glOrtho(vx, vx + ww, vy + wh, vy, 0, 1);
}

static void proj_cube(void)
{
    glFrustum(+0.5, -0.5, -0.5, +0.5, 0.5, 5.0);
}

static void view_globe(int vx, int vy, int vw, int vh, int ww, int wh)
{
    double x = (double) vx / max(1, vw - ww);
    double y = (double) vy / max(1, vh - wh);

    glRotated(180 * y -  90, 1, 0, 0);
    glRotated(360 * x - 180, 0, 1, 0);
}

static void view_chart(int vx, int vy, int vw, int vh, int ww, int wh)
{
    double k = min(0.5 * vw, vh);

    glScaled(2 * k, k, 1);
}

static void view_polar(int vx, int vy, int vw, int vh, int ww, int wh)
{
    double x = 0.5 * vw;
    double y = 0.5 * vh;
    double k = min(x, y);

    glTranslated(x, y, 0);
    glScaled    (k, k, 1);
}

static void view_cube(int i)
{
    switch (i)
    {
    case 0: glRotated(+90, 0, 1, 0);                          break;
    case 1: glRotated(-90, 0, 1, 0);                          break;
    case 2: glRotated(-90, 1, 0, 0); glRotated(180, 0, 1, 0); break;
    case 3: glRotated(+90, 1, 0, 0); glRotated(180, 0, 1, 0); break;
    case 4: glRotated(180, 0, 1, 0);                          break;
    }
}

static void transform(int f, int vx, int vy, int vw, int vh, int ww, int wh)
{
    glViewport(0, 0, ww, wh);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    if      (f & LP_RENDER_GLOBE) proj_globe(vx, vy, vw, vh, ww, wh);
    else if (f & LP_RENDER_CHART) proj_chart(vx, vy, vw, vh, ww, wh);
    else if (f & LP_RENDER_POLAR) proj_polar(vx, vy, vw, vh, ww, wh);
    else if (f & LP_RENDER_CUBE0) proj_cube();
    else if (f & LP_RENDER_CUBE1) proj_cube();
    else if (f & LP_RENDER_CUBE2) proj_cube();
    else if (f & LP_RENDER_CUBE3) proj_cube();
    else if (f & LP_RENDER_CUBE4) proj_cube();
    else if (f & LP_RENDER_CUBE5) proj_cube();
    else                          proj_image(vx, vy, vw, vh, ww, wh);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    if      (f & LP_RENDER_GLOBE) view_globe(vx, vy, vw, vh, ww, wh);
    else if (f & LP_RENDER_CHART) view_chart(vx, vy, vw, vh, ww, wh);
    else if (f & LP_RENDER_POLAR) view_polar(vx, vy, vw, vh, ww, wh);
    else if (f & LP_RENDER_CUBE0) view_cube(0);
    else if (f & LP_RENDER_CUBE1) view_cube(1);
    else if (f & LP_RENDER_CUBE2) view_cube(2);
    else if (f & LP_RENDER_CUBE3) view_cube(3);
    else if (f & LP_RENDER_CUBE4) view_cube(4);
    else if (f & LP_RENDER_CUBE5) view_cube(5);
}

//------------------------------------------------------------------------------

// Render the given lightprobe image to the accumulation buffer with a blend
// function of one-one. Use the image's unwrapped per-pixel quality as the alpha
// value, and write pre-multiplied color. The result is a weighted sum of
// images, with the total weight in the alpha channel.

static void draw_sblend(lightprobe *L, image *I, int m)
{
    GLuint P;

    // Set up the sphere transform for this image.

    glMatrixMode(GL_TEXTURE);
    {
        glLoadIdentity();
        glRotatef(I->values[LP_SPHERE_ROLL],       0.0f, 0.0f, 1.0f);
        glRotatef(I->values[LP_SPHERE_ELEVATION], -1.0f, 0.0f, 0.0f);
        glRotatef(I->values[LP_SPHERE_AZIMUTH],    0.0f, 1.0f, 0.0f);
    }
    glMatrixMode(GL_MODELVIEW);

    // Render texture coordinates to the temporary buffer.

    glBindFramebuffer(GL_FRAMEBUFFER, L->tmp.frame);

    if      (m == GL_SPHERE_CHART) P = L->schart.program;
    else if (m == GL_SPHERE_POLAR) P = L->spolar.program;
    else                           P = L->sglobe.program;

    glUseProgram(P);
    GLUNIFORM1F(P, "circle_r", I->values[LP_CIRCLE_RADIUS]);
    GLUNIFORM2F(P, "circle_p", I->values[LP_CIRCLE_X],
                               I->values[LP_CIRCLE_Y]);

    glClear(GL_COLOR_BUFFER_BIT);
    glBlendFunc(GL_ONE, GL_ZERO);
    gl_fill_sphere(&L->sphere, m);

    // Blend the image to the accumulation buffer.

    glBindFramebuffer(GL_FRAMEBUFFER, L->acc.frame);

    glUseProgram(L->sblend.program);
    gl_uniform1i(&L->sblend, "image", 0);
    gl_uniform1i(&L->sblend, "coord", 1);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, L->tmp.color);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, I->texture);

    glBlendFunc(GL_ONE, GL_ONE);
    gl_fill_sphere(&L->sphere, m);
}

// Render the accumulation buffer to the output buffer. Divide the RGB color by
// its alpha value, normalizing the weighted sum that resulted from the
// accumulation of images previously, and giving the final quality-blended blend
// of inputs. If we're rendering to the screen, then apply an exposure mapping.

static void draw_sfinal(lightprobe *L, int f, int m, GLfloat e, GLuint frame)
{
    glBindFramebuffer(GL_FRAMEBUFFER, frame);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(L->sfinal.program);
    gl_uniform1i(&L->sfinal, "image",  0);
    gl_uniform1i(&L->sfinal, "color",  1);
    gl_uniform1f(&L->sfinal, "expo_n", e);
    gl_uniform1f(&L->sfinal, "expo_k", (e                ) ? 1.0 : 0.0);
    gl_uniform1f(&L->sfinal, "reso_k", (f & LP_RENDER_RES) ? 1.0 : 0.0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_1D, L->colormap);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, L->acc.color);

    glBlendFunc(GL_ONE, GL_ZERO);
    gl_fill_screen();
}

static void draw_sphere_grid(lightprobe *L, int m)
{
    glUseProgram(0);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);

    glColor4d(0.0, 0.0, 0.0, 0.2);
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

static void draw_sphere(lightprobe *L, int f, float e, GLuint frame)
{
    int m = 0;

    if      (f & LP_RENDER_GLOBE) m = GL_SPHERE_GLOBE;
    else if (f & LP_RENDER_POLAR) m = GL_SPHERE_POLAR;
    else if (f & LP_RENDER_CHART) m = GL_SPHERE_CHART;

    glEnable(GL_BLEND);

    // Initialize the accumulation buffer.

    glBindFramebuffer(GL_FRAMEBUFFER, L->acc.frame);
    glClear(GL_COLOR_BUFFER_BIT);

    // Render any/all images to the accumulation buffer.

    if (f & LP_RENDER_ALL)
    {
        int i;
        for (i = 0; i < LP_MAX_IMAGE; i++)
            if (L->images[i].texture)
                draw_sblend(L, L->images + i, m);
    }
    else draw_sblend(L, L->images + L->select, m);

    // Map the accumulation buffer to the output buffer.

    draw_sfinal(L, f, m, e, frame);

    // Draw the grid.

    if (f & LP_RENDER_GRID)
        draw_sphere_grid(L, m);
}

static void draw_circle(lightprobe *L, int f, int vw, int vh, float e)
{
    const image *I = L->images + L->select;

    if (I->texture)
    {
        const double k = min((double) vw / I->w,
                             (double) vh / I->h);

        glDisable(GL_BLEND);

        glUseProgram(L->circle.program);

        gl_uniform1i(&L->circle, "image",  0);
        gl_uniform1f(&L->circle, "expo_n", e);
        gl_uniform1f(&L->circle, "circle_r", I->values[LP_CIRCLE_RADIUS]);
        gl_uniform2f(&L->circle, "circle_p", I->values[LP_CIRCLE_X],
                                             I->values[LP_CIRCLE_Y]);

        glBindTexture(GL_TEXTURE_RECTANGLE_ARB, I->texture);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClear(GL_COLOR_BUFFER_BIT);

        glPushMatrix();
        {
            glScaled(k, k, 1);
            glBegin(GL_QUADS);
            {
                glVertex2i(0,    0);
                glVertex2i(0,    I->h);
                glVertex2i(I->w, I->h);
                glVertex2i(I->w, 0);
            }
            glEnd();
        }
        glPopMatrix();
    }
}

//------------------------------------------------------------------------------

static void draw(lightprobe *L, int f, int vx, int vy,
                                       int vw, int vh,
                                       int ww, int wh, float e, GLuint frame)
{
    glDisable(GL_DEPTH_TEST);
//  glEnable (GL_CULL_FACE);

    gl_size_framebuffer(&L->tmp, ww, wh, 2);
    gl_size_framebuffer(&L->acc, ww, wh, 4);

    transform(f, vx, vy, vw, vh, ww, wh);

    if (f & 0xF)
        draw_sphere(L, f, e, frame);
    else
        draw_circle(L, f, vw, vh, e);
}

static void export6(lightprobe *L, int f, int s, const char *path)
{
    gl_framebuffer export;
    void *pixels[6];

    // Render each side of the cube map and copy each output to a buffer.

    gl_init_framebuffer(&export, s, s, 3);
    {
        draw(L, f | LP_RENDER_CUBE0, 0, 0, s, s, s, s, 0, export.frame);
        pixels[0] = gl_copy_framebuffer(&export, 3);

        draw(L, f | LP_RENDER_CUBE1, 0, 0, s, s, s, s, 0, export.frame);
        pixels[1] = gl_copy_framebuffer(&export, 3);

        draw(L, f | LP_RENDER_CUBE2, 0, 0, s, s, s, s, 0, export.frame);
        pixels[2] = gl_copy_framebuffer(&export, 3);

        draw(L, f | LP_RENDER_CUBE3, 0, 0, s, s, s, s, 0, export.frame);
        pixels[3] = gl_copy_framebuffer(&export, 3);

        draw(L, f | LP_RENDER_CUBE4, 0, 0, s, s, s, s, 0, export.frame);
        pixels[4] = gl_copy_framebuffer(&export, 3);

        draw(L, f | LP_RENDER_CUBE5, 0, 0, s, s, s, s, 0, export.frame);
        pixels[5] = gl_copy_framebuffer(&export, 3);
    }
    gl_free_framebuffer(&export);

    // Write the buffers to a file and release them.

    tifwriten(path, s, s, 3, 6, pixels);

    free(pixels[5]);
    free(pixels[4]);
    free(pixels[3]);
    free(pixels[2]);
    free(pixels[1]);
    free(pixels[0]);
}

static void export1(lightprobe *L, int f, int w, int h, const char *path)
{
    gl_framebuffer export;
    void *pixels;

    // Render the sphere and copy the output to a buffer.

    gl_init_framebuffer(&export, w, h, 3);
    {
        draw(L, f, 0, 0, w, h, w, h, 0, export.frame);
        pixels = gl_copy_framebuffer(&export, 3);
    }
    gl_free_framebuffer(&export);

    // Write the buffer to a file and release it.

    tifwrite(path, w, h, 3, pixels);

    free(pixels);
}

//------------------------------------------------------------------------------

void lp_render(lightprobe *L, int f, int vx, int vy,
                                     int vw, int vh,
                                     int ww, int wh, float e)
{
    draw(L, f, vx, vy, vw, vh, ww, wh, e, 0);
}

void lp_export(lightprobe *L, int f, int s, const char *path)
{
    if      (f & LP_RENDER_CHART) export1(L, f, 2 * s, s, path);
    else if (f & LP_RENDER_POLAR) export1(L, f,     s, s, path);
    else if (f & LP_RENDER_CUBE)  export6(L, f,        s, path);
}

//------------------------------------------------------------------------------
