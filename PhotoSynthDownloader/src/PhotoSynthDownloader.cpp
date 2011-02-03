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

#include <boost/threadpool.hpp>
#include <boost/filesystem/operations.hpp>
#include <OgreStringVector.h>
#include <OgreMatrix3.h>

using namespace PhotoSynth;
namespace bf = boost::filesystem;

using boost::asio::ip::tcp;

Downloader::Downloader(boost::asio::io_service* service)
{
	mService = service;
}

bool Downloader::download(const std::string& guid, const std::string& outputFolder, bool downloadThumb)
{	
	DownloadHelper::saveAsciiFile(Parser::createFilePath(outputFolder, Parser::guidFilename), guid);

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
	save3DSMaxScript(outputFolder, &parser);

	return true;
}

bool Downloader::downloadSoap(const std::string& soapFilePath, const std::string& guid)
{
	try
	{
		SocketPtr sock = DownloadHelper::connect(mService, "www.photosynth.net");

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
		if (!DownloadHelper::saveAsciiFile(soapFilePath, xml.str()))
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
		std::string host = DownloadHelper::extractHost(jsonUrl);
		SocketPtr sock = DownloadHelper::connect(mService, host);

		std::string header = DownloadHelper::createGETHeader(DownloadHelper::removeHost(jsonUrl, host), host);

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
		if (!DownloadHelper::saveAsciiFile(jsonFilePath, jsonText.str()))
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
	std::string host = DownloadHelper::extractHost(parser->getSoapInfo().collectionRoot);

	SocketPtr sock = DownloadHelper::connect(mService, host);
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
				headers << DownloadHelper::createGETHeader(DownloadHelper::removeHost(url.str(), host), host);
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
			int length = DownloadHelper::getContentLength(response_stream);
			DownloadHelper::saveBinFile(Parser::createFilePath(outputFolder, filenames[i]), response_stream, length);
		}
	}

	sock->close();
}

void Downloader::downloadAllThumbFiles(const std::string& outputFolder, const JsonInfo& info)
{
	std::cout << std::endl;
	std::cout << "Downloading " << info.thumbs.size() << " thumbs" << std::endl;

	boost::threadpool::pool tp(8);
	for (unsigned int i=0; i<info.thumbs.size(); ++i)
		tp.schedule(boost::bind(&Downloader::downloadThumb, this, outputFolder, info, i));
}

void Downloader::downloadThumb(const std::string& outputFolder, const JsonInfo& info, unsigned int index)
{
	char buf[10];
	sprintf(buf, "%08d", index);

	std::stringstream filename;
	filename << "thumbs/" << buf << ".jpg";

	std::string filepath = Parser::createFilePath(outputFolder, filename.str());
	if (!bf::exists(filepath))
	{
		std::string host = DownloadHelper::extractHost(info.thumbs[index].url);
		SocketPtr sock = DownloadHelper::connect(mService, host);

		std::string header = DownloadHelper::createGETHeader(DownloadHelper::removeHost(info.thumbs[index].url, host), host);

		//send GET request
		boost::system::error_code error;
		boost::asio::write(*sock, boost::asio::buffer(header.c_str()), boost::asio::transfer_all(), error);

		//read the response (header + content)
		boost::asio::streambuf response;
		while (boost::asio::read(*sock, response, boost::asio::transfer_at_least(1), error));

		std::istream response_stream(&response);
		int length = DownloadHelper::getContentLength(response_stream);

		DownloadHelper::saveBinFile(filepath, response_stream, length);			

		sock->close();
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
				output << "element vertex " << parser->getNbVertex(i) + 2*parser->getNbCamera(i) << std::endl;
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
					const PhotoSynth::Camera cam = parser->getCamera(i, j);
					Ogre::Vector3 pos = cam.position;

					if ((j % 2) == 0)
						output << pos.x << " " << pos.y << " " << pos.z << " 0 255 0" << std::endl;
					else
						output << pos.x << " " << pos.y << " " << pos.z << " 255 0 0" << std::endl;

					Ogre::Vector3 offset(0.0f, -0.05f, 0.0f);					
					Ogre::Vector3 p = pos + cam.orientation.Inverse() * offset;
					output << p.x << " " << p.y << " " << p.z << " 255 255 0" << std::endl;
				}
			}
			output.close();
		}
	}
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
			output << nbCamera << std::endl;
			for (unsigned int j=0; j<nbCamera; ++j)
			{
				if (cameras[j])
				{
					const Camera* cam = cameras[j];
					output << cam->focal << " " << cam->distort1 << " " << cam->distort2 << std::endl;
					
					Ogre::Matrix3 rot;
					cam->orientation.ToRotationMatrix(rot);

					Ogre::Matrix3 Rx;
					Rx.FromAxisAngle(Ogre::Vector3::UNIT_X, Ogre::Degree(180.0f));
					rot = Rx.Transpose() * rot.Transpose();

					output << rot[0][0] << " " << rot[0][1] << " " << rot[0][2] << std::endl;
					output << rot[1][0] << " " << rot[1][1] << " " << rot[1][2] << std::endl;
					output << rot[2][0] << " " << rot[2][1] << " " << rot[2][2] << std::endl;

					Ogre::Vector3 position = - rot * cam->position;
					output << position.x << " " << position.y << " " << position.z << std::endl;
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

//This function generate a 3DS Max script (ported from modified version of SynthExport created by Josh Harle in C#)
//http://blog.neonascent.net/archives/cameraexport-photosynth-to-camera-projection-in-3ds-max/
void Downloader::save3DSMaxScript(const std::string& outputFolder, Parser* parser)
{
	if (parser->getNbCoordSystem() > 0)
	{
		std::stringstream filepath;
		filepath << outputFolder << "/bin/cameras.ms";

		std::ofstream output(filepath.str().c_str());
		if (output.is_open())
		{
			output << "/* 3DS Max Camera and Projection Map Exporter by Josh Harle (http://tacticalspace.org) */" << std::endl;
			output << "/*  Enable initial camera states below; 1 = enabled, 0 = disabled */" << std::endl;
			output << std::endl;
			output << "if queryBox \"Before starting, remember to added the folder containing your images (e.g. /pmvs/visualize) to 3DS Max path list using Customize -> Configure-User-Paths -> External Files -> Add.  Have you done this?\" beep:true then (" << std::endl;
			output << std::endl;
			output << "Default_Mix_Type = 2 /*  0 = Additive, 1 = Subtractive, 2 = Mix */" << std::endl;
			output << "Default_Mix_Amount = 100" << std::endl;
			output << "Default_Angle_Threshold = 90" << std::endl;

			for (unsigned int i=0; i<parser->getNbCamera(0); ++i)
				output << "Enable_Camera_" << i  << " = 1" << std::endl;

			output << std::endl;
			output << "progressstart \"Adding Cameras and Camera Maps\"" << std::endl;
			output << std::endl;
			output << "startCamera = matrix3 [1,0,0] [0,-1,0] [0,0,-1] [0,0,0]" << std::endl;
			output << "sideCamera = matrix3 [0,1,0] [1,0,0] [0,0,-1] [0,0,0]" << std::endl;
			output << "t = matrix3 [1,0,0] [0,1,0] [0,0,1] [0,0,0]" << std::endl;
			output << "R = matrix3 [1,0,0] [0,1,0] [0,0,1] [0,0,0]" << std::endl;
			output << std::endl;

			int materialNum = 1;
			int compositeNum = 1;
			std::stringstream compositeMaterial;
			compositeMaterial << "";

			for (unsigned int i=0; i<parser->getNbCamera(0); ++i)
			{
				std::stringstream paddedIndex; //contain %8d of i (42 -> 00000042)
				paddedIndex.width(8);
				paddedIndex.fill('0');
				paddedIndex << i;

				output << "progressupdate (100.0*" << i << "/" << parser->getNbCamera(0) << ")" << std::endl;

				output << "Camera" << i << " = freecamera name: \"" << i << "\"" << std::endl;
				
				unsigned int precision = output.precision();
				output.precision(10);
				output << "Camera" << i << ".fov = cameraFOV.MMtoFOV " << (35 * parser->getCamera(0, i).focal) << std::endl;

				const PhotoSynth::Camera& cam = parser->getCamera(0, i);				
				Ogre::Matrix3 rot;
				Ogre::Vector3 pos = cam.position;
				cam.orientation.ToRotationMatrix(rot);
				
				output << "R.row1 = [" << rot[0][0] << ", " << rot[1][0] << ", " << rot[2][0] << "]" << std::endl;
				output << "R.row2 = [" << rot[0][1] << ", " << rot[1][1] << ", " << rot[2][1] << "]" << std::endl;
				output << "R.row3 = [" << rot[0][2] << ", " << rot[1][2] << ", " << rot[2][2] << "]" << std::endl;
				output << "t.row4 = [" << pos.x     << ", " << pos.y     << ", " << pos.z     << "]" << std::endl;
				output.precision(precision);
				
				std::string cameratype = "startCamera";
				std::string wAngle = "0";
				if (cam.ratio < 1)
				{
					cameratype = "sideCamera";
					wAngle = "-90";
				}
				output << "Camera" << i << ".transform = " << cameratype << " * R * t" << std::endl;
				output << std::endl;

				// create camera mapping for this camera

				output << "/* Create Diffuse */" << std::endl;
				output << "bm" << i << " = BitmapTexture name: \"bitmap_" << paddedIndex.str() << "\" filename: \"" << paddedIndex.str() << ".jpg\"" << std::endl;
				output << "bm" << i << ".coords.V_Tile = false" << std::endl;
				output << "bm" << i << ".coords.U_Tile = false" << std::endl;
				output << "bm" << i << ".coords.W_Angle = " << wAngle << std::endl;

				output << "/* create camera  map per pixel */" << std::endl;
				output << "cm_dif_" << i << " = Camera_Map_Per_Pixel name: \"cameramap_" << paddedIndex.str() << "\" camera: Camera" << i << " backFace: true angleThreshold: Default_Angle_Threshold texture: bm" << i << std::endl;

				output << "/* Create Opacity */" << std::endl;
				output << "bmo" << i << " = BitmapTexture name: \"bitmap_" << paddedIndex.str() << "\" filename: \"" << paddedIndex.str() << ".jpg\"" << std::endl;
				output << "bmo" << i << ".coords.V_Tile = false" << std::endl;
				output << "bmo" << i << ".coords.U_Tile = false" << std::endl;
				output << "bmo" << i << ".coords.W_Angle = " << wAngle << std::endl;
				output << "bmo" << i << ".output.Output_Amount = 100" << std::endl;
				output << "cmo_" << i << " = Camera_Map_Per_Pixel name: \"cameramap_o_" << paddedIndex.str() << "\" camera: Camera" << i << " backFace: true angleThreshold: 90 texture: bmo" << i << std::endl;

				output << std::endl;

				output << "/* create standard map */" << std::endl;
				output << "m" << i << " = standardMaterial name: \"material_" << paddedIndex.str() << "\" diffuseMap: cm_dif_" << i << " opacityMap: cmo_" << i << " showInViewport: true" << std::endl;

				// create or add to composite material
				if (materialNum == 10) 
				{
					materialNum = 1;
					compositeNum++;
				}
				
				compositeMaterial.str("");
				compositeMaterial << "cpm_" << compositeNum;
				std::stringstream lastCompositeMaterial;
				lastCompositeMaterial << "cpm_" << (compositeNum - 1);

				// create new composite material
				if (materialNum == 1) 
				{
					output << compositeMaterial.str() << " = compositeMaterial()" << std::endl;
					
					if (compositeNum == 1) // if first one 
					{
						output << compositeMaterial.str() << ".materialList[1] = standardMaterial name: \"Base Composite Material\" opacity: 100" << std::endl;
					}
					else
					{
						output << compositeMaterial.str() + ".materialList[1] = " + lastCompositeMaterial.str() << std::endl;
					}

				}

				output << compositeMaterial.str() << ".materialList[" << (materialNum+1) << "] = m" << i << std::endl;
				output << compositeMaterial.str() << ".amount[" << (materialNum) << "] = Default_Mix_Amount" << std::endl;
				output << compositeMaterial.str() << ".mixType[" << (materialNum) << "] = Default_Mix_Type" << std::endl;

				output << "if Enable_Camera_" << i << " != 1 do ( " << compositeMaterial.str()  << ".mapEnables[" << (materialNum+1) << "] = false )" << std::endl;
				output << std::endl;
				materialNum++;
			}

			output << "for node in rootNode.children do(" << std::endl;
			output << "node.material = " + compositeMaterial.str() << std::endl;
			output << ")" << std::endl;

			output << std::endl;
			output << "progressend()" << std::endl;
			output << std::endl;
			output << "messageBox \"Cameras and materials added!  Make sure you have rotated your model 90° to fit the camera coordinate system.  You can change the mix of materials to be projected by opening the material editor (press M), Material -> Get All Scene Materials, and then changing the setting in each combination material.\"" << std::endl;
			output << ")" << std::endl;
		}
		output.close();
	}
}