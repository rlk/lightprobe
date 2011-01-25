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

#define LP_MAX_IMAGE 8

#define SPH_R 32
#define SPH_C 64

/*----------------------------------------------------------------------------*/

struct framebuffer
{
    GLuint frame;
    GLuint color;
    GLuint depth;
};

typedef struct framebuffer framebuffer;

/*----------------------------------------------------------------------------*/

struct image
{
    GLuint texture;
    int    w;
    int    h;
    float  values[LP_MAX_VALUE];
};

typedef struct image image;

/*----------------------------------------------------------------------------*/

struct lightprobe
{
    /* GLSL programs. */

    int     prog_mode;
    int     prog_flag;

    GLuint  accum_prog;
    GLuint  final_prog;
    GLuint  annot_prog;

    /* Sphere vertex buffers. */

    GLuint  vert_buff;
    GLuint  fill_buff;
    GLuint  line_buff;

    GLsizei r;
    GLsizei c;

    /* Accumulation buffer. */

    framebuffer accum;
    GLsizei w;
    GLsizei h;

    /* Source images. */

    image images[LP_MAX_IMAGE];
    int   select;
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
/* WGL swap interval                                                          */

#ifdef _WIN32

#include <GL/wglext.h>

static void sync(int interval)
{
    PFNWGLSWAPINTERVALEXTPROC _wglSwapInvervalEXT;

    if ((_wglSwapInvervalEXT = (PFNWGLSWAPINTERVALEXTPROC)
          wglGetProcAddress("wglSwapIntervalEXT")))
         _wglSwapInvervalEXT(interval);
}

#endif

/*----------------------------------------------------------------------------*/
/* GLX swap interval                                                          */

#ifdef __linux__

static void sync(int interval)
{
    PFNGLXSWAPINTERVALSGIPROC _glXSwapInvervalSGI;

    if ((_glXSwapInvervalSGI = (PFNGLXSWAPINTERVALSGIPROC)
          glXGetProcAddress((const GLubyte *) "glXSwapIntervalSGI")))
         _glXSwapInvervalSGI(interval);
}

#endif

/*----------------------------------------------------------------------------*/
/* CGL swap interval                                                          */

#ifdef __APPLE__

#include <OpenGL/OpenGL.h>

static void sync(int interval)
{
    CGLSetParameter(CGLGetCurrentContext(), kCGLCPSwapInterval, &interval);
}

#endif

/*----------------------------------------------------------------------------*/

#include "srgb.h"

/* Write the contents of an array buffers to a multi-page 4-channel 32-bit    */
/* floating point TIFF image.                                                 */

static void tifwriten(const char *path, int w, int h, int n, void **p)
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
            TIFFSetField(T, TIFFTAG_SAMPLESPERPIXEL, 4);
        
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

/* Write the contents of a buffer to a 4-channel 32-bit floating point TIFF   */
/* image.                                                                     */

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
        TIFFSetField(T, TIFFTAG_ICCPROFILE,   sRGB_icc_len, sRGB_icc);

        s = (uint32) TIFFScanlineSize(T);

        for (i = 0; i < h; ++i)
            TIFFWriteScanline(T, (uint8 *) p + (h - i - 1) * s, i, 0);

        TIFFClose(T);
    }
}

/* Load the contents of a TIFF image to a newly-allocated buffer.  Return the */
/* buffer and its configuration.                                              */

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

/* Load the named TIFF image into a 32-bit floating point OpenGL rectangular  */
/* texture. Release the image buffer after loading, and return the texture    */
/* object.                                                                    */

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

/* Load the named multi-page TIFF image into a 32-bit floating point OpenGL   */
/* cube map. Release the image buffer after loading, and return the texture   */
/* object.                                                                    */

unsigned int lp_load_cubemap(const char *path)
{
    const GLenum T = GL_TEXTURE_CUBE_MAP;

    static const GLenum targets[6] = {
        GL_TEXTURE_CUBE_MAP_POSITIVE_X,
        GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
        GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
        GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
        GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
        GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
    };

    GLuint o = 0;
    void  *p = 0;

    int f, w, h, c, b;

    glGenTextures(1, &o);
    glBindTexture(T,  o);

    glTexParameteri(T, GL_TEXTURE_WRAP_S,     GL_CLAMP);
    glTexParameteri(T, GL_TEXTURE_WRAP_T,     GL_CLAMP);
    glTexParameteri(T, GL_TEXTURE_WRAP_R,     GL_CLAMP);
    glTexParameteri(T, GL_GENERATE_MIPMAP,    GL_TRUE);
    glTexParameteri(T, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(T, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

    for (f = 0; f < 6; ++f)

        if ((p = tifread(path, f, &w, &h, &c, &b)))
        {
            GLenum i = internal_form(b, c);
            GLenum e = external_form(c);
            GLenum t = external_type(b);

            glTexImage2D(targets[f], 0, i, w, h, 0, e, t, p);

            free(p);
        }

    return (unsigned int) o;
}

/*----------------------------------------------------------------------------*/

/* Check the shader compile status.  If failed, print the log.                */

static int check_shader_log(GLuint shader, const unsigned char *glsl)
{
    const char *type = (shader == GL_VERTEX_SHADER) ? "vertex" : "fragment";

    GLchar *p = 0;
    GLint   s = 0;
    GLint   n = 0;

    glGetShaderiv(shader, GL_COMPILE_STATUS,  &s);
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &n);

    if (s == 0)
    {
        if ((p = (GLchar *) calloc(n + 1, 1)))
        {
            glGetShaderInfoLog(shader, n, NULL, p);

            fprintf(stderr, "OpenGL %s shader error:\n%s\n%s", type, p, glsl);
            free(p);
        }
        return 0;
    }
    return 1;
}

/* Check the program link status.  If failed, print the log.                  */

static int check_program_log(GLuint program)
{
    GLchar *p = 0;
    GLint   s = 0;
    GLint   n = 0;

    glGetProgramiv(program, GL_LINK_STATUS,     &s);
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &n);

    if (s == 0)
    {
        if ((p = (GLchar *) calloc(n + 1, 1)))
        {
            glGetProgramInfoLog(program, n, NULL, p);

            fprintf(stderr, "OpenGL program linker error:\n%s", p);
            free(p);
        }
        return 0;
    }
    return 1;
}

/*----------------------------------------------------------------------------*/

/* Prepare a GLSL shader, taking it from source code to usable GL object.     */
/* Report any errors in the log, and return 0 on failure.                     */

static GLuint load_shader(GLenum type, const unsigned char *str,
                                             unsigned int   len)
{
    /* Compile a new shader with the given source. */

    GLuint shader = glCreateShader(type);

    glShaderSource (shader, 1, (const GLchar **) &str,
                               (const GLint   *) &len);
    glCompileShader(shader);

    /* If the shader is valid, return it.  Else, delete it. */

    if (check_shader_log(shader, str))
        return shader;
    else
        glDeleteShader(shader);

    return 0;
}

/* Short-cut the shader type parameter to load_shader.                        */

static GLuint load_vshader(const unsigned char *str, unsigned int len)
{
    return load_shader(GL_VERTEX_SHADER,   str, len);
}

static GLuint load_fshader(const unsigned char *str, unsigned int len)
{
    return load_shader(GL_FRAGMENT_SHADER, str, len);
}

/* Receive vertex and fragment shader objects. Link these into a GLSL program */
/* object, checking logs and reporting any errors. Return 0 on failure.       */

static GLuint load_program(GLuint vert_shader, GLuint frag_shader)
{
    if (vert_shader && frag_shader)
    {
        /* Link a new program with the given shaders. */

        GLuint program = glCreateProgram();

        glAttachShader(program, vert_shader);
        glAttachShader(program, frag_shader);

        glLinkProgram(program);

        /* If the program is valid, return it.  Else, delete it. */

        if (check_program_log(program))
            return program;
        else
            glDeleteProgram(program);
    }
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

static void size_color(GLenum target, GLuint buffer, GLsizei w, GLsizei h)
{
    glBindTexture(target, buffer);

    glTexImage2D(target, 0, GL_RGBA32F, w, h, 0, GL_RGBA, GL_FLOAT, NULL);

    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(target, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
    glTexParameteri(target, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);
}

static void size_depth(GLenum target, GLuint buffer, GLsizei w, GLsizei h)
{
    glBindTexture(target, buffer);

    glTexImage2D(target, 0, GL_DEPTH_COMPONENT24, w, h, 0,
                 GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, NULL);

    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(target, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
    glTexParameteri(target, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);
}

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

static void size_framebuffer(framebuffer *F, GLsizei w, GLsizei h)
{
    GLenum target = GL_TEXTURE_RECTANGLE_ARB;
    
    size_color(target, F->color, w, h);
    size_depth(target, F->depth, w, h);

    glBindFramebuffer(GL_FRAMEBUFFER, F->frame);
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER,
                               GL_COLOR_ATTACHMENT0, target, F->color, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER,
                               GL_DEPTH_ATTACHMENT,  target, F->depth, 0);
        test_framebuffer();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void init_framebuffer(framebuffer *F, GLsizei w, GLsizei h)
{
    glGenFramebuffers(1, &F->frame);
    glGenTextures    (1, &F->color);
    glGenTextures    (1, &F->depth);

    if (w && h) size_framebuffer(F, w, h);
}

static void free_framebuffer(framebuffer *F)
{
    glDeleteTextures    (1, &F->depth);
    glDeleteTextures    (1, &F->color);
    glDeleteFramebuffers(1, &F->frame);
}

static void *read_framebuffer(framebuffer *F, GLsizei w, GLsizei h)
{
    GLubyte *p = 0;

    glBindFramebuffer(GL_FRAMEBUFFER, F->frame);
    {
        if ((p = (GLubyte *) malloc(w * h * 4 * sizeof (GLfloat))))
        {
            glReadBuffer(GL_FRONT);
            glReadPixels(0, 0, w, h, GL_RGBA, GL_FLOAT, p);
            glReadBuffer(GL_BACK);
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return p;
}

/*----------------------------------------------------------------------------*/

/* Various render modes, render options, and export formats are implemented   */
/* by different GLSL programs. A mix-and-match of vertex and fragent shaders  */
/* with separate accumulation and final rendering passes take into account    */
/* all possible circumstances. When the mode, options, or operation changes,  */
/* the program must change accordingly.                                       */

#include "lp-cube-vs.h"
#include "lp-dome-vs.h"
#include "lp-rect-vs.h"
#include "lp-view-vs.h"
#include "lp-accum-data-fs.h"
#include "lp-accum-reso-fs.h"
#include "lp-final-tone-fs.h"
#include "lp-final-data-fs.h"
#include "lp-final-reso-fs.h"
#include "lp-annot-fs.h"
#include "lp-image-vs.h"
#include "lp-image-fs.h"

enum
{
    LP_RENDER_IMAGE = 0,
    LP_RENDER_CUBE  = 1,
    LP_RENDER_DOME  = 2,
    LP_RENDER_RECT  = 3,
    LP_RENDER_VIEW  = 4,
};

static GLuint lp_choose_vshader(int m)
{
    switch (m)
    {
    case LP_RENDER_IMAGE:
        return load_vshader(lp_image_vs_glsl, lp_image_vs_glsl_len);
    case LP_RENDER_CUBE:
        return load_vshader(lp_cube_vs_glsl,  lp_cube_vs_glsl_len);
    case LP_RENDER_DOME:
        return load_vshader(lp_dome_vs_glsl,  lp_dome_vs_glsl_len);
    case LP_RENDER_RECT:
        return load_vshader(lp_rect_vs_glsl,  lp_rect_vs_glsl_len);
    case LP_RENDER_VIEW:
        return load_vshader(lp_view_vs_glsl,  lp_view_vs_glsl_len);
    }
    return 0;
}

static GLuint lp_accum_program(int m, int f)
{
    if (m == LP_RENDER_IMAGE)
        return 0;
    else
    {
        GLuint fs = 0;

        if (f & LP_RENDER_RES)
            fs = load_fshader(lp_accum_reso_fs_glsl, lp_accum_reso_fs_glsl_len);
        else
            fs = load_fshader(lp_accum_data_fs_glsl, lp_accum_data_fs_glsl_len);

        return load_program(lp_choose_vshader(m), fs);
    }
}

static GLuint lp_final_program(int m, int f)
{
    GLuint fs = 0;

    if      (m == LP_RENDER_IMAGE)
        fs = load_fshader(lp_image_fs_glsl,      lp_image_fs_glsl_len);
    else if (f &  LP_RENDER_RES)
        fs = load_fshader(lp_final_reso_fs_glsl, lp_final_reso_fs_glsl_len);
    else if (m == LP_RENDER_VIEW)
        fs = load_fshader(lp_final_tone_fs_glsl, lp_final_tone_fs_glsl_len);
    else
        fs = load_fshader(lp_final_data_fs_glsl, lp_final_data_fs_glsl_len);

    return load_program(lp_choose_vshader(m), fs);
}

static GLuint lp_annot_program(int m, int f)
{
    if (m == LP_RENDER_IMAGE)
        return 0;
    else
    {
        GLuint vs = lp_choose_vshader(m);
        GLuint fs = load_fshader(lp_annot_fs_glsl, lp_annot_fs_glsl_len);

        return load_program(vs, fs);
    }
}

static void lp_set_program(lightprobe *L, int m, int f)
{
    /* If the mode or flags have changed... */

    if (m != L->prog_mode || f != L->prog_flag)
    {
        L->prog_mode = m;
        L->prog_flag = f;

        /* Release any previous program. */

        if (L->accum_prog) free_program(L->accum_prog);
        if (L->final_prog) free_program(L->final_prog);
        if (L->annot_prog) free_program(L->annot_prog);

        /* Load the new programs. */

        L->accum_prog = lp_accum_program(m, f);
        L->final_prog = lp_final_program(m, f);
        L->annot_prog = lp_annot_program(m, f);
    }
}

/* The accumulation buffer size can change due to a window resize event or an */
/* export request with a specific size. Resize the FBO-bound textures to      */
/* match the gives size as necessary.                                         */

static void lp_set_buffer(lightprobe *L, int w, int h)
{
    if (L->w != (GLsizei) w || L->h != (GLsizei) h)
    {
        size_framebuffer(&L->accum, w, h);
        L->w  = (GLsizei) w;
        L->h  = (GLsizei) h;
    }
}

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

/* Render the sphere defined by the previous function. Pass the normal and    */
/* tangent vectors along using the position and normal vertex attributes.     */

static void draw_object(GLenum mode, GLuint vbo, GLuint ebo, GLsizei n)
{
    size_t s = sizeof (GLfloat);

    glBindBuffer(GL_ARRAY_BUFFER,         vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    {
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_NORMAL_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        {
            glVertexPointer  (3, GL_FLOAT, 8 * s, (const GLvoid *) (s * 0));
            glNormalPointer  (   GL_FLOAT, 8 * s, (const GLvoid *) (s * 3));
            glTexCoordPointer(2, GL_FLOAT, 8 * s, (const GLvoid *) (s * 6));

            glDrawElements(mode, n, GL_UNSIGNED_SHORT, 0);
        }
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glDisableClientState(GL_NORMAL_ARRAY);
        glDisableClientState(GL_VERTEX_ARRAY);
    }
    glBindBuffer(GL_ARRAY_BUFFER,         0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

/*----------------------------------------------------------------------------*/

static void gl_init(lightprobe *L, GLsizei r, GLsizei c)
{
    L->prog_mode = -1;
    L->prog_flag = -1;

    /* Generate off-screen render buffers. */

    init_framebuffer(&L->accum, 0, 0);

    /* Generate and initialize vertex buffer objects. */

    glGenBuffers(1, &L->vert_buff);
    glGenBuffers(1, &L->fill_buff);
    glGenBuffers(1, &L->line_buff);

    L->r = r;
    L->c = c;
    make_sphere(L->vert_buff, L->fill_buff, L->line_buff, L->r, L->c);
}

static void gl_free(lightprobe *L)
{
    glDeleteBuffers(1, &L->vert_buff);
    glDeleteBuffers(1, &L->fill_buff);
    glDeleteBuffers(1, &L->line_buff);

    free_framebuffer(&L->accum);

    if (L->accum_prog) free_program(L->accum_prog);
    if (L->final_prog) free_program(L->final_prog);
    if (L->annot_prog) free_program(L->annot_prog);

    L->accum_prog = 0;
    L->final_prog = 0;
    L->annot_prog = 0;

    L->w = 0;
    L->h = 0;
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
    sync(1);

    if ((L = (lightprobe *) calloc (1, sizeof (lightprobe))))
        gl_init(L, SPH_R, SPH_C);

    return L;
}

/* Bounce the GLSL state of the given lightprobe. This is useful during GLSL  */
/* development                                                                */

void lp_tilt(lightprobe *L)
{
    gl_free(L);
    gl_init(L, SPH_R, SPH_C);
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

int lp_add_image(lightprobe *L, const char *path)
{
    GLuint o = 0;
    int    i = 0;
    int    w = 0;
    int    h = 0;

    assert(L);
    assert(path);

    /* Load the texture. */

    if ((o = lp_load_texture(path, &w, &h)))
    {
        /* Find the lightprobe's first unused image slot. */

        for (i = 0; i < LP_MAX_IMAGE; i++)
            if (L->images[i].texture == 0)
            {
                /* Store the texture. */

                L->images[i].texture = o;
                L->images[i].w       = w;
                L->images[i].h       = h;

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
    int j;

    assert(L);
    assert(0 <= i && i < LP_MAX_IMAGE);

    /* Release the image. */

    if (L->images[i].texture)
    {
        glDeleteTextures(1, &L->images[i].texture);
        memset(L->images + i, 0, sizeof (image));
    }

    /* Select another image, if possible. */

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

/*----------------------------------------------------------------------------*/

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

/*----------------------------------------------------------------------------*/

/* Set up circle rendering to a viewport of width W and height H. Set the     */
/* uniform values for exposure E.                                             */

static void draw_circle_setup(lightprobe *L, int w, int h, float e)
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

    glUseProgram(L->final_prog);

    UNIFORM1I(L->final_prog, "image",    0);
    UNIFORM1F(L->final_prog, "exposure", e);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
}

/* Render the image at index i scaled to zoom level Z, translated to panning  */
/* position X Y.                                                              */

static void draw_circle_image(image *c, GLuint p, int w, int h,
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
    assert(L);

    if (L->images[L->select].texture)
    {
        lp_set_program(L, LP_RENDER_IMAGE, f);
        lp_set_buffer (L, w, h);

        draw_circle_setup(L, w, h, e);
        draw_circle_image(L->images + L->select, L->final_prog, w, h, x, y, z);
    }
}

/*----------------------------------------------------------------------------*/

/* Render the given lightprobe image to the accumulation buffer with a blend  */
/* function of one-one. Use the image's unwrapped per-pixel quality as the    */
/* alpha value, and write pre-multiplied color. The result is a weighted sum  */
/* of images, with the total weight in the alpha channel.                     */

static void draw_sphere_accum(lightprobe *L, image *I, GLfloat s)
{
    glUseProgram(L->accum_prog);

    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, I->texture);

    UNIFORM1I(L->accum_prog, "image",    0);
    UNIFORM1F(L->accum_prog, "saturate", s);
    UNIFORM1F(L->accum_prog, "circle_r", I->values[LP_CIRCLE_RADIUS]);
    UNIFORM2F(L->accum_prog, "circle_p", I->values[LP_CIRCLE_X],
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
    draw_object(GL_QUADS, L->vert_buff, L->fill_buff, 4 * L->r * L->c);
}

/* Render the accumulation buffer to the output buffer. Divide the RGB color  */
/* by its alpha value, normalizing the weighted sum that resulted from the    */
/* accumulation of images previously, and giving the final quality-blended    */
/* blend of inputs. If we're rendering to the screen, then apply an exposure  */
/* mapping.                                                                   */

static void draw_sphere_final(lightprobe *L, GLfloat e)
{
    glUseProgram(L->final_prog);

    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, L->accum.color);

    UNIFORM1I(L->final_prog, "image",    0);
    UNIFORM1F(L->final_prog, "exposure", e);

    glBlendFunc(GL_ONE, GL_ZERO);
    draw_object(GL_QUADS, L->vert_buff, L->fill_buff, 4 * L->r * L->c);
}

static void draw_sphere_grid(lightprobe *L)
{
    glUseProgram(L->annot_prog);

    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    {
        glLineWidth(0.25f);
        draw_object(GL_QUADS, L->vert_buff, L->fill_buff, 4 * L->r     * L->c);
        glLineWidth(2.00f);
        draw_object(GL_LINES, L->vert_buff, L->line_buff, 8 * L->r + 2 * L->c);
    }
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

static void draw_sphere(GLuint frame, lightprobe *L, int f, float e)
{
    glEnable(GL_BLEND);

    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    /* Render any/all images to the accumulation buffer. */

    glBindFramebuffer(GL_FRAMEBUFFER, L->accum.frame);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (f & LP_RENDER_ALL)
    {
        int i;
        for (i = 0; i < LP_MAX_IMAGE; i++)
            if (L->images[i].texture)
                draw_sphere_accum(L, L->images + i, 0.0);
    }
    else
        draw_sphere_accum(L, L->images + L->select, 1.0);

    /* Map the accumulation buffer to the output buffer. */

    glBindFramebuffer(GL_FRAMEBUFFER, frame);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    draw_sphere_final(L, e);

    /* Draw the grid. */

    if (f & LP_RENDER_GRID)
        draw_sphere_grid(L);
}

/*----------------------------------------------------------------------------*/

void lp_render_sphere(lightprobe *L, int f, int w, int h,
                      float x, float y, float e, float z)
{
    /* Compute the frustum parameters. */

    const GLdouble H = 0.1 * w / h / z;
    const GLdouble V = 0.1         / z;

    assert(L);

    /* Apply the view transformation. */

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-H, +H, -V, +V, 0.1, 5.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glRotated(180.0 * y -  90.0, 1.0, 0.0, 0.0);
    glRotated(360.0 * x - 180.0, 0.0, 1.0, 0.0);

    /* Update the program and buffer caches.  Render the sphere. */

    lp_set_program(L, LP_RENDER_VIEW, f);
    lp_set_buffer(L, w, h);
    glViewport(0, 0, w, h);

    draw_sphere(0, L, f, e);
}

/*----------------------------------------------------------------------------*/

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

/*----------------------------------------------------------------------------*/

void lp_export_cube(lightprobe *L, const char *path, int s, int f)
{
    transform_func transform[6] = {
        transform_pos_x,
        transform_neg_x,
        transform_pos_y,
        transform_neg_y,
        transform_pos_z,
        transform_neg_z
    };
    GLfloat color[6][3] = {
        { 1.0f, 0.0f, 0.0f },
        { 1.0f, 0.0f, 1.0f },
        { 0.0f, 1.0f, 0.0f },
        { 1.0f, 1.0f, 0.0f },
        { 0.0f, 0.0f, 1.0f },
        { 0.0f, 1.0f, 1.0f }
    };

    framebuffer export;
    void       *pixels[6];
    int i;

    /* Update the program and buffer caches. */

    lp_set_program(L, LP_RENDER_CUBE, f);
    lp_set_buffer (L, s, s);

    init_framebuffer(&export, s, s);
    {
        GLfloat d = 0.25;

        glViewport(0, 0, s, s);

        for (i = 0; i < 6; ++i)
        {
            /* Apply the view transformation and render the sphere. */

            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glFrustum(-d, +d, -d, +d, d, 5.0);

            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();
            transform[i]();
/*
            glBindFramebuffer(GL_FRAMEBUFFER, export.frame);
            glClearColor(color[i][0], color[i][1], color[i][2], 1.0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
*/
            draw_sphere(export.frame, L, f, 0);

            /* Copy the output to a buffer. */

            pixels[i] = read_framebuffer(&export, s, s);
        }
    }
    free_framebuffer(&export);

    /* Write the buffers to a file and release them. */

    tifwriten(path, s, s, 6, pixels);

    for (i = 0; i < 6; ++i)
        free(pixels[i]);
}

/*----------------------------------------------------------------------------*/

static void lp_export(lightprobe *L,
                      const char *path, int m, int w, int h, int f)
{
    framebuffer export;
    void       *pixels;

    /* Update the program and buffer caches. */

    lp_set_program(L, m, f);
    lp_set_buffer (L, w, h);

    /* Initialize the export framebuffer. */

    init_framebuffer(&export, w, h);
    {
        /* Render the sphere. */

        glViewport(0, 0, w, h);
        draw_sphere(export.frame, L, f, 1.0);

        /* Copy the output to a buffer and write the buffer to a file. */

        if ((pixels = read_framebuffer(&export, w, h)))
        {
            tifwrite(path, w, h, pixels);
            free(pixels);
        }
    }
    free_framebuffer(&export);
}

void lp_export_dome(lightprobe *L, const char *path, int s, int f)
{
    lp_export(L, path, LP_RENDER_DOME, s, s, f);
}

void lp_export_sphere(lightprobe *L, const char *path, int s, int f)
{
    lp_export(L, path, LP_RENDER_RECT, 2 * s, s, f);
}

/*----------------------------------------------------------------------------*/
