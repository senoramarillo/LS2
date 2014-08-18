/*
  This file is part of LS² - the Localization Simulation Engine of FU Berlin.

  Copyright 2011-2013  Heiko Will, Marcel Kyas, Thomas Hillebrandt,
  Stefan Adler, Malte Rohde, Jonathan Gunthermann, Paul Podlech

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

/********************************************************************
 **
 **  This file is made only for including in the LS² project
 **  and not desired for stand alone usage!
 **
 ********************************************************************/

/* @algorithm_name: Maximum Likelihood Estimator (Gamma) */

/*******************************************************************
 ***
 ***   Maximum Likelihood Estimator (Gamma);
 ***
 *******************************************************************/

#ifndef MLE_GAMMA_ALGORITHM_C_INCLUDED
#define MLE_GAMMA_ALGORITHM_C_INCLUDED 1

#if HAVE_CONFIG_H
#  include "ls2/ls2-config.h"
#endif

#include <glib.h>
#include <gsl/gsl_multimin.h>

#include "mle_gamma_algorithm.h"

#include "algorithm/nllsq_algorithm.c"

#ifndef MLE_GAMMA_DEFAULT_SHAPE
#define MLE_GAMMA_DEFAULT_SHAPE 3.0
#endif

#ifndef MLE_GAMMA_DEFAULT_RATE
#define MLE_GAMMA_DEFAULT_RATE (MLE_GAMMA_DEFAULT_SHAPE / 50.0)
#endif

#ifndef MLE_GAMMA_DEFAULT_OFFSET
#define MLE_GAMMA_DEFAULT_OFFSET 25.0
#endif

#ifndef MLE_GAMMA_DEFAULT_EPSILON
#define MLE_GAMMA_DEFAULT_EPSILON 1e-2
#endif

#ifndef MLE_GAMMA_DEFAULT_ITERATIONS
#define MLE_GAMMA_DEFAULT_ITERATIONS 100
#endif

static double mle_gamma_shape      = MLE_GAMMA_DEFAULT_SHAPE;
static double mle_gamma_rate       = MLE_GAMMA_DEFAULT_RATE;
static double mle_gamma_offset     = MLE_GAMMA_DEFAULT_OFFSET;
static double mle_gamma_epsilon    = MLE_GAMMA_DEFAULT_EPSILON;
static    int mle_gamma_iterations = MLE_GAMMA_DEFAULT_ITERATIONS;



static GOptionEntry mle_gamma_arguments[] = {
        { "mle-gamma-rate", 0, 0, G_OPTION_ARG_DOUBLE,
          &mle_gamma_rate,
          "rate of the gamma distribution", NULL },
        { "mle-gamma-shape", 0, 0, G_OPTION_ARG_DOUBLE,
          &mle_gamma_shape,
          "shape of the gamma distribution", NULL },
        { "mle-gamma-offset", 0, 0, G_OPTION_ARG_DOUBLE,
          &mle_gamma_offset,
          "offset to the gamma distribution", NULL },
        { "mle-gamma-epsilon", 0, 0, G_OPTION_ARG_DOUBLE,
          &mle_gamma_epsilon,
          "maximum size of the simplex for termination", NULL },
        { "mle-gamma-iterations", 0, 0, G_OPTION_ARG_INT,
          &mle_gamma_iterations,
          "maximum number of iterations before termination", NULL },
        { NULL }
};



void __attribute__((__nonnull__))
ls2_add_mle_gamma_option_group(GOptionContext *context)
{
     GOptionGroup *group;
     group = g_option_group_new("mle-gamma",
                                "Parameters to the MLE Gamma algorithm",
                                "Parameters to the MLE Gamma algorithm",
                                NULL, NULL);
     g_option_group_add_entries(group, mle_gamma_arguments);
     g_option_context_add_group(context, group);
}


struct mle_gamma_point2d {
    double x;
    double y;
};

/* This structure holds the parameters to the likelihood function. */
struct mle_gamma_params {
    struct mle_gamma_point2d *anchors;
    double *ranges;
    size_t no_anchors;
    double gammaval;
    double factor;
};


static double
__attribute__((__nonnull__,__flatten__))
mle_gamma_likelihood_function(const gsl_vector *restrict X, void *restrict params)
{
    double result = 0.0;
    const struct mle_gamma_params *const p = params;
    const double thetaX = gsl_vector_get(X, 0);
    const double thetaY = gsl_vector_get(X, 1);

    for (size_t j = 0; j < p->no_anchors; j++) {
        const double d = distance_sf(thetaX, thetaY, p->anchors[j].x, p->anchors[j].y);
        const double Z = p->ranges[j] + mle_gamma_offset - d;
        if (__builtin_expect(Z <= 0.0, 0)) {
            result = INFINITY; // If a measurement is too short, return this.
            break;
        }
        const double likelihood =
            log(p->factor * pow(Z, mle_gamma_shape - 1.0)) - mle_gamma_rate * Z;
        result -= likelihood;
    }
#if !defined(NDEBUG)
    fprintf(stderr, "f(%f, %f) = %f\n", thetaX, thetaY, result);
#endif
    return result;
}



static void
__attribute__((__nonnull__,__flatten__))
mle_gamma_likelihood_gradient(const gsl_vector *restrict X, void *restrict params,
                              gsl_vector *restrict g)
{
    const struct mle_gamma_params *const p = params;
    const double thetaX = gsl_vector_get(X, 0);
    const double thetaY = gsl_vector_get(X, 1);
    double gradX = 0.0, gradY = 0.0;

    for (size_t j = 0; j < p->no_anchors; j++) {
        if (thetaX != p->anchors[j].x && thetaY != p->anchors[j].y) {
            const double d2 = distance_squared_sf(thetaX, thetaY, p->anchors[j].x, p->anchors[j].y);
            const double d = sqrt(d2);
            const double Z = p->ranges[j] + mle_gamma_offset - d;

            if (__builtin_expect(Z <= 0, 0)) {
                /* If this is the case, the initial guess does not have
                   a likelihood associated to it. */
                gradX = NAN;
                gradY = NAN;
                break;
            }
            const double t =
                (mle_gamma_rate * (d - Z - mle_gamma_offset) + mle_gamma_shape - 1.0) /
                (d * (d - Z - mle_gamma_offset));
            gradX -= (thetaX -  p->anchors[j].x) * t;
            gradY -= (thetaY -  p->anchors[j].y) * t;
        } // TODO: Otherwise the value of the gradient component is 0?
    }
#if !defined(NDEBUG)
    fprintf(stderr, "df(%f, %f) = (%f, %f)\n", thetaX, thetaY, gradX, gradY);
#endif
    gsl_vector_set(g, 0, gradX);
    gsl_vector_set(g, 1, gradY);
}




static void
__attribute__((__nonnull__,__flatten__,__hot__,__used__))
mle_gamma_likelihood_fdf(const gsl_vector *X, void *restrict params,
                         double *restrict y, gsl_vector *restrict g)
{
    *y = mle_gamma_likelihood_function(X, params);
    mle_gamma_likelihood_gradient(X, params, g);
}



static inline void
__attribute__((__always_inline__,__gnu_inline__,__artificial__,__nonnull__))
mle_gamma_run(const VECTOR* vx, const VECTOR* vy, const VECTOR *restrict r,
              size_t no_anchors,
              int width __attribute__((__unused__)),
              int height __attribute__((__unused__)),
              VECTOR *restrict resx, VECTOR *restrict resy)
{
    /* Step 0: Set up the likelihood function. */
    struct mle_gamma_params p;
    struct mle_gamma_point2d anchors[no_anchors];
    double ranges[no_anchors];
    p.no_anchors = no_anchors;
    p.anchors = anchors;
    for (size_t i = 0; i < no_anchors; i++) {
        p.anchors[i].x = vx[i][0];
        p.anchors[i].y = vy[i][0];
    }
    p.ranges = ranges;
    p.gammaval = tgamma(mle_gamma_shape);
    p.factor = pow(mle_gamma_rate, mle_gamma_shape) / p.gammaval;

    gsl_multimin_function_fdf fdf;
    fdf.n = 2u;
    fdf.f = &mle_gamma_likelihood_function;
    fdf.df = &mle_gamma_likelihood_gradient;
    fdf.fdf = &mle_gamma_likelihood_fdf;
    fdf.params = &p;

    

    /* Step: Call the optimiser. */
    const gsl_multimin_fdfminimizer_type *T =
        gsl_multimin_fdfminimizer_vector_bfgs2;
    gsl_multimin_fdfminimizer *s = gsl_multimin_fdfminimizer_alloc(T, 2);

    gsl_vector *x = gsl_vector_alloc(2);

    VECTOR sx, sy;

    // Calculate the initial guess.
    nllsq_run(vx, vy, r, no_anchors, width, height, &sx, &sy);

    for (int i = 0; i < VECTOR_OPS; i++) {
        /* Step 2a: Check whether the initial guess can be used. */
        if (__builtin_expect(isnan(sx[i]) || isnan(sy[i]), 0)) {
#if !defined(NDEBUG)
            fprintf(stderr, "initial guess is (%f, %f)\n", sx[i], sy[i]);
#endif
            (*resx)[i] = NAN;
            (*resy)[i] = NAN;
            continue;
        }

        /* Step 2b: Initialize the parameters. */
        for (size_t j = 0; j < no_anchors; j++) {
            p.ranges[j] = r[j][i];
        }
            
        gsl_vector_set(x, 0, sx[i]);
        gsl_vector_set(x, 1, sy[i]);

        /* Step 2c: Check for the validity of the likelihood value. */
        double likelihood = mle_gamma_likelihood_function(x, &p);
#if !defined(NDEBUG)
        fprintf(stderr, "likelihood = %f\n", likelihood);
#endif
        if (__builtin_expect(isinf(likelihood), 0)) {
            /* If this is the case, the initial guess does not have
               a likelihood associated to it. We set the estimated coordinate
               to an undefined value and continue. */
            (*resx)[i] = NAN;
            (*resy)[i] = NAN;
            continue;
        }

        /* Step 2d: Iterate the minimization algorithm. */
        int iter = 0;
        int status;
        gsl_multimin_fdfminimizer_set(s, &fdf, x, 1e-2, 1e-4);

        do {
            iter++;
            status = gsl_multimin_fdfminimizer_iterate(s);
            if (__builtin_expect(status, 0)) // Not zero if there was an error.
                break;
            status =
                gsl_multimin_test_gradient (s->gradient, mle_gamma_epsilon);
        } while (status == GSL_CONTINUE && iter < mle_gamma_iterations);

        /* Step 2c: Store the result. */
        (*resx)[i] = (float) gsl_vector_get(s->x, 0);
        (*resy)[i] = (float) gsl_vector_get(s->x, 1);
    }

    /* Step 3: Clean up. */
    gsl_multimin_fdfminimizer_free(s);
    gsl_vector_free(x);
}

#endif
