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
    LP_RENDER_GLOBE =   1,
    LP_RENDER_CHART =   2,
    LP_RENDER_POLAR =   4,
    LP_RENDER_CUBE  =   8,
    LP_RENDER_ALL   =  16,
    LP_RENDER_RES   =  32,
    LP_RENDER_GRID  =  64,
    LP_RENDER_ALPHA = 256,
};

void lp_export(lightprobe *lp, int f, int s, const char *path);
/*
void lp_render(lightprobe *lp, int f, int w, int h,
               float x, float y, float e, float z);
*/

void lp_render(lightprobe *lp, int f, int vx, int vy,
                                      int vw, int vh,
                                      int ww, int wh, float e);

/*----------------------------------------------------------------------------*/

unsigned int lp_load_texture(const char *path, int *w, int *h);
unsigned int lp_load_cubemap(const char *path);

/*----------------------------------------------------------------------------*/

#endif
