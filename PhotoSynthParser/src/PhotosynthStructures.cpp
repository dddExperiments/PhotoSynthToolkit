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

#include "PhotosynthStructures.h"

using namespace PhotoSynth;

Camera::Camera()
{
	orientation = Ogre::Quaternion::IDENTITY;
	position    = Ogre::Vector3::ZERO;
	focal       = 0.0f;
	distort1    = 0.0f;
	distort2    = 0.0f;
	ratio       = 0.0f;

	index       = -1;
}

Camera::Camera(int index, Ogre::Quaternion orientation, Ogre::Vector3 position, float focal, float distort1, float distort2, float ratio)
{
	this->orientation = orientation;
	this->position    = position;
	this->focal       = focal;
	this->distort1    = distort1;
	this->distort2    = distort2;
	this->ratio       = ratio;
	this->index       = index;
}

Thumb::Thumb()
{
	url    = "";
	width  = 0;
	height = 0;
}

Thumb::Thumb(const std::string& url, int width, int height)
{
	this->url    = url;
	this->width  = width;
	this->height = height;
}

Vertex::Vertex()
{
	this->position = Ogre::Vector3::ZERO;
	this->color    = Ogre::ColourValue::Black;
}

Vertex::Vertex(Ogre::Vector3 position, Ogre::ColourValue color)
{
	this->position = position;
	this->color    = color;
}

VertexInfo::VertexInfo()
{
	this->vertexIndex = 0;
	this->value       = 0;
}

VertexInfo::VertexInfo(unsigned int vertexIndex, unsigned int value)
{
	this->vertexIndex = vertexIndex;
	this->value       = value;
}

CoordSystem::CoordSystem(unsigned int nbBinFile)
{
	this->nbBinFile = nbBinFile;
}

CoordSystem::CoordSystem(const std::vector<Camera>& cameras, const std::vector<PointCloud>& pointClouds)
{
	this->cameras     = cameras;
	this->pointClouds = pointClouds;
	this->nbBinFile   = 0;
}

const Camera& CoordSystem::getCamera(unsigned int index) const
{
	for (unsigned int i=0; i<cameras.size(); ++i)
	{
		if (cameras[i].index == index)
			return cameras[i];
	}

	static Camera cam;
	return cam;
}