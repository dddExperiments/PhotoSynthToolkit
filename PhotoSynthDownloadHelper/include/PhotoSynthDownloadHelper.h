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

#include <boost/asio.hpp>

typedef boost::shared_ptr<boost::asio::ip::tcp::socket> SocketPtr;

namespace PhotoSynth
{
	class DownloadHelper
	{
		public:
			//socket helper
			static SocketPtr connect(boost::asio::io_service* service, const std::string& url);
			static std::string extractHost(const std::string& url);
			static std::string removeHost(const std::string& url, const std::string& host);
			static std::string createGETHeader(const std::string& get, const std::string& host = "");
			static unsigned int getContentLength(std::istream& header);

			//file helper			
			static bool saveAsciiFile(const std::string& filepath, const std::string& content);
			static bool saveBinFile(const std::string& filepath, std::istream& input, unsigned int length);						
	};
}