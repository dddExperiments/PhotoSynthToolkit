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

#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include "PhotoSynthDownloader.h"

int main(int argc, char* argv[])
{
	if (argc < 3)
	{
		std::cout << "Usage: " << argv[0] << " <guid> <outputFolder> [optional]"<<std::endl;
		std::cout << "<guid>: PhotoSynth GUID (http://photosynth.net/view.aspx?cid=GUID)"<<std::endl;
		std::cout << "<outputFolder>: folder in which the synth will be downloaded"<<std::endl;	
		std::cout << "[optional] : thumb (will download thumbs)" <<std::endl;
		std::cout <<std::endl;
		std::cout << "Example: "<<argv[0]<< " 1471c7c7-da12-4859-9289-a2e6d2129319 church thumb"<<std::endl;
		std::cout << "will download json, thumbs and bin files and put everything in \"church\" folder"<<std::endl;

		return -1;
	}

	std::string guid         = argv[1];
	std::string outputFolder = argv[2];
	bool downloadThumb       = false;

	for (int i=1; i<argc; ++i)
	{
		std::string current(argv[i]);
		if (current == "thumb")
			downloadThumb = true;
	}

	boost::asio::io_service* service;
	try
	{
		service = new boost::asio::io_service;
		service->run();

		PhotoSynth::Downloader downloader(service);
		downloader.download(guid, outputFolder, downloadThumb);
	}
	catch(std::exception& e)
	{
		std::cout << e.what() << std::endl;
	}

	return 0;
}