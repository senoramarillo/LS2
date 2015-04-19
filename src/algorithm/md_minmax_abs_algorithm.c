/*
  This file is part of LS² - the Localization Simulation Engine of FU Berlin.

  Copyright 2011-2013  Heiko Will, Marcel Kyas, Thomas Hillebrandt,
  Stefan Adler, Malte Rohde, Jonathan Gunthermann

  LS² is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  LS² is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with LS².  If not, see <http://www.gnu.org/licenses/>.

 */

#if HAVE_CONFIG_H
#  include "ls2/ls2-config.h"
#endif

#if HAVE_POPT_H
#  include <popt.h>
#endif

#include <assert.h>

/********************************************************************
 **
 **  This file is made only for including in the lib_lat project
 **  and not intended for stand alone usage!
 **
 ********************************************************************/

/*******************************************************************
 ***
 ***   MD Min Max Abs (Absolute error distribution)
 ***
 *******************************************************************/

/* @algorithm_name: MD MinMax (absolute) */

/*
static float md_minmax_abs_left         = -2.161;*(50.0/5.0);
static float md_minmax_abs_middle_left  = 1.6362*(50.0/5.0);
static float md_minmax_abs_middle_right = 1.6362*(50.0/5.0);
static float md_minmax_abs_right        = 16.0428*(50.0/5.0);
*/

static float md_minmax_abs_left         = -50.0f;
static float md_minmax_abs_middle_left  = 50.0f;
static float md_minmax_abs_middle_right = 50.0f;
static float md_minmax_abs_right        = 150.0f;

#if HAVE_POPT_H
struct poptOption md_minmax_abs_arguments[] = {
        { "mf-left", 0, POPT_ARG_FLOAT | POPT_ARGFLAG_SHOW_DEFAULT,
          &md_minmax_abs_left, 0,
          "left value of the membership function", NULL },
        { "mf-middle-left", 0, POPT_ARG_FLOAT | POPT_ARGFLAG_SHOW_DEFAULT,
          &md_minmax_abs_middle_left, 0,
          "middle left value of the membership function", NULL },
        { "mf-middle-right", 0, POPT_ARG_FLOAT | POPT_ARGFLAG_SHOW_DEFAULT,
          &md_minmax_abs_middle_right, 0,
          "middle right value of the membership function", NULL },
        { "mf-right", 0, POPT_ARG_FLOAT | POPT_ARGFLAG_SHOW_DEFAULT,
          &md_minmax_abs_right, 0,
          "right value of the membership function", NULL },
        POPT_TABLEEND
};
#endif


static inline void __attribute__((__always_inline__,__gnu_inline__,__nonnull__,__artificial__))
md_minmax_abs_run (const VECTOR* vx, const VECTOR* vy, const VECTOR *restrict r, size_t num_anchors, int width __attribute__((__unused__)), int height __attribute__((__unused__)), VECTOR *restrict resx, VECTOR *restrict resy) {

    const float left_rate      =
        (1.00F - 0.00F) / (md_minmax_abs_middle_left - md_minmax_abs_left);
    const float left_const     =
        -1.00F * left_rate * md_minmax_abs_left;
    const float right_rate     =
        (0.00F - 1.00F) / (md_minmax_abs_right - md_minmax_abs_middle_right);
    const float right_const    =
        -1.00F * right_rate * md_minmax_abs_right;

    const VECTOR v_left_rate   = VECTOR_BROADCASTF(left_rate);
    const VECTOR v_left_const  = VECTOR_BROADCASTF(left_const);
    const VECTOR v_right_rate  = VECTOR_BROADCASTF(right_rate);
    const VECTOR v_right_const = VECTOR_BROADCASTF(right_const);

    VECTOR vcount = VECTOR_BROADCASTF((float)num_anchors);

    VECTOR east  = vx[0] + r[0];
    VECTOR west  = vx[0] - r[0];
    VECTOR south = vy[0] + r[0];
    VECTOR north = vy[0] - r[0];
    
    for (size_t i = 1; i < num_anchors; i++) {
        east  = VECTOR_MIN(east,  vx[i] + r[i]);
        west  = VECTOR_MAX(west,  vx[i] - r[i]);
        south = VECTOR_MIN(south, vy[i] + r[i]);
        north = VECTOR_MAX(north, vy[i] - r[i]);
    }
    
    VECTOR grid_x[4];
    VECTOR grid_y[4];
    
    grid_x[0] = west;
    grid_y[0] = north;
    grid_x[1] = east;
    grid_y[1] = north;
    grid_x[2] = west;
    grid_y[2] = south;
    grid_x[3] = east;
    grid_y[3] = south;
        
    VECTOR grid_weight[4];
    VECTOR com_weights[4];

    for (size_t i = 0; i < 4; i++) {
        /* To calculate the standard deviation, we use Welfords method,
         * as described in TAOCP Vol. 2, 3rd ed., p. 232
         */
        VECTOR M_cur = zero, M_old;
	VECTOR S = zero, vk = one;
        for (size_t k = 0; k < num_anchors; k++, vk += one) {
            /* Compute weight */
            VECTOR tmp  = r[k] - distance(vx[k], vy[k], grid_x[i], grid_y[i]);
	    VECTOR down = v_left_rate * tmp + v_left_const;
	    VECTOR up   = v_right_rate * tmp + v_right_const;
	    grid_weight[i] = VECTOR_CLAMP(VECTOR_MIN(up, down), VECTOR_BROADCASTF(FLT_EPSILON), one);
            /* Compute mean and variance */
	    M_old = M_cur;
	    M_cur += (grid_weight[i] - M_old) / vk;
	    S += (grid_weight[i] - M_old) * (grid_weight[i] - M_cur);
        }
        VECTOR sdev = VECTOR_SQRT(S / (vcount - one));
        com_weights[i] = M_cur / VECTOR_MAX(sdev, VECTOR_BROADCASTF(FLT_EPSILON));
    }

    VECTOR ptsx = zero, ptsy = zero, mass = zero;
    for (size_t i = 0; i < 4;  i++) {
        ptsx += grid_x[i] * com_weights[i];
        ptsy += grid_y[i] * com_weights[i];
	mass += com_weights[i];
    }
    *resx = ptsx / mass;
    *resy = ptsy / mass;
}
