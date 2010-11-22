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

#include "PhotoSynthParser.h"
#include <OgreStringVector.h>

#include <tinyxml.h>
#include <json_spirit_reader.h>

using namespace PhotoSynth;

std::string Parser::jsonFilename = "0.json";
std::string Parser::soapFilename = "soap.xml";
std::string Parser::guidFilename = "guid.txt";

const SoapInfo& Parser::getSoapInfo() const
{
	return mSoapInfo;
}

const JsonInfo& Parser::getJsonInfo() const
{
	return mJsonInfo;
}

void Parser::parseSoap(const std::string& soapFilePath)
{
	TiXmlDocument doc;
	doc.LoadFile(soapFilePath);

	TiXmlHandle handle(&doc);
	TiXmlElement* parent = handle.FirstChildElement().FirstChildElement().FirstChildElement().FirstChildElement().Element();

	mSoapInfo.succeeded      =  parent->FirstChildElement("Result")->GetText() == "OK";
	mSoapInfo.collectionType = (parent->FirstChildElement("CollectionType")->GetText() == "Synth" ? SoapInfo::SYNTH : SoapInfo::PANORAMA);
	mSoapInfo.dzcUrl         =  parent->FirstChildElement("DzcUrl")->GetText();
	mSoapInfo.jsonUrl        =  parent->FirstChildElement("JsonUrl")->GetText();
	mSoapInfo.collectionRoot =  parent->FirstChildElement("CollectionRoot")->GetText();
	mSoapInfo.privacyLevel   =  parent->FirstChildElement("PrivacyLevel")->GetText();
}

void Parser::parseJson(const std::string& jsonFilePath, const std::string& guid)
{
	json_spirit::mValue json;
	json_spirit::read(readAsciiFile(jsonFilePath), json);

	json_spirit::mObject obj = json.get_obj();
	double version = obj["_json_synth"].get_real();

	json_spirit::mObject root;
	if (obj["l"].get_obj().find(guid) == obj["l"].get_obj().end()) //http://photosynth.net/view.aspx?cid=fea8cfff-2c8c-4317-8cec-06169f1bd367
		root = obj["l"].get_obj()[""].get_obj();
	else
		root = obj["l"].get_obj()[guid].get_obj(); //new synth

	int nbImage         = root["_num_images"].get_int();
	int nbCoordSystem   = root["_num_coord_systems"].get_int();
	std::string license = "";

	mJsonInfo.version = version;
	mJsonInfo.license = license;

	json_spirit::mObject thumbs = root["image_map"].get_obj();	
	
	//Retrieve thumb information
	for (int i=0; i<nbImage; ++i)
	{
		std::stringstream index; 
		index << i;

		std::string url = thumbs[index.str()].get_obj()["u"].get_str();

		json_spirit::mArray dim = thumbs[index.str()].get_obj()["d"].get_array();
		int width  = dim[0].get_int();
		int height = dim[1].get_int();		
		mJsonInfo.thumbs.push_back(Thumb(url, width, height));
	}	

	//Retrieve CoordSystem information
	for (int i=0; i<nbCoordSystem; ++i)
	{
		std::stringstream index; 
		index << i;
		
		json_spirit::mObject current = root["x"].get_obj()[index.str()].get_obj();

		if (current["k"].is_null())
		{
			mCoordSystems.push_back(CoordSystem(0));
		}
		else
		{
			int nbBinFile = current["k"].get_array()[1].get_int();
			json_spirit::mObject images = current["r"].get_obj();

			mCoordSystems.push_back(CoordSystem(nbBinFile));

			for (unsigned int j=0; j<images.size(); ++j)
			{
				std::stringstream iteratorIndex;
				iteratorIndex << j;

				json_spirit::mArray infos = images[iteratorIndex.str()].get_obj()["j"].get_array();
				int index    = infos[0].get_int();
				double x     = infos[1].get_real();
				double y     = infos[2].get_real();
				double z     = infos[3].get_real();
				double qx    = infos[4].get_real();
				double qy    = infos[5].get_real();
				double qz    = infos[6].get_real();
				double qw    = sqrt(1-qx*qx-qy*qy-qz*qz);
				double ratio = infos[7].get_real();
				double focal = infos[8].get_real();

				json_spirit::mArray distorts = images[iteratorIndex.str()].get_obj()["f"].get_array();
				double distort1 = distorts[0].get_real();
				double distort2 = distorts[1].get_real();

				Ogre::Quaternion orientation((float)qw, (float)qx, (float)qy, (float)qz);
				Ogre::Vector3 position((float)x, (float)y, (float)z);
				Camera cam(index, orientation, position, (float)focal, (float)distort1, (float)distort2, (float)ratio);

				mCoordSystems[i].cameras.push_back(cam);
			}
		}
	}
}

void Parser::parseBinFiles(const std::string& inputPath)
{
	for (unsigned int i=0; i<getNbCoordSystem(); ++i)
	{
		unsigned int nbPointCloud = getNbPointCloud(i);
		mCoordSystems[i].pointClouds = std::vector<PointCloud>(nbPointCloud);

		for (unsigned int j=0; j<nbPointCloud; ++j)
		{
			loadBinFile(inputPath, i, j);
		}
	}
}

unsigned int Parser::getNbCoordSystem() const
{
	return mCoordSystems.size();
}

const CoordSystem& Parser::getCoordSystem(unsigned int coordSystemIndex) const
{
	return mCoordSystems[coordSystemIndex];
}

unsigned int Parser::getNbVertex(unsigned int coordSystemIndex) const
{
	unsigned int nbVertex = 0;
	for (unsigned int i=0; i<getNbPointCloud(coordSystemIndex); ++i)
		nbVertex += getPointCloud(coordSystemIndex, i).vertices.size();

	return nbVertex;
}

unsigned int Parser::getNbCamera(unsigned int coordSystemIndex) const
{
	return mCoordSystems[coordSystemIndex].cameras.size();
}

const Camera& Parser::getCamera(unsigned int coordSystemIndex, unsigned int cameraIndex) const
{
	return mCoordSystems[coordSystemIndex].cameras[cameraIndex];
}

unsigned int Parser::getNbPointCloud(unsigned int coordSystemIndex) const
{
	return mCoordSystems[coordSystemIndex].nbBinFile; //hack to avoid loading of bin file
}

const PointCloud& Parser::getPointCloud(unsigned int coordSystemIndex, unsigned int pointCloudIndex) const
{
	return mCoordSystems[coordSystemIndex].pointClouds[pointCloudIndex];
}

std::string Parser::createFilePath(const std::string& path, const std::string& filename)
{
	std::stringstream filepath;
	filepath << path << "/" << filename;

	return filepath.str();
}

std::string Parser::readAsciiFile(const std::string& filepath)
{
	std::stringstream content;

	std::ifstream input(filepath.c_str());
	std::string line;
	while(std::getline(input, line))
		content << line << std::endl;
	input.close();

	return content.str();
}

std::string Parser::getGuid(const std::string& filepath)
{
	std::string line;
	std::ifstream input(filepath.c_str());
	std::getline(input, line);
	input.close();
	Ogre::StringUtil::toLowerCase(line);

	return line;
}

int Parser::ReadCompressedInt(std::istream& input)
{
	int i = 0;
	unsigned char b;

	do
	{
		input.read((char*)&b, sizeof(b));
		i = (i << 7) | (b & 127);
	} while (b < 128);

	return i;
}

float Parser::ReadBigEndianSingle(std::istream& input)
{
	unsigned char b1[4];
	unsigned char b2[4];

	input.read((char*)b1, sizeof(b1));
	b2[0] = b1[3];
	b2[1] = b1[2];
	b2[2] = b1[1];
	b2[3] = b1[0];

	float result = *((float*) b2);
	return 	result;
}

unsigned int Parser::ReadBigEndianUInt16(std::istream& input)
{
	unsigned int value;

	unsigned char b1, b2;
	input.read((char*)&b1, sizeof(b1));
	input.read((char*)&b2, sizeof(b2));

	value = (b2 | b1 << 8);

	return value;
}

bool Parser::loadBinFile(const std::string& inputPath, unsigned int coordSystemIndex, unsigned int binFileIndex)
{
	std::stringstream filename;
	filename << inputPath << "/bin/points_" << coordSystemIndex << "_" << binFileIndex << ".bin";

	std::ifstream input;
	input.open(filename.str().c_str(), std::ios::binary);
	if (input.is_open())
	{
		unsigned int versionMajor = ReadBigEndianUInt16(input);
		unsigned int versionMinor = ReadBigEndianUInt16(input);

		if (versionMajor != 1 || versionMinor != 0)
		{
			input.close();
			return false;
		}

		int nbImage = ReadCompressedInt(input);
		mCoordSystems[coordSystemIndex].pointClouds[binFileIndex].infos = std::vector<std::vector<VertexInfo>>(nbImage);
		for (int i=0; i<nbImage; i++)
		{
			int nbInfo = ReadCompressedInt(input);
			mCoordSystems[coordSystemIndex].pointClouds[binFileIndex].infos[i] = std::vector<VertexInfo>(nbInfo);

			for (int j=0; j<nbInfo; j++)
			{
				int vertexIndex = ReadCompressedInt(input);
				int vertexValue = ReadCompressedInt(input);

				mCoordSystems[coordSystemIndex].pointClouds[binFileIndex].infos[i][j] = VertexInfo(vertexIndex, vertexValue);
			}
		}

		int nbVertex = ReadCompressedInt(input);
		mCoordSystems[coordSystemIndex].pointClouds[binFileIndex].vertices = std::vector<Vertex>(nbVertex);

		for (int i=0; i<nbVertex; i++)
		{
			float x = ReadBigEndianSingle(input);
			float y = ReadBigEndianSingle(input);
			float z = ReadBigEndianSingle(input);
			Ogre::Vector3 position(x, y, z);

			unsigned int color = ReadBigEndianUInt16(input);

			unsigned char r = (unsigned char) (((color >> 11) * 255) / 31);
			unsigned char g = (unsigned char) ((((color >> 5) & 63) * 255) / 63);
			unsigned char b = (unsigned char) (((color & 31) * 255) / 31);
			Ogre::ColourValue colorValue(r/255.0f, g/255.0f, b/255.0f, 1.0f);

			mCoordSystems[coordSystemIndex].pointClouds[binFileIndex].vertices[i] = Vertex(position, colorValue);
		}
	}
	input.close();

	return true;
}