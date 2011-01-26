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

#include "PlyImporter.h"

#include <iostream>
#include <fstream>

std::vector<PhotoSynth::Vertex> PlyImporter::importPly(const std::string& filepath)
{
	std::vector<PhotoSynth::Vertex> vertices;

	Ogre::Vector3 noNormalValue(Ogre::Vector3::ZERO);
	Ogre::ColourValue noColorValue(0.f, 0.f, 0.f, 0.f);

	std::ifstream input;
	input.open(filepath.c_str(), std::ios::binary);

	if (!input.is_open())
		return vertices;

	std::string line;
	unsigned int nbVertices = 0;
	unsigned int nbTriangles = 0;
	bool isBinaryFile = false;
	bool hasNormals = false;
	bool hasColors  = false;
	bool hasAlpha   = false;

	//parse header
	do 
	{
		std::getline(input, line);

		//property list uchar int vertex_indices

		if (line == "format binary_little_endian 1.0")
			isBinaryFile = true;
		else if (line == "format ascii 1.0")
			isBinaryFile = false;
		else if (line == "property float nx")
			hasNormals = true;
		else if (line == "property uchar red")
			hasColors = true;
		else if (line == "property uchar alpha")
			hasAlpha = true;
		else if (line == "property uchar diffuse_red")
			hasColors = true;
		else if (line == "property uchar diffuse_alpha")
			hasAlpha = true;
		else
		{
			std::string keywordVertex("element vertex ");
			unsigned int lengthVertex = keywordVertex.size();

			std::string keywordFace("element face ");
			unsigned int lengthFace = keywordFace.size();

			if (line.size() > lengthVertex && line.substr(0, lengthVertex) == keywordVertex)
			{
				//std::string tmpVertex = line.substr(lengthVertex);
				nbVertices = atoi(line.substr(lengthVertex).c_str());
			}
			else if (line.size() > lengthFace && line.substr(0, lengthFace) == keywordFace)
			{
				//std::string tmpFace = line.substr(lengthFace);
				nbTriangles = atoi(line.substr(lengthFace).c_str());
			}
		}
	} while (line != "end_header"); //missing eof check...

	if (!isBinaryFile)
	{
		float* pos = new float[hasNormals?6:3];
		int* color = new int[hasAlpha?4:3];

		for (unsigned int i=0; i<nbVertices; ++i)
		{
			input >> pos[0]; //x
			input >> pos[1]; //y
			input >> pos[2]; //z
			if (hasNormals)
			{
				input >> pos[3]; //nx
				input >> pos[4]; //ny
				input >> pos[5]; //nz
			}
			if (hasColors)
			{
				input >> color[0]; //r
				input >> color[1]; //g
				input >> color[2]; //b
				if (hasAlpha)
					input >> color[3]; //a
			}

			Ogre::Vector3 position((Ogre::Real)pos[0], (Ogre::Real)pos[1], (Ogre::Real)pos[2]);
			Ogre::ColourValue colorValue = hasColors ? Ogre::ColourValue(color[0]/255.0f, color[1]/255.0f, color[2]/255.0f) : noColorValue;
			Ogre::Vector3 normal = hasNormals ? Ogre::Vector3((Ogre::Real)pos[3], (Ogre::Real)pos[4], (Ogre::Real)pos[5]) : noNormalValue;

			vertices.push_back(PhotoSynth::Vertex(position, colorValue));
		}

		unsigned char three;
		int indexA;
		int indexB;
		int indexC;
		for (unsigned int i=0; i<nbTriangles; ++i)
		{
			input >> three; //I know a triangle face is composed of 3 vertex ;-)
			input >>indexA;
			input >>indexB;
			input >>indexC;
		}
		delete[] pos;
		delete[] color;
	}
	else
	{
		float* pos = new float[hasNormals?6:3];
		size_t sizePos = sizeof(float)*(hasNormals?6:3);

		unsigned char* color = new unsigned char[hasAlpha?4:3];
		size_t sizeColor = sizeof(unsigned char)*(hasAlpha?4:3);

		for (unsigned int i=0; i<nbVertices; ++i)
		{
			input.read((char*)pos, sizePos);
			if (hasColors)
				input.read((char*)color, sizeColor);

			Ogre::Vector3 position((Ogre::Real)pos[0], (Ogre::Real)pos[1], (Ogre::Real)pos[2]);
			Ogre::ColourValue colorValue = hasColors ? Ogre::ColourValue(color[0]/255.0f, color[1]/255.0f, color[2]/255.0f) : noColorValue;
			Ogre::Vector3 normal = hasNormals ? Ogre::Vector3((Ogre::Real)pos[3], (Ogre::Real)pos[4], (Ogre::Real)pos[5]) : noNormalValue;
			vertices.push_back(PhotoSynth::Vertex(position, colorValue));
		}

		unsigned char three = 3;
		int indexes[3];
		for (unsigned int i=0; i<nbTriangles; ++i)
		{
			input.read((char*)&three, sizeof(three));
			input.read((char*)indexes, sizeof(indexes));
		}
		delete[] pos;
		delete[] color;
	}

	input.close();

	return vertices;
}