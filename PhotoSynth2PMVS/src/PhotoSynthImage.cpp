#include "PhotoSynthImage.h"

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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define BITS2BYTES(b) (((b) >> 3) + (((b) % 8) == 0 ? 0 : 1))
#define LERP(x0, x1, f0, f1, f2, f3) ((1.0 - (x1)) * ((1.0 - (x0)) * (f0) + (x0) * (f1)) + (x1) * ((1.0 - (x0)) * (f2) + (x0) * (f3)))

v2_t v2_new(double x, double y) 
{
	v2_t v = { { x, y } };
	return v;
}

fcolor_t fcolor_new(float r, float g, float b) 
{
	fcolor_t c = { r, g, b };
	return c;
}

int iround(double x)
{
	if (x < 0.0)
		return (int) (x - 0.5);
	else
		return (int) (x + 0.5);
}

/* Create a new image with the given width and height */
img_t *img_new(int w, int h) 
{
	img_t* img = (img_t*) malloc(sizeof(img_t));
	img->w = w; img->h = h;
	img->pixels = (color_t*) calloc(sizeof(color_t), w * h);
	img->origin = v2_new(0.0, 0.0);

	/* All pixels start out as invalid */
	img->pixel_mask = (u_int8_t *)calloc(BITS2BYTES(w * h), sizeof(u_int8_t));

	return img;
}

void img_free(img_t *img) 
{
	free(img->pixels);
	free(img->pixel_mask);
	free(img);
}

void img_set_pixel(img_t *img, int x, int y, u_int8_t r, u_int8_t g, u_int8_t b)
{
	color_t *p = img->pixels + y * img->w + x;

	p->r = r;
	p->g = g;
	p->b = b;
	p->extra = 0;

	/* Mark the pixel as valid */
	img_set_valid_pixel(img, x, y);
}

void img_set_valid_pixel(img_t *img, int x, int y) 
{
	int idx, byte, bit;
	if (x < 0 || y < 0 || x >= img->w || y >= img->h)
		printf("[img_set_valid_pixel] Error: pixel (%d, %d) out of range (%d, %d)\n", x, y, img->w, img->h);

	idx = y * img->w + x;
	byte = (idx >> 3);
	bit = idx - (byte << 3);

	img->pixel_mask[byte] |= (1 << bit);
}

color_t img_get_pixel(img_t *img, int x, int y) 
{
	/* Find a pixel inside the image */
	if (x < 0) 
		x = 0;
	else if (x >= img->w)
		x = img->w - 1;

	if (y < 0)
		y = 0;
	else if (y >= img->h)
		y = img->h -1;

	return img->pixels[y * img->w + x];
}

fcolor_t pixel_lerp(img_t *img, double x, double y) 
{
	int xf = (int) floor(x), yf = (int) floor(y);
	double xp = x - xf, yp = y - yf;

	//using bilinear interpolation
	double params[2] = { xp, yp };
	color_t pixels[4];
	fcolor_t col;
	double rd, gd, bd;

	pixels[0] = img_get_pixel(img, xf, yf);
	pixels[1] = img_get_pixel(img, xf + 1, yf);
	pixels[2] = img_get_pixel(img, xf, yf + 1);
	pixels[3] = img_get_pixel(img, xf + 1, yf + 1);

	rd = LERP(xp, yp, pixels[0].r, pixels[1].r, pixels[2].r, pixels[3].r); // nlerp2(params, fr);
	gd = LERP(xp, yp, pixels[0].g, pixels[1].g, pixels[2].g, pixels[3].g); // nlerp2(params, fr);
	bd = LERP(xp, yp, pixels[0].b, pixels[1].b, pixels[2].b, pixels[3].b); // nlerp2(params, fr);

	col.r = (float) rd;
	col.g = (float) gd;
	col.b = (float) bd;

	return col;
}