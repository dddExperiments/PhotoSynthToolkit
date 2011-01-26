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

#include <string>
#include <vector>
#include <boost/asio.hpp>
#include <OgreRoot.h>
#include <PhotoSynthParser.h>

namespace PhotoSynth
{
	struct TileInfo
	{	
		std::string url;
		std::string filePath;
		unsigned int i;
		unsigned int j;
		unsigned int size;
	};

	struct PictureInfo
	{
		std::string url;
		std::string filePath;
		unsigned int id;
		unsigned int width;
		unsigned int height;		
		unsigned int nbColumn;
		unsigned int nbRow;	
		unsigned int nbLevel;

		std::vector<TileInfo> tiles;
	};

	class TileDownloader
	{
		public:
			TileDownloader(boost::asio::io_service* service);
			~TileDownloader();

			void download(const std::string& projectFolder);

		protected:
			bool downloadCollection(const std::string& collectionFilePath, const std::string& url);
			void downloadPicture(unsigned int index);
			void downloadPictureTile(unsigned int pictureIndex, unsigned int tileIndex);

			void parseCollection(const std::string& collectionFilePath);			
			unsigned int getPOT(int value);
			void clearScreen();

			std::string mProjectPath;
			Ogre::Root* mRoot;
			Ogre::Log*  mLog;
			Ogre::LogManager* mLogManager;
			PhotoSynth::Parser* mParser;
			std::string mGuid;
			std::vector<PictureInfo> mPictures;
			boost::asio::io_service* mService;
	};
}