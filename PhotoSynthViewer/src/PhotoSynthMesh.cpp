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

#include "PhotoSynthMesh.h"

using namespace PhotoSynth;

Vertex::Vertex(Ogre::Vector3 position, Ogre::ColourValue color, Ogre::Vector3 normal)
{
	this->position = position;
	this->color    = color;
	this->normal   = normal;
}

Triangle::Triangle(unsigned int indexA, unsigned int indexB, unsigned int indexC)
{
	this->indexA = indexA;
	this->indexB = indexB;
	this->indexC = indexC;
}

Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<Triangle>& triangles)
{
	this->vertices = vertices;
	this->triangles = triangles;
}