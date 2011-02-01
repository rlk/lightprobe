/* GL-SPHERE Copyright (C) 2011 Robert Kooima                                 */
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

#ifndef GL_SPHERE_H
#define GL_SPHERE_H

/*----------------------------------------------------------------------------*/

typedef struct gl_sphere gl_sphere;

void gl_init_sphere(gl_sphere *, int, int);
void gl_free_sphere(gl_sphere *);

void gl_fill_globe(const gl_sphere *);
void gl_fill_chart(const gl_sphere *);
void gl_fill_polar(const gl_sphere *);

void gl_line_globe(const gl_sphere *);
void gl_line_chart(const gl_sphere *);
void gl_line_polar(const gl_sphere *);

/*----------------------------------------------------------------------------*/

#endif
