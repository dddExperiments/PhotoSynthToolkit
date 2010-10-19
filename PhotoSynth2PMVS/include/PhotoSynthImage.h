#pragma once

//Imported from Bundler (http://phototour.cs.washington.edu/bundler/)

/* 
 *  Copyright (c) 2008-2010  Noah Snavely (snavely (at) cs.cornell.edu)
 *    and the University of Washington
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */


typedef unsigned char u_int8_t;
typedef unsigned short u_int16_t;
typedef unsigned long u_int32_t;
typedef signed long int32_t;

typedef signed short int16_t;

/* 2D vector of doubles */
typedef struct 
{
	double p[2];
} v2_t;

typedef struct 
{
	u_int8_t r, g, b, extra;
} color_t;

typedef struct 
{
	float r, g, b;
} fcolor_t;

typedef struct 
{
	/* Width, height */
	u_int16_t w, h;

	/* Pixel data */
	color_t *pixels;

	/* Mask of valid pixels */
	u_int8_t *pixel_mask;

	/* The location of the image origin in world space */
	v2_t origin;
} img_t;


img_t *img_new(int w, int h);
void img_free(img_t *img);
fcolor_t fcolor_new(float r, float g, float b);

color_t img_get_pixel(img_t *img, int x, int y);
void img_set_pixel(img_t *img, int x, int y, u_int8_t r, u_int8_t g, u_int8_t b);
void img_set_valid_pixel(img_t *img, int x, int y);

fcolor_t pixel_lerp(img_t *img, double x, double y);
int iround(double x);