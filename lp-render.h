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

#ifndef LP_RENDER_H
#define LP_RENDER_H

/*----------------------------------------------------------------------------*/

typedef struct lightprobe lightprobe;

/*----------------------------------------------------------------------------*/

lightprobe *lp_init();
void        lp_tilt(lightprobe *lp);
void        lp_free(lightprobe *lp);

/*----------------------------------------------------------------------------*/

int  lp_add_image(lightprobe *lp, const char *path);
void lp_del_image(lightprobe *lp, int);
void lp_sel_image(lightprobe *lp, int);

/*----------------------------------------------------------------------------*/

enum
{
    LP_CIRCLE_X,
    LP_CIRCLE_Y,
    LP_CIRCLE_RADIUS,
    LP_SPHERE_ELEVATION,
    LP_SPHERE_AZIMUTH,
    LP_SPHERE_ROLL,
    LP_MAX_VALUE,
    LP_SPHERE_X,
    LP_SPHERE_Y,
    LP_SPHERE_Z,
    LP_SPHERE_W
};

int   lp_get_width (lightprobe *lp);
int   lp_get_height(lightprobe *lp);
float lp_get_value (lightprobe *lp, int k);
void  lp_set_value (lightprobe *lp, int k, float v);

/*----------------------------------------------------------------------------*/

enum
{
    LP_RENDER_GRID  = 1,
    LP_RENDER_RES   = 2,
    LP_RENDER_ALL   = 4,
    LP_RENDER_ALPHA = 8
};

void lp_render_circle(lightprobe *lp, int f, int w, int h,
                      float x, float y, float e, float z);
void lp_render_sphere(lightprobe *lp, int f, int w, int h,
                      float x, float y, float e, float z);
/*
void lp_render_img(lightprobe *lp, int f, int W, int H, float e, float z,
                                                        float x, float y);
void lp_render_env(lightprobe *lp, int f, int W, int H, float e, float k,
                                      float x, float y, float z, float w);
*/
/*----------------------------------------------------------------------------*/

void lp_export_cube(lightprobe *lp, const char *path, int s, int f);
void lp_export_dome(lightprobe *lp, const char *path, int s, int f);
void lp_export_rect(lightprobe *lp, const char *path, int s, int f);

/*----------------------------------------------------------------------------*/

unsigned int lp_load_texture(const char *path, int *w, int *h);
unsigned int lp_load_cubemap(const char *path);

/*----------------------------------------------------------------------------*/

#endif
