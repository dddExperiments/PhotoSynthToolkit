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

#pragma once

#include <OgreVector2.h>
#include <PhotoSynthStructures.h>
#include <PhotoSynthImage.h>

namespace PhotoSynth
{
	class RadialUndistort //Imported from Bundler (http://phototour.cs.washington.edu/bundler/)
	{
		public:
			static void undistort(const std::string& inputFolder, unsigned int index, const std::string& filepath, const Camera& cam);

		protected:			
			static Ogre::Vector2 getJpegDimensions(const std::string& filepath);
			static void saveContourFile(const std::string& inputFolder, unsigned int index, Ogre::Vector2& dimension, const Camera& cam);
			static void saveUndistortImage(const std::string& inputFolder, unsigned int index, const std::string& filepath, const Camera& cam);

			static void convertPhotoSynthRotToBundler(const Ogre::Quaternion& q, double* R);
			static void convertPhotoSynthTransToBundler(const Ogre::Vector3& v, double* R, double* t);
			static void matrix_scale(int m, int n, double *A, double s, double *R);
			static void matrix_product(int Am, int An, int Bm, int Bn, const double *A, const double *B, double *R);
			
			static img_t* loadJpeg(const std::string& filepath);
			static void writeJpeg(const img_t *img, const std::string& filepath);
	};
}