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

#include "PhotoSynthConverter.h"

#include <PhotoSynthParser.h>
#include <PhotoSynthRadialUndistort.h>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/convenience.hpp>
#include <OgreString.h>

using namespace PhotoSynth;
namespace bf = boost::filesystem;

bool Converter::convert(const std::string& inputFolder)
{
	std::string jsonFilePath = Parser::createFilePath(inputFolder, Parser::jsonFilename);
	if (!bf::exists(jsonFilePath))
	{
		std::cout << "Error: " << Parser::jsonFilename << " file missing: you need to run PhotoSynthDownloader first !" << std::endl;
		return false;
	}
	
	std::string soapFilePath = Parser::createFilePath(inputFolder, Parser::soapFilename);
	if (!bf::exists(soapFilePath))
	{
		std::cout << "Error: " << Parser::soapFilename << " missing: you need to run PhotoSynthDownloader first !" << std::endl;
		return false;
	}

	std::string guidFilePath = Parser::createFilePath(inputFolder, Parser::guidFilename);
	if (!bf::exists(guidFilePath))
	{
		std::cout << "Error: " << Parser::guidFilename << " missing: you need to run PhotoSynthDownloader first !" << std::endl;
		return false;
	}
	std::string guid = Parser::getGuid(guidFilePath);

	std::stringstream path;
	path << inputFolder << "/pmvs/txt";

	if (!bf::exists(path.str()))
		bf::create_directory(path.str());

	path.str("");
	path << inputFolder << "/pmvs/visualize";
	if (!bf::exists(path.str()))
		bf::create_directory(path.str());

	path.str("");
	path << inputFolder << "/pmvs/models";
	if (!bf::exists(path.str()))
		bf::create_directory(path.str());

	path.str("");
	path << inputFolder << "/distort";
	if (!bf::exists(path.str()))
		bf::create_directory(path.str());

	Parser parser;
	parser.parseSoap(soapFilePath);
	parser.parseJson(jsonFilePath, guid);

	if (!scanDistortFolder(inputFolder, Parser::createFilePath(inputFolder, "distort"), &parser))
	{
		std::cout << "Error: " << mInputImages.size() << " images in your GUID/distort folder but " << parser.getJsonInfo().thumbs.size() << " referenced in this PhotoSynth"<< std::endl;
		return false;
	}
	
	const CoordSystem coord = parser.getCoordSystem(0);
	std::cout << parser.getNbCamera(0) << " pictures in CoordSystem 0" << std::endl;
	for (unsigned int i=0; i<coord.cameras.size(); ++i)
	{
		Camera cam = coord.cameras[i];
		int cameraIndex = cam.index;
		RadialUndistort::undistort(inputFolder, i, mInputImages[cameraIndex], cam);
		std::cout << "[" << (i+1) << "/" << coord.cameras.size() << "] " << mInputImages[cameraIndex] << " undistorted" << std::endl;
	}
	
	return true;
}

bool Converter::scanDistortFolder(const std::string& inputFolder, const std::string& distortFolder, Parser* parser)
{
	std::string filepath = Parser::createFilePath(inputFolder, "list.txt");
	std::ofstream output(filepath.c_str());
	
	bf::path path = bf::path(distortFolder);

	unsigned int counter = 0;
	bf::directory_iterator itEnd;

	//Caution: will iterate in lexico-order: thumbs_1.jpg < thumbs_10.jpg < thumbs_2.jpg
	//-> use padding to prevent this error : 00001.jpg > 00002.jpg > 00010.jpg
	for (bf::directory_iterator it(path); it != itEnd; ++it)
	{
		if (!bf::is_directory(it->status()))
		{
			std::string filename = it->filename();
			std::string extension = bf::extension(*it); //.JPG
			extension = extension.substr(1);
			Ogre::StringUtil::toLowerCase(extension); //jpg

			if (extension == "jpg")
			{	
				std::stringstream filepath;
				filepath << inputFolder << "distort\\" << filename;
				mInputImages.push_back(filepath.str());
				counter++;
				output << filepath.str() << std::endl;
			}
		}
	}
	output.close();

	if (mInputImages.size() != parser->getJsonInfo().thumbs.size())
		return false;

	return true;
}