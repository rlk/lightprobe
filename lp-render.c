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

#include <stdlib.h>
#include <stdio.h>

#ifdef __APPLE__
#include <OpenGL/OpenGL.h>
#else
#include <GL/glew.h>
#endif

#include "lp-render.h"

/*----------------------------------------------------------------------------*/

struct lightprobe
{
    float circle_x;
    float circle_y;
    float circle_r;

    float sphere_e;
    float sphere_a;
    float sphere_r;
};

/*----------------------------------------------------------------------------*/

lightprobe *lp_init()
{
    lightprobe *lp = 0;

    printf("New\n");

    if ((lp = (lightprobe *) malloc (sizeof (lightprobe))))
    {
        lp->circle_x = 0.0f;
        lp->circle_y = 0.0f;
        lp->circle_r = 0.0f;

        lp->sphere_e = 0.0f;
        lp->sphere_a = 0.0f;
        lp->sphere_r = 0.0f;
    }

    return lp;
}

lightprobe *lp_open(const char *path)
{
    lightprobe *lp = 0;

    if (path)
    {
        printf("Open %s\n", path);

        if ((lp = (lightprobe *) malloc (sizeof (lightprobe))))
        {
            lp->circle_x = 0.0f;
            lp->circle_y = 0.0f;
            lp->circle_r = 0.0f;

            lp->sphere_e = 0.0f;
            lp->sphere_a = 0.0f;
            lp->sphere_r = 0.0f;
        }
    }

    return lp;
}
 
void lp_free(lightprobe *lp)
{
    printf("Close\n");

    if (lp)
    {
        free(lp);
    }
}
 
int lp_save(lightprobe *lp, const char *path)
{
    printf("Save %s\n", path);
    return 1;
}

/*----------------------------------------------------------------------------*/

int lp_export_cube(lightprobe *lp, const char *path)
{
    printf("Export Cube Map %s\n", path);
    return 1;
}

int lp_export_sphere(lightprobe *lp, const char *path)
{
    printf("Export Sphere Map %s\n", path);
    return 1;
}


/*----------------------------------------------------------------------------*/

void lp_append_image(lightprobe *lp, const char *path)
{
    printf("Append Image %s\n", path);
}

void lp_remove_image(lightprobe *lp, const char *path)
{
    printf("Remove Image %s\n", path);
}

/*----------------------------------------------------------------------------*/

void lp_set_image_flags(lightprobe *lp, const char *path, int f)
{
    printf("Set Image Flags %s\n", path);
}

int lp_get_image_flags(lightprobe *lp, const char *path)
{
    printf("Get Image Flags %s\n", path);
    return 0;
}

/*----------------------------------------------------------------------------*/

void lp_move_circle(lightprobe *lp, float x, float y, float r)
{
    printf("Circle %f %f %f\n", x, y, r);
}

void lp_move_sphere(lightprobe *lp, float e, float a, float r)
{
    printf("Sphere %f %f %f\n", e, a, r);
}

/*----------------------------------------------------------------------------*/

void lp_render_circle(lightprobe *lp, int f, int w, int h,
                      float x, float y, float e, float z)
{
}

void lp_render_sphere(lightprobe *lp, int f, int w, int h,
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

