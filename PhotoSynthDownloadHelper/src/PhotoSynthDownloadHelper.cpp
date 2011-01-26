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

#include "PhotoSynthDownloadHelper.h"
#include <OgreStringVector.h>

using namespace PhotoSynth;
using boost::asio::ip::tcp;

//url should be like http://toto.net/index.html -> return toto.net
std::string DownloadHelper::extractHost(const std::string& url)
{
	Ogre::StringVector tmp = Ogre::StringUtil::split(url, "/");
	if (tmp.size() < 2)
		return "";
	else
		return tmp[1];
}

//url should be like http://toto.net/index.html -> return /index.html (host is optional and sould be like toto.net)
std::string DownloadHelper::removeHost(const std::string& url, const std::string& host)
{
	std::string domain = (host == "" ? extractHost(url) : host);

	return url.substr(domain.size()+std::string("http://").size());
}

SocketPtr DownloadHelper::connect(boost::asio::io_service* service, const std::string& url)
{
	tcp::resolver resolver(*(service));
	tcp::resolver::query query(url, "http");

	tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
	tcp::resolver::iterator end;

	SocketPtr socket = SocketPtr(new tcp::socket(*(service)));
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

std::string DownloadHelper::createGETHeader(const std::string& get, const std::string& host)
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

unsigned int DownloadHelper::getContentLength(std::istream& header)
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

bool DownloadHelper::saveAsciiFile(const std::string& filepath, const std::string& content)
{
	std::ofstream output;
	output.open(filepath.c_str());

	bool opened = output.is_open();

	if (opened)
		output << content;
	output.close();

	return opened;
}

bool DownloadHelper::saveBinFile(const std::string& filepath, std::istream& input, unsigned int length)
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