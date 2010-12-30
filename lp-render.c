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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tiffio.h>

#ifdef __APPLE__
#include <OpenGL/OpenGL.h>
#else
#include <GL/glew.h>
#endif

#include "lp-render.h"

/*----------------------------------------------------------------------------*/

struct image
{
    char  *path;
    int    w;
    int    h;
    GLuint texture;

    int    flags;

    float  circle_x;
    float  circle_y;
    float  circle_r;

    float  sphere_e;
    float  sphere_a;
    float  sphere_r;

    struct image *next;
};

typedef struct image image;

struct lightprobe
{
    struct image *first;
    struct image *last;
};

/*----------------------------------------------------------------------------*/

/* Load the contents of a 4-channel 32-bit floating point TIFF image to a     */
/* newly-allocated buffer.  Return the buffer and size.  Return 0 if the      */
/* named image doesn't match the expected format.                             */

static void *tifread(const char *path, int *w, int *h)
{
    TIFF *T = 0;
    void *p = 0;
    
    if ((T = TIFFOpen(path, "r")))
    {
        uint32 W, H, C, B, i, s = (uint32) TIFFScanlineSize(T);
        
        TIFFGetField(T, TIFFTAG_IMAGEWIDTH,      &W);
        TIFFGetField(T, TIFFTAG_IMAGELENGTH,     &H);
        TIFFGetField(T, TIFFTAG_BITSPERSAMPLE,   &B);
        TIFFGetField(T, TIFFTAG_SAMPLESPERPIXEL, &C);

        if (C == 4 && B == 32)
        {
            if ((p = malloc(H * s)))
            {         
                for (i = 0; i < *h; ++i)
                    TIFFReadScanline(T, (uint8 *) p + i * s, i, 0);
                    
                *w = (int) W;
                *h = (int) H;

                printf("read %s %d %d\n", path, *w, *h);
            }
        }
        TIFFClose(T);
    }
    return p;
}

/* Write the contents of a buffer to a 4-channel 32-bit floating point TIFF   */
/* image.                                                                     */

void tifwrite(const char *path, int w, int h, void *p)
{
    TIFF *T = 0;
    
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

/*----------------------------------------------------------------------------*/

/* Load the named TIFF image into a 32-bit floating point OpenGL rectangular  */
/* texture. Release the image buffer after loading, and return the texture    */
/* object.                                                                    */

static GLuint load_image(const char *path, int *w, int *h)
{
    const GLenum T = GL_TEXTURE_RECTANGLE_ARB;

    GLuint o = 0;
    void  *p = 0;

    if ((p = tifread(path, w, h)))
    {
        glGenTextures(1, &o);
        glBindTexture(T,  o);

        glTexImage2D(T, 0, GL_RGBA32F_ARB, *w, *h, 0, GL_RGBA, GL_FLOAT, p);

        glTexParameteri(T, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(T, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(T, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(T, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        free(p);
    }
    return o;
}

/*----------------------------------------------------------------------------*/

static image *find_image(lightprobe *L, const char *path)
{
    /* Iterate over the images, comparing each to the given path. */

    image *c;

    for (c = L->first; c; c = c->next)
        if (strcmp(c->path, path) == 0)
            return c;

    return 0;
}

static image *append_image(lightprobe *L, const char *path, int f,
                                        float cx, float cy, float cr,
                                        float se, float sa, float sr)
{
    /* Allocate a new image structure. */

    image *c;

    if ((c = (image *) malloc (sizeof (image))))
    {
        /* Cache the image path name. */

        if ((c->path = (char *) malloc(strlen(path) + 1)))
            strcpy(c->path, path);

        /* Load the texture and initialize the configuration. */

        c->texture  = load_image(path, &c->w, &c->h);
        c->flags    = f;
        c->next     = 0;

        c->circle_x = cx;
        c->circle_y = cy;
        c->circle_r = cr;
        c->sphere_e = se;
        c->sphere_a = sa;
        c->sphere_r = sr;

        /* Append the new image to the lightprobe's image list. */

        if (L->last)
            L->last->next = c;
        else
            L->first      = c;

        L->last = c;
    }
    return c;
}

static void remove_image(lightprobe *L, const char *path)
{
    /* Iterate over the images, comparing each to the given path. */

    image *p = 0;
    image *c = 0;

    for (c = L->first; c; p = c, c = c->next)
        if (strcmp(c->path, path) == 0)
        {
            /* Remove the image from the lightprobe's image list. */
            if (p)
                p->next  = c->next;
            else
                L->first = c->next;

            if (L->last == c)
                L->last  = p;

            /* Release the image. */

            glDeleteTextures(1, &c->texture);

            free(c->path);
            free(c);

            return;
        }
}

/*----------------------------------------------------------------------------*/

lightprobe *lp_init()
{
    lightprobe *L = 0;

    if ((L = (lightprobe *) malloc (sizeof (lightprobe))))
    {
        L->first = 0;
        L->last  = 0;
    }

    return L;
}

lightprobe *lp_open(const char *path)
{
    lightprobe *L = 0;

    if (path)
    {
        if ((L = (lightprobe *) malloc (sizeof (lightprobe))))
        {
            L->first = 0;
            L->last  = 0;
        }
    }

    return L;
}
 
void lp_free(lightprobe *L)
{
    printf("Close\n");

    if (L)
    {
        free(L);
    }
}
 
int lp_save(lightprobe *L, const char *path)
{
    printf("Save %s\n", path);
    return 1;
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

void lp_append_image(lightprobe *L, const char *path)
{
    if (L && path)
        append_image(L, path, LP_IMAGE_ACTIVE, 0.0f, 0.0f, 0.0f,
                                               0.0f, 0.0f, 0.0f);
}

void lp_remove_image(lightprobe *L, const char *path)
{
    if (L && path)
        remove_image(L, path);
}

/*----------------------------------------------------------------------------*/

void lp_set_image_flags(lightprobe *L, const char *path, int f)
{
    image *c;

    if ((c = find_image(L, path)))
        c->flags = f;
}

int lp_get_image_flags(lightprobe *L, const char *path)
{
    image *c;

    if ((c = find_image(L, path)))
        return c->flags;

    return 0;
}

/*----------------------------------------------------------------------------*/

void lp_move_circle(lightprobe *L, float x, float y, float r)
{
    image *c;

    for (c = L->first; c; c = c->next)
        if (c->flags & LP_IMAGE_ACTIVE)
        {
            c->circle_x += x;
            c->circle_y += y;
            c->circle_r += r;
        }

    printf("Circle %f %f %f\n", x, y, r);
}

void lp_move_sphere(lightprobe *L, float e, float a, float r)
{
    image *c;

    for (c = L->first; c; c = c->next)
        if (c->flags & LP_IMAGE_ACTIVE)
        {
            c->sphere_e += e;
            c->sphere_a += a;
            c->sphere_r += r;
        }

    printf("Sphere %f %f %f\n", e, a, r);
}

/*----------------------------------------------------------------------------*/

void lp_render_circle(lightprobe *L, int f, int w, int h,
                      float x, float y, float e, float z)
{
}

void lp_render_sphere(lightprobe *L, int f, int w, int h,
                      float x, float y, float e, float z)
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

