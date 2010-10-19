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

#include <OgreQuaternion.h>
#include <OgreVector3.h>
#include <OgreColourValue.h>

namespace PhotoSynth
{
	struct Camera
	{
		Camera();
		Camera(int index, Ogre::Quaternion orientation, Ogre::Vector3 position, float focal, float distort1, float distort2, float ratio);

		Ogre::Quaternion orientation;
		Ogre::Vector3    position;

		float focal;
		float distort1;
		float distort2;
		float ratio;

		int index;
	};

	struct Thumb
	{
		Thumb();
		Thumb(const std::string& url, int width, int height);

		std::string url;
		int width;
		int height;
	};

	struct Vertex
	{
		Vertex();
		Vertex(Ogre::Vector3 position, Ogre::ColourValue color = Ogre::ColourValue::Black);

		Ogre::ColourValue color;
		Ogre::Vector3 position;
	};

	struct VertexInfo
	{
		VertexInfo();
		VertexInfo(unsigned int vertexIndex, unsigned int value);

		unsigned int vertexIndex;
		unsigned int value;
	};

	struct PointCloud
	{
		std::vector<Vertex> vertices;
		std::vector<std::vector<VertexInfo>> infos; //one vector<VertexInfo> per image
	};

	struct CoordSystem
	{
		CoordSystem(unsigned int nbBinFile);
		CoordSystem(const std::vector<Camera>& cameras, const std::vector<PointCloud>& pointClouds = std::vector<PointCloud>());
		const Camera& getCamera(unsigned int index) const;

		unsigned int nbBinFile;
		std::vector<Camera> cameras;
		std::vector<PointCloud> pointClouds;		
	};
}