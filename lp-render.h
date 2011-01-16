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

int  lp_get_image_width (lightprobe *lp, int i);
int  lp_get_image_height(lightprobe *lp, int i);

/*----------------------------------------------------------------------------*/

enum
{
    LP_FLAG_LOADED = 1,
    LP_FLAG_ACTIVE = 2,
    LP_FLAG_HIDDEN = 4
};

void lp_set_image_flags(lightprobe *lp, int i, int f);
void lp_clr_image_flags(lightprobe *lp, int i, int f);
int  lp_get_image_flags(lightprobe *lp, int i);

/*----------------------------------------------------------------------------*/

enum
{
    LP_CIRCLE_X,
    LP_CIRCLE_Y,
    LP_CIRCLE_RADIUS,
    LP_SPHERE_ELEVATION,
    LP_SPHERE_AZIMUTH,
    LP_SPHERE_ROLL,
    LP_MAX_VALUE
};

void  lp_set_image_value(lightprobe *lp, int i, int k, float v);
float lp_get_image_value(lightprobe *lp, int i, int k);

/*----------------------------------------------------------------------------*/

enum
{
    LP_RENDER_GRID = 1,
    LP_RENDER_RES  = 2
};

void lp_render_circle(lightprobe *lp, int f, int w, int h,
                      float x, float y, float e, float z);
void lp_render_sphere(lightprobe *lp, int f, int w, int h,
                      float x, float y, float e, float z);

/*----------------------------------------------------------------------------*/

int lp_export_cube  (lightprobe *lp, const char *path);
int lp_export_dome  (lightprobe *lp, const char *path);
int lp_export_sphere(lightprobe *lp, const char *path);

/*----------------------------------------------------------------------------*/

#endif
