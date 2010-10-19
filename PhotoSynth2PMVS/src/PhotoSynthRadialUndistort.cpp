/*
	Copyright (c) 2010 ASTRE Henri (http://www.visual-experiments.com)

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in
	all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
	THE SOFTWARE.
*/

#include "PhotoSynthRadialUndistort.h"

#include <OgreVector3.h>
#include <OgreQuaternion.h>
#include <OgreMatrix3.h>
#include <jpeglib.h>

#include <PhotoSynthImage.h>

using namespace PhotoSynth;

void RadialUndistort::undistort(const std::string& inputFolder, unsigned int index, const std::string& filepath, const Camera& cam)
{
	Ogre::Vector2 dim = getJpegDimensions(filepath);	
	saveContourFile(inputFolder, index, dim, cam);
	saveUndistortImage(inputFolder, index, filepath, cam);
}

void RadialUndistort::saveUndistortImage(const std::string& inputFolder, unsigned int index, const std::string& filepath, const Camera& cam)
{
	char buf[10];
	sprintf(buf, "%08d", index);

	std::stringstream outFilepath;
	outFilepath << inputFolder << "/pmvs/visualize/" << buf << ".jpg";

	img_t *img = loadJpeg(filepath.c_str());
	int w = img->w;
	int h = img->h;

	img_t *img_out = img_new(w, h);
	double focal = cam.focal*std::max(w,h);
	double f2_inv = 1.0 / (focal*focal);

	for (int y = 0; y < h; y++) 
	{
		for (int x = 0; x < w; x++) 
		{
			double x_c = x - 0.5 * w;
			double y_c = y - 0.5 * h;

			double r2 = (x_c * x_c + y_c * y_c) * f2_inv;
			//double factor = 1.0 + camera.k[0] * r2 + camera.k[1] * r2 * r2;
			double factor = 1.0 + cam.distort2 * r2 + cam.distort1 * r2 * r2;

			x_c *= factor;
			y_c *= factor;

			x_c += 0.5 * w;
			y_c += 0.5 * h;

			fcolor_t c;
			if (x_c >= 0 && x_c < w - 1 && y_c >= 0 && y_c < h - 1) {
				c = pixel_lerp(img, x_c, y_c);
			} else {
				c = fcolor_new(0.0, 0.0, 0.0);
			}

			img_set_pixel(img_out, x, y, iround(c.r), iround(c.g), iround(c.b));
		}
	}

	writeJpeg(img_out, outFilepath.str());

	img_free(img);
	img_free(img_out);
}

void RadialUndistort::saveContourFile(const std::string& inputFolder, unsigned int index, Ogre::Vector2& dimension, const Camera& camera)
{
	char buf[10];
	sprintf(buf, "%08d", index);

	std::stringstream filepath;
	filepath << inputFolder << "/pmvs/txt/" << buf << ".txt";

	FILE *f = fopen(filepath.str().c_str(), "w");

	// Compute the projection matrix
	double focal = camera.focal*std::max(dimension.x, dimension.y);
	double R[9];
	double t[3];
	convertPhotoSynthRotToBundler(camera.orientation, R);
	convertPhotoSynthTransToBundler(camera.position, R, t);

	int w = (int) dimension[0];
	int h = (int) dimension[1];

	double K[9] = { -focal,   0.0, 0.5 * w - 0.5,
					   0.0, focal, 0.5 * h - 0.5,
					   0.0,   0.0,           1.0 };

	double Ptmp[12] = { R[0], R[1], R[2], t[0],
						R[3], R[4], R[5], t[1],
						R[6], R[7], R[8], t[2] };

	double P[12];
	matrix_product(3, 3, 3, 4, K, Ptmp, P);
	matrix_scale(3, 4, P, -1.0, P);

	fprintf(f, "CONTOUR\n");
	fprintf(f, "%0.6f %0.6f %0.6f %0.6f\n", P[0], P[1], P[2],  P[3]);
	fprintf(f, "%0.6f %0.6f %0.6f %0.6f\n", P[4], P[5], P[6],  P[7]);
	fprintf(f, "%0.6f %0.6f %0.6f %0.6f\n", P[8], P[9], P[10], P[11]);

	fclose(f);
}

Ogre::Vector2 RadialUndistort::getJpegDimensions(const std::string& filepath)
{
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr jerr;

	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_decompress(&cinfo);

	FILE *f;

	if ((f = fopen(filepath.c_str(), "rb")) == NULL) 
		return Ogre::Vector2::ZERO;

	jpeg_stdio_src(&cinfo, f);
	jpeg_read_header(&cinfo, TRUE);

	int w = cinfo.image_width;
	int h = cinfo.image_height;

	jpeg_destroy_decompress(&cinfo);

	fclose(f);

	return Ogre::Vector2((float)w, (float)h);
}

img_t* RadialUndistort::loadJpeg(const std::string& filepath)
{
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr jerr;

	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_decompress(&cinfo);

	FILE *f;

	if ((f = fopen(filepath.c_str(), "rb")) == NULL)
		return NULL;        

	jpeg_stdio_src(&cinfo, f);
	jpeg_read_header(&cinfo, TRUE);
	jpeg_start_decompress(&cinfo);

	int w = cinfo.output_width;
	int h = cinfo.output_height;
	int n = cinfo.output_components;

	assert(n == 1 || n == 3);

	img_t *img = img_new(w, h);
	JSAMPROW row = new JSAMPLE[n * w];

	for (int y = 0; y < h; y++) 
	{
		jpeg_read_scanlines(&cinfo, &row, 1);

		for (int x = 0; x < w; x++) 
		{
			if (n == 3) 
				img_set_pixel(img, x, h - y - 1, row[3 * x + 0], row[3 * x + 1], row[3 * x + 2]);

			else if (n == 1) 
				img_set_pixel(img, x, h - y - 1, row[x], row[x], row[x]);
		}
	}

	jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);

	delete [] row;

	fclose(f);

	return img;
}

void RadialUndistort::writeJpeg(const img_t *img, const std::string& filepath)
{
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;

	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);


	FILE *outfile;

	if ((outfile = fopen(filepath.c_str(), "wb")) == NULL)
		return;

	jpeg_stdio_dest(&cinfo, outfile);

	cinfo.image_width = img->w;     // image width and height, in pixels
	cinfo.image_height = img->h;
	cinfo.input_components = 3;     // # of color components per pixel
	cinfo.in_color_space = JCS_RGB; // colorspace of input image
	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, 98, TRUE);

	jpeg_start_compress(&cinfo, TRUE);

	JSAMPROW row = new JSAMPLE[3 * img->w];
	for (int y = 0; y < img->h; y++) 
	{
		// JSAMPROW row_pointer[1];      // pointer to a single row
		int row_stride;                 // physical row width in buffer
		row_stride = img->w * 3;        // JSAMPLEs per row in image_buffer

		for (int x = 0; x < img->w; x++) 
		{
			color_t c = img_get_pixel((img_t *) img, x, img->h - y - 1);
			row[3 * x + 0] = c.r;
			row[3 * x + 1] = c.g;
			row[3 * x + 2] = c.b;
		}

		jpeg_write_scanlines(&cinfo, &row, 1);
	}

	jpeg_finish_compress(&cinfo);
	jpeg_destroy_compress(&cinfo);

	delete [] row;

	fclose(outfile);
}

void RadialUndistort::convertPhotoSynthRotToBundler(const Ogre::Quaternion& q, double* R)
{
	Ogre::Matrix3 mx;
	mx.FromAxisAngle(Ogre::Vector3::UNIT_X, Ogre::Degree(180));

	Ogre::Matrix3 m;
	q.ToRotationMatrix(m);

	m = mx.Transpose()*m.Transpose(); // (A*B)' = B' * A'

	for (unsigned int i=0; i<3; ++i)
	{
		for (unsigned int j=0; j<3; ++j)
		{
			R[i*3+j] = m[i][j];
		}
	}
}

void RadialUndistort::convertPhotoSynthTransToBundler(const Ogre::Vector3& v, double* R, double* t)
{
	Ogre::Matrix3 m;
	for (unsigned int i=0; i<3; ++i)
	{
		for (unsigned int j=0; j<3; ++j)
		{
			m[i][j] = (float)R[i*3+j];
		}
	}
	Ogre::Vector3 v2 = m * v;

	for (unsigned int i=0; i<3; ++i)
	{
		t[i] = -v2[i];
	}
}

void RadialUndistort::matrix_scale(int m, int n, double *A, double s, double *R)
{
	int i;
	int entries = m * n;

	for (i = 0; i < entries; i++) 
	{
		R[i] = A[i] * s;
	}
}

void RadialUndistort::matrix_product(int Am, int An, int Bm, int Bn, const double *A, const double *B, double *R)
{
	int r = Am;
	int c = Bn;
	int m = An;

	int i, j, k;
	for (i = 0; i < r; i++) 
	{
		for (j = 0; j < c; j++) 
		{
			R[i * c + j] = 0.0;
			for (k = 0; k < m; k++) 
			{
				R[i * c + j] += A[i * An + k] * B[k * Bn + j];
			}
		}
	}
}