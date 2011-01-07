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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tiffio.h>
#include <GL/glew.h>

#include "lp-render.h"

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

struct lightprobe
{
    GLuint circle_program;
    GLuint sphere_program;

    struct image images[LP_MAX_IMAGE];
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

/* Allocate and initialize a new, empty lightprobe object. Initialize all GL  */
/* state needed to operate upon the input and render the output.              */

lightprobe *lp_init()
{
    lightprobe *L = 0;

    /* GLEW initialization has to go somewhere, and it might as well go here. */
    /* It would be ugly to require the user to call it, and it shouldn't hurt */
    /* to call it multiple times.                                             */

    glewInit();

    if ((L = (lightprobe *) calloc (1, sizeof (lightprobe))))
    {
        L->circle_program = 0;
        L->sphere_program = 0;

        L->circle_program = load_program(CIRCLE_VERT, CIRCLE_FRAG);
        L->sphere_program = load_program(SPHERE_VERT, SPHERE_FRAG);
    }

    return L;
}

/* Release a lightprobe object and all state associated with it.  Unload any  */
/* images and delete all OpenGL state.                                        */

void lp_free(lightprobe *L)
{
    int i;

    assert(L);

    for (i = 0; i < LP_MAX_IMAGE; i++)
        lp_del_image(L, i);

    free_program(L->circle_program);
    free_program(L->sphere_program);

    free(L);
}

/*----------------------------------------------------------------------------*/

int lp_export_cube(lightprobe *L, const char *path)
{
    printf("Export Cube Map %s\n", path);
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
        memset(L->images + i, 0, sizeof (struct image));
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

static void render_circle_setup(lightprobe *L, int w, int h, float e, float z)
{
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glViewport(0, 0, w, h);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, w, h, 0, 0, 1);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glEnable(GL_TEXTURE_RECTANGLE_ARB);

    glUseProgram(L->circle_program);
    glUniform1f(glGetUniformLocation(L->circle_program, "exposure"), e);
    glUniform1f(glGetUniformLocation(L->circle_program, "zoom"),     z);
}

static void render_circle_image(lightprobe *L, struct image *c,
                                int w, int h, float x, float y, float z)
{
    int X = -(c->w * z - w) * x;
    int Y = -(c->h * z - h) * y;

    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, c->texture);
    glUniform2f(glGetUniformLocation(L->circle_program, "size"), c->w, c->h);

    glPushMatrix();
    {
        glTranslated(X, Y, 0.0);
        glScaled(z, z, 1.0);
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

void lp_render_circle(lightprobe *L, int f, int w, int h,
                      float x, float y, float e, float z)
{
    int i;

    assert(L);
/*
    printf("render_circle %d %d %d %f %f %f %f\n", f, w, h, x, y, e, z);
*/
    render_circle_setup(L, w, h, e, z);

    for (i = 0; i < LP_MAX_IMAGE; i++)
        if ((L->images[i].flags & LP_FLAG_LOADED) != 0 &&
            (L->images[i].flags & LP_FLAG_HIDDEN) == 0)
            render_circle_image(L, L->images + i, w, h, x, y, z);
}

/*----------------------------------------------------------------------------*/

void lp_render_sphere(lightprobe *L, int f, int w, int h,
                      float x, float y, float e, float z)
{
    if (L)
    {
        GLdouble l = (GLdouble) w / (GLdouble) h;

        glClearColor(0.2, 0.2, 0.2, 0.0);
        glClear(GL_COLOR_BUFFER_BIT);

        glViewport(0, 0, w, h);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glFrustum(-l, l, -1.0, 1.0, 2.0, 10.0);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glTranslated(0.0, 0.0, -5.0);

        glBegin(GL_QUADS);
        {
            glColor4d(1.0, 1.0, 0.0, 1.0);
            glVertex3d(0.0, 0.0, 0.0);
            glVertex3d(1.0, 0.0, 0.0);
            glVertex3d(1.0, 1.0, 0.0);
            glVertex3d(0.0, 1.0, 0.0);
        }
        glEnd();
    }
}

/*----------------------------------------------------------------------------*/
