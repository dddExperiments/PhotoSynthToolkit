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

#include "PhotoSynthDownloader.h"

#include <boost/filesystem/operations.hpp>
#include <OgreStringVector.h>
#include <OgreMatrix3.h>

using namespace PhotoSynth;
namespace bf = boost::filesystem;

using boost::asio::ip::tcp;

Downloader::Downloader(boost::asio::io_service* service)
{
	mService                = service;
	mTransmissionBufferSize = 16384;
	mTransmissionBuffer     = new unsigned char[mTransmissionBufferSize];
}

Downloader::~Downloader()
{
	delete[] mTransmissionBuffer;
}

bool Downloader::download(const std::string& guid, const std::string& outputFolder, bool downloadThumb)
{	
	saveAsciiFile(Parser::createFilePath(outputFolder, Parser::guidFilename), guid);

	Parser parser;

	if (!bf::exists(outputFolder))
		bf::create_directory(outputFolder);

	std::stringstream path;
	path << outputFolder << "/bin";

	if (!bf::exists(path.str()))
		bf::create_directory(path.str());

	path.str("");
	path << outputFolder << "/pmvs";
	if (!bf::exists(path.str()))
		bf::create_directory(path.str());

	path.str("");
	path << outputFolder << "/distort";
	if (!bf::exists(path.str()))
		bf::create_directory(path.str());

	if (downloadThumb)
	{
		path.str("");
		path << outputFolder << "/thumbs";
		if (!bf::exists(path.str()))
			bf::create_directory(path.str());
	}

	std::string soapFilePath = Parser::createFilePath(outputFolder, Parser::soapFilename);
	if (!bf::exists(soapFilePath))
	{
		if (!downloadSoap(soapFilePath, guid))
		{
			std::cout << "Error while downloading Soap request" << std::endl;
			return false;
		}
	}

	parser.parseSoap(soapFilePath);

	std::string jsonFilePath = Parser::createFilePath(outputFolder, Parser::jsonFilename);
	if (!bf::exists(jsonFilePath))
	{		
		if (!downloadJson(jsonFilePath, parser.getSoapInfo().jsonUrl))
		{
			std::cout << "Error while downloading JSON file" << std::endl;
			return false;
		}
	}

	parser.parseJson(jsonFilePath, guid);
	saveCamerasParameters(outputFolder, &parser);

	downloadAllBinFiles(outputFolder, &parser);

	parser.parseBinFiles(outputFolder);

	//stats
	std::cout << "PhotoSynth composed of " << parser.getJsonInfo().thumbs.size() << " pictures and " << parser.getNbCoordSystem() << " CoordSystems:" << std::endl;
	for (unsigned int i=0; i<parser.getNbCoordSystem(); ++i)
		std::cout << "[" << i<< "]: " << parser.getNbCamera(i) << " cameras, " << parser.getNbVertex(i) << " points" <<std::endl;

	if (downloadThumb)
		downloadAllThumbFiles(outputFolder, parser.getJsonInfo());

	savePly(outputFolder, &parser);

	return true;
}

bool Downloader::downloadSoap(const std::string& soapFilePath, const std::string& guid)
{
	try
	{
		SocketPtr sock = connect("www.photosynth.net");

		//prepare header + content
		std::stringstream content;
		content << "<?xml version=\"1.0\" encoding=\"utf-8\"?>"<<std::endl;
		content << "<soap12:Envelope xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" xmlns:soap12=\"http://www.w3.org/2003/05/soap-envelope\">"<<std::endl;
		content << "	<soap12:Body>"<<std::endl;
		content << "		<GetCollectionData xmlns=\"http://labs.live.com/\">"<<std::endl;
		content << "		<collectionId>"<<guid<<"</collectionId>"<<std::endl;
		content << "		<incrementEmbedCount>false</incrementEmbedCount>"<<std::endl;
		content << "		</GetCollectionData>"<<std::endl;
		content << "	</soap12:Body>"<<std::endl;
		content << "</soap12:Envelope>"<<std::endl;

		std::stringstream header;
		header << "POST /photosynthws/PhotosynthService.asmx HTTP/1.1"<<std::endl;
		header << "Host: photosynth.net"<<std::endl;
		header << "Content-Type: application/soap+xml; charset=utf-8"<<std::endl;
		header << "Content-Length: "<<content.str().size()<<std::endl;
		header << ""<<std::endl;

		//send header + content
		boost::system::error_code error;
		boost::asio::write(*sock, boost::asio::buffer(header.str().c_str()), boost::asio::transfer_all(), error);
		boost::asio::write(*sock, boost::asio::buffer(content.str().c_str()), boost::asio::transfer_all(), error);

		sock->shutdown(tcp::socket::shutdown_send);

		//read the response
		boost::asio::streambuf response;
		while (boost::asio::read(*sock, response, boost::asio::transfer_at_least(1), error));

		sock->close();

		std::istream response_stream(&response);
		std::string line;

		//skip header from response
		while (std::getline(response_stream, line) && line != "\r");		

		//put soap response into xml
		std::stringstream xml;
		while (std::getline(response_stream, line))
			xml << line << std::endl;

		//save soap response
		if (!saveAsciiFile(soapFilePath, xml.str()))
			return false;		
	}
	catch (std::exception& e)
	{		
		std::cout << e.what() << std::endl;
		return false;
	}
	return true;
}

bool Downloader::downloadJson(const std::string& jsonFilePath, const std::string& jsonUrl)
{
	try
	{
		std::string host = extractHost(jsonUrl);
		SocketPtr sock = connect(host);

		std::string header = createGETHeader(removeHost(jsonUrl, host), host);

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

		//put json into jsonText
		std::stringstream jsonText;
		while (std::getline(response_stream, line))
			jsonText << line << std::endl;

		//save json
		if (!saveAsciiFile(jsonFilePath, jsonText.str()))
			return false;
	}
	catch (std::exception& e)
	{		
		std::cout << e.what() << std::endl;
		return false;
	}
	return true;
}

void Downloader::downloadAllBinFiles(const std::string& outputFolder, Parser* parser)
{
	std::string host = extractHost(parser->getSoapInfo().collectionRoot);

	SocketPtr sock = connect(host);
	boost::asio::socket_base::keep_alive keepAlive(true);
	sock->set_option(keepAlive);

	std::stringstream headers;
	std::vector<std::string> filenames;

	int nbCoorSystem = parser->getNbCoordSystem();

	for (unsigned int i=0; i<parser->getNbCoordSystem(); ++i)
	{
		int nbPointCloud = parser->getNbPointCloud(i);
		for (unsigned int j=0; j<parser->getNbPointCloud(i); ++j)
		{
			std::stringstream filename;
			filename << "bin/points_"<< i << "_" << j << ".bin";
			if (!bf::exists(Parser::createFilePath(outputFolder, filename.str())))
			{
				filenames.push_back(filename.str());
				std::stringstream url;
				url << parser->getSoapInfo().collectionRoot << filename.str().substr(4);
				headers << createGETHeader(removeHost(url.str(), host), host);
			}
		}
	}

	if (filenames.size() > 0)
	{
		//send GET request
		boost::system::error_code error;
		boost::asio::write(*sock, boost::asio::buffer(headers.str().c_str()), boost::asio::transfer_all(), error);

		//read the response (header + content)
		boost::asio::streambuf response;
		while (boost::asio::read(*sock, response, boost::asio::transfer_at_least(1), error));

		//read all response header and content
		std::istream response_stream(&response);
		for (unsigned int i=0; i<filenames.size(); ++i)
		{
			int length = getContentLength(response_stream);
			saveBinFile(Parser::createFilePath(outputFolder, filenames[i]), response_stream, length);
		}
	}

	sock->close();
}

void Downloader::downloadAllThumbFiles(const std::string& outputFolder, const JsonInfo& info)
{
	std::cout << std::endl;
	std::cout << "Downloading " << info.thumbs.size() << " thumbs" << std::endl;
	for (unsigned int i=0; i<info.thumbs.size(); ++i)
	{
		char buf[10];
		sprintf(buf, "%08d", i);

		std::stringstream filename;
		filename << "thumbs/" << buf << ".jpg";

		std::string filepath = Parser::createFilePath(outputFolder, filename.str());
		if (!bf::exists(filepath))
		{
			std::string host = extractHost(info.thumbs[i].url);
			SocketPtr sock = connect(host);

			std::string header = createGETHeader(removeHost(info.thumbs[i].url, host), host);

			//send GET request
			boost::system::error_code error;
			boost::asio::write(*sock, boost::asio::buffer(header.c_str()), boost::asio::transfer_all(), error);

			//read the response (header + content)
			boost::asio::streambuf response;
			while (boost::asio::read(*sock, response, boost::asio::transfer_at_least(1), error));

			std::istream response_stream(&response);
			int length = getContentLength(response_stream);

			saveBinFile(filepath, response_stream, length);			

			sock->close();
			std::cout << "[" << (i+1) << "/" << info.thumbs.size() << "] " << filename.str() << " downloaded" << std::endl;
		}
	}
}

void Downloader::savePly(const std::string& outputFolder, Parser* parser)
{
	for (unsigned int i=0; i<parser->getNbCoordSystem(); ++i)
	{		
		std::stringstream filepath;
		filepath << outputFolder << "/bin/coord_system_" << i << ".ply";

		if (!bf::exists(filepath.str()))
		{
			std::ofstream output(filepath.str().c_str());
			if (output.is_open())
			{
				output << "ply" << std::endl;
				output << "format ascii 1.0" << std::endl;
				output << "element vertex " << parser->getNbVertex(i) << std::endl;
				output << "property float x" << std::endl;
				output << "property float y" << std::endl;
				output << "property float z" << std::endl;
				output << "property uchar red" << std::endl;
				output << "property uchar green" << std::endl;
				output << "property uchar blue" << std::endl;
				output << "element face 0" << std::endl;
				output << "property list uchar int vertex_indices" << std::endl;
				output << "end_header" << std::endl;

				for (unsigned int j=0; j<parser->getNbPointCloud(i); ++j)
				{
					const PointCloud& pointCloud = parser->getPointCloud(i, j);
					for (unsigned int k=0; k<pointCloud.vertices.size(); ++k)
					{
						Ogre::Vector3 pos = pointCloud.vertices[k].position;
						Ogre::ColourValue color = pointCloud.vertices[k].color;
						output << pos.x << " " << pos.y << " " << pos.z << " " << (int)(color.r*255.0f) << " " << (int)(color.g*255.0f) << " " << (int)(color.b*255.0f) << std::endl;
					}				
				}
			}
			output.close();
		}

		filepath.str("");
		filepath << outputFolder << "/bin/coord_system_" << i << "_with_cameras.ply";

		if (!bf::exists(filepath.str()))
		{
			std::ofstream output(filepath.str().c_str());
			if (output.is_open())
			{
				output << "ply" << std::endl;
				output << "format ascii 1.0" << std::endl;
				output << "element vertex " << parser->getNbVertex(i) + parser->getNbCamera(i) << std::endl;
				output << "property float x" << std::endl;
				output << "property float y" << std::endl;
				output << "property float z" << std::endl;
				output << "property uchar red" << std::endl;
				output << "property uchar green" << std::endl;
				output << "property uchar blue" << std::endl;
				output << "element face 0" << std::endl;
				output << "property list uchar int vertex_indices" << std::endl;
				output << "end_header" << std::endl;

				for (unsigned int j=0; j<parser->getNbPointCloud(i); ++j)
				{
					const PointCloud& pointCloud = parser->getPointCloud(i, j);
					for (unsigned int k=0; k<pointCloud.vertices.size(); ++k)
					{
						Ogre::Vector3 pos = pointCloud.vertices[k].position;
						Ogre::ColourValue color = pointCloud.vertices[k].color;
						output << pos.x << " " << pos.y << " " << pos.z << " " << (int)(color.r*255.0f) << " " << (int)(color.g*255.0f) << " " << (int)(color.b*255.0f) << std::endl;
					}
				}

				for (unsigned int j=0; j<parser->getNbCamera(i); ++j)
				{
					Ogre::Vector3 pos = parser->getCamera(i, j).position;
					output << pos.x << " " << pos.y << " " << pos.z << " 255 255 0" << std::endl;
				}
			}
			output.close();
		}
	}
}

//url should be like http://toto.net/index.html -> return toto.net
std::string Downloader::extractHost(const std::string& url)
{
	Ogre::StringVector tmp = Ogre::StringUtil::split(url, "/");
	if (tmp.size() < 2)
		return "";
	else
		return tmp[1];
}

//url should be like http://toto.net/index.html -> return /index.html (host is optional and sould be like toto.net)
std::string Downloader::removeHost(const std::string& url, const std::string& host)
{
	std::string domain = (host == "" ? extractHost(url) : host);

	return url.substr(domain.size()+std::string("http://").size());
}

SocketPtr Downloader::connect(const std::string& url)
{
	tcp::resolver resolver(*(mService));
	tcp::resolver::query query(url, "http");

	tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
	tcp::resolver::iterator end;

	SocketPtr socket = SocketPtr(new tcp::socket(*(mService)));
	boost::system::error_code error = boost::asio::error::host_not_found;
	while (error && endpoint_iterator != end)
	{
		socket->close();
		socket->connect(*endpoint_iterator++, error);
	}
	if (error)
		throw boost::system::system_error(error);

	return socket;
}

std::string Downloader::createGETHeader(const std::string& get, const std::string& host)
{
	std::stringstream header;
	header << "GET " << get << " HTTP/1.1" <<std::endl;
	header << "Host: " << host <<std::endl;
	header << "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8"<<std::endl;
	header << "Accept-Language: fr,fr-fr;q=0.8,en-us;q=0.5,en;q=0.3"<<std::endl;
	header << "Accept-Charset: ISO-8859-1,utf-8;q=0.7,*;q=0.7"<<std::endl;
	header << "Connection: keep-alive"<<std::endl;
	header << ""<<std::endl;

	return header.str();
}

unsigned int Downloader::getContentLength(std::istream& header)
{
	//example: "Content-Length: 3789" -> 3789

	int length = 0;
	std::string contentLength = "Content-Length: ";
	std::string line;
	while (std::getline(header, line) && line != "\r")
	{
		std::string name = line.substr(0, contentLength.size());
		if (name == contentLength)
			length = atoi(Ogre::StringUtil::split(line, contentLength)[0].c_str());
	}
	return length;
}

bool Downloader::saveAsciiFile(const std::string& filepath, const std::string& content)
{
	std::ofstream output;
	output.open(filepath.c_str());

	bool opened = output.is_open();

	if (opened)
		output << content;
	output.close();

	return opened;
}

bool Downloader::saveBinFile(const std::string& filepath, std::istream& input, unsigned int length)
{
	char* buffer = new char[length];
	std::ofstream output;
	output.open(filepath.c_str(), std::ios::binary);
	input.read(buffer, length);
	output.write(buffer, length);
	output.close();

	delete[] buffer;

	return true;
}

void Downloader::saveCamerasParameters(const std::string& outputFolder, Parser* parser)
{
	for (unsigned int i=0; i<parser->getNbCoordSystem(); ++i)
	{
		unsigned int nbCamera = parser->getJsonInfo().thumbs.size();
		std::vector<const Camera*> cameras(nbCamera);
		memset(&cameras[0], NULL, sizeof(Camera*)*nbCamera);

		unsigned int nbCameraInCoordSystem = parser->getNbCamera(i);
		for (unsigned int j=0; j<nbCameraInCoordSystem; ++j)
		{
			const PhotoSynth::Camera& cam = parser->getCamera(i, j);
			cameras[cam.index] = &cam;
		}

		std::stringstream filepath;
		filepath << outputFolder << "/bin/coord_system_" << i << "_cameras.txt";

		std::ofstream output(filepath.str().c_str());
		if (output.is_open())
		{
			for (unsigned int j=0; j<nbCamera; ++j)
			{
				if (cameras[j])
				{
					const Camera* cam = cameras[j];
					output << cam->focal << " " << cam->distort1 << " " << cam->distort2 << std::endl;
					
					Ogre::Matrix3 rot;
					cam->orientation.ToRotationMatrix(rot);

					output << rot[0][0] << " " << rot[0][1] << " " << rot[0][2] << std::endl;
					output << rot[1][0] << " " << rot[1][1] << " " << rot[1][2] << std::endl;
					output << rot[2][0] << " " << rot[2][1] << " " << rot[2][2] << std::endl;

					output << cam->position.x << " " << cam->position.y << " " << cam->position.z << std::endl;
				}
				else
				{
					//focal k1 k2
					output << "0 0 0" << std::endl;
					
					//rotation
					output << "0 0 0" << std::endl;
					output << "0 0 0" << std::endl;
					output << "0 0 0" << std::endl;
					
					//translation
					output << "0 0 0" << std::endl;
				}				
			}
		}
		output.close();
	}
}