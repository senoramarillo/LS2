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

#ifndef MD_MINMAX_ABS_H_INCLUDED
#define MD_MINMAX_ABS_H_INCLUDED 1

extern float md_minmax_abs_left;
extern float md_minmax_abs_middle_left;
extern float md_minmax_abs_middle_right;
extern float md_minmax_abs_right;

#if HAVE_POPT_H
extern struct poptOption md_minmax_abs_arguments[];
#endif

#if defined(STAND_ALONE)
#  define ALGORITHM_NAME "MD Min-Max (Absolute)"
#  define ALGORITHM_ARGUMENTS md_minmax_abs_arguments
#endif

#endif
