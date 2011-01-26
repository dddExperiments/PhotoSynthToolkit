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

#include "PhotoSynthTileDownloader.h"

#include <PhotoSynthDownloadHelper.h>
#include <boost/filesystem/operations.hpp>
#include <boost/threadpool.hpp>
#include <OgreRoot.h>
#include <OgreCodec.h>
#include <tinyxml.h>
#include <CanvasTexture.h>

using namespace PhotoSynth;
namespace bf = boost::filesystem;

TileDownloader::TileDownloader(boost::asio::io_service* service)
{
	mService = service;
	mParser = new PhotoSynth::Parser;

	mLogManager = new Ogre::LogManager();
	mLog  = mLogManager->createLog("Ogre.log", false, false);
	mRoot = new Ogre::Root("plugins.cfg", "ogre.cfg");
	Ogre::LogManager::getSingleton().getDefaultLog()->setDebugOutputEnabled(false);
}

TileDownloader::~TileDownloader()
{
	delete mParser;

	mRoot->shutdown();
	delete mRoot;
	mRoot = NULL;
	mLogManager->destroyLog(mLog);
	delete mLogManager;
}

void TileDownloader::download(const std::string& projectFolder)
{
	mProjectPath = projectFolder;

	if (!bf::exists(projectFolder))
	{
		std::cout << projectFolder << " not found" << std::endl;
		return;
	}

	std::stringstream path;
	path << projectFolder << "hd";
	if (!bf::exists(path.str()))
		bf::create_directory(path.str());

	path.str("");
	path << projectFolder << "hd/tmp";
	if (!bf::exists(path.str()))
		bf::create_directory(path.str());
	Ogre::ResourceGroupManager::getSingleton().addResourceLocation(path.str(), "FileSystem", Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);

	std::stringstream guidPath;
	guidPath << projectFolder << "guid.txt";

	if (!bf::exists(guidPath.str()))
	{
		std::cout << guidPath.str() << " not found" << std::endl;
		return;
	}

	mGuid = PhotoSynth::Parser::getGuid(guidPath.str());
	
	std::string soapFilePath = Parser::createFilePath(projectFolder, Parser::soapFilename);
	if (!bf::exists(soapFilePath))
	{
		std::cout << soapFilePath << " not found" << std::endl;
		return;
	}
	
	mParser->parseSoap(soapFilePath);

	std::string jsonFilePath = Parser::createFilePath(projectFolder, Parser::jsonFilename);
	if (!bf::exists(jsonFilePath))
	{
		std::cout << jsonFilePath << " not found" << std::endl;
		return;
	}

	mParser->parseJson(jsonFilePath, mGuid);

	std::cout << "[" << mParser->getNbCamera(0) << " cameras found in Synth " << mGuid << "]" << std::endl;
	std::cout << "[Downloading Synth Collection information...]";
	std::string collectionFilePath = Parser::createFilePath(projectFolder, "collection.xml");
	if (!bf::exists(collectionFilePath))
		downloadCollection(collectionFilePath, mParser->getSoapInfo().dzcUrl);

	parseCollection(collectionFilePath);
	clearScreen();
	std::cout << "[Synth Collection Downloaded]" << std::endl;

	
	for (unsigned int i=0; i<mPictures.size(); ++i)
	{
		int percent = (int)(((i+1)*100.0f) / (1.0f*mPictures.size()));
		if (!bf::exists(mPictures[i].filePath))
			downloadPicture(i);
		clearScreen();
		std::cout << "[Downloading picture : " << percent << "%] - ("<<i+1<<"/"<<mPictures.size()<<" "<< mPictures[i].width <<" x " << mPictures[i].height << ")";
	}
	clearScreen();
	std::cout << "[Picture downloaded]" << std::endl;

	path.str("");
	path << projectFolder << "thumbs/00000000.jpg";
	if (bf::exists(path.str())) //test if thumbs are available
	{
		for (unsigned int i=0; i<mPictures.size(); ++i)
		{
			std::stringstream paddedIndex;
			paddedIndex.width(8);
			paddedIndex.fill('0');
			paddedIndex << i;

			std::stringstream thumbPath;			
			thumbPath << projectFolder << "thumbs/" << paddedIndex.str() << ".jpg";
			
			std::stringstream hdPath;
			hdPath << projectFolder << "hd/" << paddedIndex.str() << ".jpg";

			std::stringstream command;
			command << "jhead -te " << thumbPath.str() << " " << hdPath.str();

			if (bf::exists(thumbPath.str()) && bf::exists(hdPath.str()))
				system(command.str().c_str());
		}
	}

	path.str("");
	path << projectFolder << "hd/tmp";
	bf::remove_all(path.str());
}

void TileDownloader::downloadPicture(unsigned int index)
{
	const PictureInfo& info = mPictures[index];
	
	//Downloading all tiles in parallel
	{
		boost::threadpool::pool tp(info.tiles.size());
		for (unsigned int i=0; i<info.tiles.size(); ++i)
			tp.schedule(boost::bind(&TileDownloader::downloadPictureTile, this, index, i));
	}
	//compose all tiles on the canvas
	unsigned int width  = info.width;
	unsigned int height = info.height;
	Ogre::Canvas::Context* ctx = new Ogre::Canvas::Context(width, height, false);
	for (unsigned int i=0; i<info.tiles.size(); ++i)
	{
		const TileInfo& tileInfo = info.tiles[i];
		Ogre::Image img;
		std::stringstream filename;
		filename << tileInfo.j << "_" << tileInfo.i << ".jpg";
		img.load(filename.str(), Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);		
		float x = (tileInfo.i == 0) ? 0 : 509.0f + (tileInfo.i-1)*510.0f;
		float y = (tileInfo.j == 0) ? 0 : 509.0f + (tileInfo.j-1)*510.0f;
		ctx->drawImage(img, x, y);
	}
	
	//save canvas to jpeg
	ctx->saveToFile(info.filePath);

	delete ctx;
}

void TileDownloader::downloadPictureTile(unsigned int pictureIndex, unsigned int tileIndex)
{
	TileInfo& tileInfo = mPictures[pictureIndex].tiles[tileIndex];
	std::string url      = tileInfo.url;
	std::string filepath = tileInfo.filePath;

	std::string host = DownloadHelper::extractHost(url);
	SocketPtr sock = DownloadHelper::connect(mService, host);

	std::string header = DownloadHelper::createGETHeader(DownloadHelper::removeHost(url, host), host);

	//send GET request
	boost::system::error_code error;
	boost::asio::write(*sock, boost::asio::buffer(header.c_str()), boost::asio::transfer_all(), error);

	//read the response (header + content)
	boost::asio::streambuf response;
	while (boost::asio::read(*sock, response, boost::asio::transfer_at_least(1), error));

	std::istream response_stream(&response);
	int length = DownloadHelper::getContentLength(response_stream);
	tileInfo.size = (unsigned int) length;

	DownloadHelper::saveBinFile(filepath, response_stream, length);			

	sock->close();
}

bool TileDownloader::downloadCollection(const std::string& collectionFilePath, const std::string& collectionUrl)
{
	try
	{
		std::string host = DownloadHelper::extractHost(collectionUrl);
		SocketPtr sock = DownloadHelper::connect(mService, host);

		std::string header = DownloadHelper::createGETHeader(DownloadHelper::removeHost(collectionUrl, host), host);

		//send GET request
		boost::system::error_code error;
		boost::asio::write(*sock, boost::asio::buffer(header.c_str()), boost::asio::transfer_all(), error);

		//read the response (header + content)
		boost::asio::streambuf response;
		while (boost::asio::read(*sock, response, boost::asio::transfer_at_least(1), error));

		sock->close();

		//skip header from response
		std::istream response_stream(&response);
		std::string line;
		while (std::getline(response_stream, line) && line != "\r");

		//put collection into collectionText
		std::stringstream collectionText;
		while (std::getline(response_stream, line))
			collectionText << line << std::endl;

		//save collection
		if (!DownloadHelper::saveAsciiFile(collectionFilePath, collectionText.str()))
			return false;
	}
	catch (std::exception& e)
	{		
		std::cout << e.what() << std::endl;
		return false;
	}
	return true;
}

void TileDownloader::parseCollection(const std::string& collectionFilePath)
{
	TiXmlDocument doc;
	doc.LoadFile(collectionFilePath);

	TiXmlHandle handle(&doc);
	TiXmlElement* parent = handle.FirstChildElement().FirstChildElement().Element();
	
	mPictures.clear();

	for (TiXmlNode* elt = parent->FirstChild("I"); elt; elt = elt->NextSibling("I"))
	{
		TiXmlElement* size = elt->FirstChild("Size")->ToElement();

		PictureInfo pictureInfo;		
		std::string url      = elt->ToElement()->Attribute("Source");
		pictureInfo.id       = atoi(elt->ToElement()->Attribute("Id"));;
		pictureInfo.width    = atoi(size->Attribute("Width"));
		pictureInfo.height   = atoi(size->Attribute("Height"));
		pictureInfo.nbColumn = (unsigned int) Ogre::Math::Ceil(pictureInfo.width/510.0f);
		pictureInfo.nbRow    = (unsigned int) Ogre::Math::Ceil(pictureInfo.height/510.0f);
		pictureInfo.nbLevel  = getPOT(std::max(pictureInfo.width, pictureInfo.height));
				
		std::stringstream replacement;
		replacement << "_files/" << pictureInfo.nbLevel << "/";
		url = Ogre::StringUtil::replaceAll(url, ".dzi", replacement.str());
		pictureInfo.url = url;



		std::stringstream pictureFilePath;
		pictureFilePath << mProjectPath << "hd/";
		pictureFilePath.width(8);
		pictureFilePath.fill('0');
		pictureFilePath << pictureInfo.id << ".jpg";
		pictureInfo.filePath = pictureFilePath.str();
				
		for (unsigned int i=0; i<pictureInfo.nbColumn; ++i)
		{
			for (unsigned int j=0; j<pictureInfo.nbRow; ++j)
			{
				std::stringstream tileUrl;
				tileUrl <<  url << i << "_" << j << ".jpg";

				std::stringstream tileFilePath;
				tileFilePath << mProjectPath << "hd/tmp/" << j << "_" << i << ".jpg";

				TileInfo tileInfo;
				tileInfo.i = i;
				tileInfo.j = j;
				tileInfo.url  = tileUrl.str();
				tileInfo.filePath = tileFilePath.str();
				pictureInfo.tiles.push_back(tileInfo);
			}
		}
		mPictures.push_back(pictureInfo);
	}
}

unsigned int TileDownloader::getPOT(int value) 
{
	unsigned int pot = 0;
	int currentValue = 1;
	do
	{
		pot++;
		currentValue *=2;
	}
	while (currentValue < value);

	return pot;
}

void TileDownloader::clearScreen()
{
	std::cout << "\r                                                                          \r";
}