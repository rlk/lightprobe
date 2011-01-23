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

#define CIRCLE_VERT         "lp-circle.vert"
#define CIRCLE_FRAG         "lp-circle.frag"
#define SPHERE_ACC_VERT     "lp-sphere-acc.vert"
#define SPHERE_FIN_VERT     "lp-sphere-fin.vert"
#define SPHERE_DAT_ACC_FRAG "lp-sphere-dat-acc.frag"
#define SPHERE_DAT_FIN_FRAG "lp-sphere-dat-fin.frag"
#define SPHERE_RES_ACC_FRAG "lp-sphere-res-acc.frag"
#define SPHERE_RES_FIN_FRAG "lp-sphere-res-fin.frag"

#define LP_MAX_IMAGE 8

#define SPH_R 32
#define SPH_C 64

/*----------------------------------------------------------------------------*/

struct image
{
    GLuint texture;
    int    w;
    int    h;
    float  values[LP_MAX_VALUE];
};

typedef struct image image;

struct lightprobe
{
    GLuint  accum_prog;
    GLuint  final_prog;

    int     prog_mode;
    int     prog_flag;

    GLuint  vb_sphere;
    GLuint  qb_sphere;
    GLuint  lb_sphere;

    GLsizei w;
    GLsizei h;

    GLuint  frame;
    GLuint  color;
    GLuint  depth;

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

static int check_program_log(GLuint program, const char *vert_path,
                                             const char *frag_path)
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

            fprintf(stderr, "OpenGL Linker Error:\n\t%s\n\t%s\n%s",
                    vert_path, frag_path, p);
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

        if (check_program_log(program, path_vert, path_frag))
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

static void init_color(GLenum target, GLuint color, GLsizei w, GLsizei h)
{
    glBindTexture(target, color);

    glTexImage2D(target, 0, GL_RGBA32F, w, h, 0, GL_RGBA, GL_FLOAT, NULL);

    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(target, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
    glTexParameteri(target, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);
}

static void init_depth(GLenum target, GLuint depth, GLsizei w, GLsizei h)
{
    glBindTexture(target, depth);

    glTexImage2D(target, 0, GL_DEPTH_COMPONENT24, w, h, 0,
                 GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, NULL);

    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(target, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
    glTexParameteri(target, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);
}

static void test_frame()
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

void init_frame(GLuint frame, GLuint color, GLuint depth, GLsizei w, GLsizei h)
{
    GLenum target = GL_TEXTURE_RECTANGLE_ARB;
    
    init_color(target, color, w, h);
    init_depth(target, depth, w, h);

    glBindFramebuffer(GL_FRAMEBUFFER, frame);
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER,
                               GL_COLOR_ATTACHMENT0, target, color, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER,
                               GL_DEPTH_ATTACHMENT,  target, depth, 0);
        test_frame();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

/*----------------------------------------------------------------------------*/

/* Initialize the vertex buffers for a sphere. Compute a normal and tangent   */
/* vector for each vertex, noting that position is trivially derived from     */
/* normal.                                                                    */

static void make_sphere(GLuint vb, GLuint qb, GLuint lb, GLsizei r, GLsizei c)
{
    GLfloat *v = 0;
    GLshort *q = 0;
    GLshort *l = 0;

    GLsizei i;
    GLsizei j;

    /* Compute the buffer sizes. */

    const GLsizei vn = 6 * sizeof (GLfloat) * (r + 1) * (c + 1);
    const GLsizei qn = 4 * sizeof (GLshort) * (    r * c);
    const GLsizei ln = 2 * sizeof (GLshort) * (4 * r + c);

    /* Compute the vertex normal and tangent. */

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
            }
        glUnmapBuffer(GL_ARRAY_BUFFER);
    }

    /* Compute the face indices. */

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, qb);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, qn, 0, GL_STATIC_DRAW);

    if ((q = glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY)))
    {
        for     (i = 0; i <  r; i++)
            for (j = 0; j <  c; j++)
            {
                *q++ = (GLushort) ((i    ) * (c + 1) + (j    ));
                *q++ = (GLushort) ((i + 1) * (c + 1) + (j    ));
                *q++ = (GLushort) ((i + 1) * (c + 1) + (j + 1));
                *q++ = (GLushort) ((i    ) * (c + 1) + (j + 1));
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
    size_t s = 3 * sizeof (GLfloat);

    glBindBuffer(GL_ARRAY_BUFFER,         vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    {
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_NORMAL_ARRAY);
        {
            glVertexPointer(3, GL_FLOAT, 2 * s, (const GLvoid *) 0);
            glNormalPointer(   GL_FLOAT, 2 * s, (const GLvoid *) s);

            glDrawElements(mode, n, GL_UNSIGNED_SHORT, 0);
        }
        glDisableClientState(GL_NORMAL_ARRAY);
        glDisableClientState(GL_VERTEX_ARRAY);
    }
    glBindBuffer(GL_ARRAY_BUFFER,         0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

/*----------------------------------------------------------------------------*/

static void gl_init(lightprobe *L)
{
    /* Initialize all GLSL. */

    L->pro_circle         = load_program(CIRCLE_VERT, CIRCLE_FRAG);
    L->pro_sphere_dat_acc = load_program(SPHERE_ACC_VERT, SPHERE_DAT_ACC_FRAG);
    L->pro_sphere_dat_fin = load_program(SPHERE_FIN_VERT, SPHERE_DAT_FIN_FRAG);
    L->pro_sphere_res_acc = load_program(SPHERE_ACC_VERT, SPHERE_RES_ACC_FRAG);
    L->pro_sphere_res_fin = load_program(SPHERE_FIN_VERT, SPHERE_RES_FIN_FRAG);

    /* Generate off-screen render buffers. */

    glGenFramebuffers(1, &L->frame);
    glGenTextures    (1, &L->color);
    glGenTextures    (1, &L->depth);

    /* Generate and initialize vertex buffer objects. */

    glGenBuffers(1, &L->vb_sphere);
    glGenBuffers(1, &L->qb_sphere);
    glGenBuffers(1, &L->lb_sphere);

    make_sphere(L->vb_sphere, L->qb_sphere, L->lb_sphere, SPH_R, SPH_C);
}

static void gl_free(lightprobe *L)
{
    glDeleteBuffers(1, &L->vb_sphere);
    glDeleteBuffers(1, &L->qb_sphere);
    glDeleteBuffers(1, &L->lb_sphere);

    free_program(L->pro_circle);
    free_program(L->pro_sphere_dat_acc);
    free_program(L->pro_sphere_dat_fin);
    free_program(L->pro_sphere_res_acc);
    free_program(L->pro_sphere_res_fin);

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

    glUseProgram(L->pro_circle);

    UNIFORM1I(L->pro_circle, "image",    0);
    UNIFORM1F(L->pro_circle, "exposure", e);

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
        draw_circle_setup(L, w, h, e);
        draw_circle_image(L->images + L->select, L->pro_circle, w, h, x, y, z);
    }
}

/*----------------------------------------------------------------------------*/

/* Apply transformation matrices for rendering the interactive sphere view to */
/* a viewport with width W and height H, at pan position X Y, and zoom Z.     */

static void transform_view(int w, int h, float x, float y, float z)
{
    const GLdouble H = 0.1 * w / h / z;
    const GLdouble V = 0.1         / z;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-H, +H, -V, +V, 0.1, 5.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glRotated(180.0 * y -  90.0, 1.0, 0.0, 0.0);
    glRotated(360.0 * x - 180.0, 0.0, 1.0, 0.0);
}

#if 0
/* Apply transformation matrices for rendering cube map side I to an image    */
/* with width and height S.                                                   */

static void transform_cube(int s, int i)
{
}

/* Apply transformation matrices for rendering a full-dome map to an image    */
/* with width and height S.                                                   */

static void transform_dome(int s)
{
}

/* Apply transformation matrices for rendering a sphere map in an image with  */
/* width and height S.                                                        */

static void transform_sphere(int s)
{
}
#endif

/*----------------------------------------------------------------------------*/

/* Various render modes, render options, and export formats are implemented   */
/* by different GLSL programs. A mix-and-match of vertex and fragent shaders  */
/* with separate accumulation and final rendering passes take into account    */
/* all possible circumstances. When the mode, options, or operation changes,  */
/* the program must change accordingly.                                       */

enum
{
    LP_RENDER_IMAGE = 0,
    LP_RENDER_VIEW  = 1,
    LP_RENDER_CUBE  = 2,
    LP_RENDER_DOME  = 3,
    LP_RENDER_RECT  = 4,
};

void lp_set_program(lightprobe *L, int m, int f)
{
    /* If the mode or flags have changed... */

    if (m != L->prog_mode || f != L->prog_flag)
    {
        char *accum_vert = 0;
        char *accum_frag = 0;
        char *final_vert = 0;
        char *final_frag = 0;

        /* Determine the shader source files for the current mode and flags. */

        if (m == LP_RENDER_IMAGE)
        {
            final_vert = "lp-image.vert";
            final_frag = "lp-image.frag";
        }
        else
        {
            final_vert = "lp-final.vert"

            if (f & LP_RENDER_RES)
            {
                accum_frag = "lp-accum-reso.frag";
                final_frag = "lp-final-reso.frag";
            }
            else
            {
                accum_frag = "lp-accum-data.frag";
                final_frag = "lp-final-data.frag";
            }

            if (m == LP_RENDER_VIEW)
                accum_vert = "lp-accum-view.vert";
            if (m == LP_RENDER_CUBE)
                accum_vert = "lp-accum-cube.vert";
            if (m == LP_RENDER_DOME)
                accum_vert = "lp-accum-dome.vert";
            if (m == LP_RENDER_RECT)
                accum_vert = "lp-accum-rect.vert";
        }

        /* Release any previous program. */

        if (L->accum_prog)
            free_program(L->accum_prog);
        if (L->free_prog)
            free_program(L->final_prog);

        /* Load the new programs. */

        if (accum_vert && accum_frag)
            L->accum_prog = load_program(accum_vert, accum_frag);
        else
            L->accum_prog = 0;

        if (final_vert && final_frag)
            L->final_prog = load_program(final_vert, final_frag);
        else
            L->final_prog = 0;
    }
}

/*----------------------------------------------------------------------------*/

static float draw_sphere_image(GLuint vb, GLuint qb,
                               GLsizei r, GLsizei c,
                               GLuint  p, GLfloat s, image *t)
{
    glUseProgram(p);

    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, t->texture);

    UNIFORM1I(p, "image",    0);
    UNIFORM1F(p, "saturate", s);
    UNIFORM1F(p, "circle_r", t->values[LP_CIRCLE_RADIUS]);
    UNIFORM2F(p, "circle_p", t->values[LP_CIRCLE_X],
                             t->values[LP_CIRCLE_Y]);

    glMatrixMode(GL_TEXTURE);
    {
        glLoadIdentity();
        glRotatef(t->values[LP_SPHERE_ROLL],       0.0f, 0.0f, 1.0f);
        glRotatef(t->values[LP_SPHERE_ELEVATION], -1.0f, 0.0f, 0.0f);
        glRotatef(t->values[LP_SPHERE_AZIMUTH],    0.0f, 1.0f, 0.0f);
    }
    glMatrixMode(GL_MODELVIEW);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);

    draw_object(GL_QUADS, vb, qb, 4 * SPH_R * SPH_C);

    return 1.0f;
}

static void draw_sphere_grid(GLuint vb, GLuint qb, GLuint lb, GLsizei r, GLsizei c)
{
    glUseProgram(0);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glEnable(GL_LINE_SMOOTH);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.0f, 0.0f, 0.0f, 0.5f);

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    {
        glLineWidth(0.5f);
        draw_object(GL_QUADS, vb, qb, 4 * SPH_R     * SPH_C);
        glLineWidth(2.0f);
        draw_object(GL_LINES, vb, lb, 8 * SPH_R + 2 * SPH_C);
    }
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

static void draw_sphere_screen(GLuint p, GLfloat e, GLfloat c, GLuint color)
{
    glUseProgram(p);
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, color);

    UNIFORM1I(p, "image",    0);
    UNIFORM1F(p, "count",    c);
    UNIFORM1F(p, "exposure", e);

    glBegin(GL_QUADS);
    {
        glVertex2i(-1, -1);
        glVertex2i(+1, -1);
        glVertex2i(+1, +1);
        glVertex2i(-1, +1);
    }
    glEnd();
}

void lp_render_sphere(lightprobe *L, int f, int w, int h,
                      float x, float y, float e, float z)
{
    GLuint acc = (f & LP_RENDER_RES) ? L->pro_sphere_res_acc
                                     : L->pro_sphere_dat_acc;
    GLuint fin = (f & LP_RENDER_RES) ? L->pro_sphere_res_fin
                                     : L->pro_sphere_dat_fin;
    float c = 0.0;
    int i;

    assert(L);

    glViewport(0, 0, w, h);

    /* Resize the accumulation buffer to match the viewport as necessary. */

    if (L->w != (GLsizei) w || L->h != (GLsizei) h)
    {
        L->w  = (GLsizei) w;
        L->h  = (GLsizei) h;
        init_frame(L->frame, L->color, L->depth, L->w, L->h);
    }

    /* Render any/all images to the accumulation buffer. */

    glBindFramebuffer(GL_FRAMEBUFFER, L->frame);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    transform_view(w, h, x, y, z);

    if (f & LP_RENDER_ALL)
    {
        for (i = 0; i < LP_MAX_IMAGE; i++)
            if (L->images[i].texture)
                c += draw_sphere_image(L->vb_sphere, L->qb_sphere, SPH_R, SPH_C,
                                       acc, 0.0, L->images + i);
    }
    else
    {
        c += draw_sphere_image(L->vb_sphere, L->qb_sphere, SPH_R, SPH_C,
                               acc, 1.0, L->images + L->select);
    }

    /* Map the accumulation buffer to the screen. */

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    draw_sphere_screen(fin, e, c, L->color);

    if (f & LP_RENDER_GRID)
        draw_sphere_grid(L->vb_sphere, L->qb_sphere, L->lb_sphere, SPH_R, SPH_C);
}
/*----------------------------------------------------------------------------*/
