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

#include "PhotoSynthStructures.h"

namespace PhotoSynth
{
	struct SoapInfo
	{
		enum CollectionType
		{
			SYNTH,
			PANORAMA
		};

		bool succeeded;
		CollectionType collectionType;
		std::string dzcUrl;
		std::string jsonUrl;
		std::string collectionRoot;
		std::string privacyLevel;

	};

	struct JsonInfo
	{
		double version;
		std::string license;

		std::vector<Thumb> thumbs;
	};

	class Parser
	{
		public:
			static std::string createFilePath(const std::string& path, const std::string& filename);
			static std::string readAsciiFile(const std::string& filepath);
			static std::string getGuid(const std::string& filepath);

			static std::string jsonFilename;
			static std::string soapFilename;
			static std::string guidFilename;

		public:

			void parseSoap(const std::string& soapFilePath);
			void parseJson(const std::string& jsonFilePath, const std::string& guid);
			void parseBinFiles(const std::string& inputPath);

			const SoapInfo& getSoapInfo() const;
			const JsonInfo& getJsonInfo() const;

			unsigned int getNbCoordSystem() const;
			const CoordSystem& getCoordSystem(unsigned int coordSystemIndex) const;
			unsigned int getNbVertex(unsigned int coordSystemIndex) const;

			unsigned int getNbCamera(unsigned int coordSystemIndex) const;
			const Camera& getCamera(unsigned int coordSystemIndex, unsigned int cameraIndex) const;

			unsigned int getNbPointCloud(unsigned int coordSystemIndex) const;
			const PointCloud& getPointCloud(unsigned int coordSystemIndex, unsigned int pointCloudIndex) const;		

		protected:

			std::string mJsonFilename;
			std::string mSoapFilename;

			SoapInfo mSoapInfo;
			JsonInfo mJsonInfo;

			std::vector<CoordSystem> mCoordSystems;

			//Converted (C# -> C++) from SynthExport (http://synthexport.codeplex.com/)
			bool loadBinFile(const std::string& inputPath, unsigned int coordSystemIndex, unsigned int binFileIndex);
			int ReadCompressedInt(std::istream& input);
			float ReadBigEndianSingle(std::istream& input);
			unsigned int ReadBigEndianUInt16(std::istream& input);
	};
}