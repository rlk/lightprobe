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
lightprobe *lp_open(const char *path);
int         lp_save(lightprobe *lp, const char *path);
void        lp_free(lightprobe *lp);

/*----------------------------------------------------------------------------*/

int lp_export_cube  (lightprobe *lp, const char *path);
int lp_export_sphere(lightprobe *lp, const char *path);

/*----------------------------------------------------------------------------*/

int lp_append_image(lightprobe *lp, const char *path);
int lp_remove_image(lightprobe *lp, const char *path);

int lp_get_image_width (lightprobe *lp, const char *path);
int lp_get_image_height(lightprobe *lp, const char *path);

#define LP_IMAGE_DEFAULT 0
#define LP_IMAGE_ACTIVE  1
#define LP_IMAGE_HIDDEN  2

void lp_set_image_flags(lightprobe *lp, const char *path, int f);
void lp_clr_image_flags(lightprobe *lp, const char *path, int f);
int  lp_get_image_flags(lightprobe *lp, const char *path);

/*----------------------------------------------------------------------------*/

void lp_move_circle(lightprobe *lp, float x, float y, float r);
void lp_move_sphere(lightprobe *lp, float e, float a, float r);

/*----------------------------------------------------------------------------*/

#define LP_RENDER_DEFAULT 0
#define LP_RENDER_GRID    1
#define LP_RENDER_RES     2

void lp_render_circle(lightprobe *lp, int f, int w, int h,
                      float x, float y, float e, float z);
void lp_render_sphere(lightprobe *lp, int f, int w, int h,
                      float x, float y, float e, float z);

/*----------------------------------------------------------------------------*/

#endif
