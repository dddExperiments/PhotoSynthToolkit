#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

union VertexIndex
{		
	VertexIndex(__int32 index);
	VertexIndex(unsigned int indexA, unsigned int indexB);

	struct
	{			
		unsigned int indexA : 16;
		unsigned int indexB : 16;
	};
	__int32 index;
};

std::vector<int> getPictureIndexes(const std::string& plyFilePath);

int main(int argc, char* argv[])
{
	std::ofstream output("ske.dat");
	if (output.is_open())
	{
		unsigned int nbImage = 216;
		unsigned int nbCluster = 3;

		output << "SKE" << std::endl;
		output << nbImage << " " << nbCluster <<std::endl;
		for (unsigned int i=0; i<nbCluster; ++i)
		{
			std::stringstream filename;
			filename << "cluster_" << i << ".ply";
			
			const std::vector<int>& indexes = getPictureIndexes(filename.str());
			
			output << indexes.size() << " 0" << std::endl;
			for (unsigned int j=0; j<indexes.size(); ++j)
			{
				output << indexes[j] << " ";
			}
			output << std::endl;
		}
	}
	output.close();
}

VertexIndex::VertexIndex(__int32 index)
{
	this->index = index;
}

VertexIndex::VertexIndex(unsigned int indexA, unsigned int indexB)
{
	this->indexA = indexA;
	this->indexB = indexB;
}

std::vector<int> getPictureIndexes(const std::string& plyFilePath)
{
	std::vector<int> indexes;

	std::ifstream input;
	input.open(plyFilePath.c_str(), std::ios::binary);

	if (input.is_open())
	{
		std::string line;
		unsigned int nbVertices = 0;
		unsigned int nbTriangles = 0;
		bool isBinaryFile = false;
		bool hasNormals = false;
		bool hasColors  = false;
		bool hasAlpha   = false;

		//parse header
		do 
		{
			std::getline(input, line);

			if (line == "format binary_little_endian 1.0")
				isBinaryFile = true;
			else if (line == "format ascii 1.0")
				isBinaryFile = false;
			else if (line == "property float nx")
				hasNormals = true;
			else if (line == "property uchar red")
				hasColors = true;
			else if (line == "property uchar alpha")
				hasAlpha = true;
			else if (line == "property uchar diffuse_red")
				hasColors = true;
			else if (line == "property uchar diffuse_alpha")
				hasAlpha = true;
			else
			{
				std::string keywordVertex("element vertex ");
				unsigned int lengthVertex = keywordVertex.size();

				std::string keywordFace("element face ");
				unsigned int lengthFace = keywordFace.size();

				if (line.size() > lengthVertex && line.substr(0, lengthVertex) == keywordVertex)
				{
					nbVertices = atoi(line.substr(lengthVertex).c_str());
				}
				else if (line.size() > lengthFace && line.substr(0, lengthFace) == keywordFace)
				{
					nbTriangles = atoi(line.substr(lengthFace).c_str());
				}
			}
		} while (line != "end_header"); //missing eof check...

		if (!isBinaryFile || !hasNormals)
		{
			std::cout << "The file was written in ascii format so indexes store in normals are corrupted !" << std::endl;
			std::cout << "You need to clean the mesh again and save it in binary format with normals" << std::endl;
		}
		else
		{		
			float* pos = new float[6];
			size_t sizePos = sizeof(float)*6;

			unsigned char* color = new unsigned char[hasAlpha?4:3];
			size_t sizeColor = sizeof(unsigned char)*(hasAlpha?4:3);

			for (unsigned int i=0; i<nbVertices; ++i)
			{
				input.read((char*)pos, sizePos);
				if (hasColors)
					input.read((char*)color, sizeColor);

				// pos[0] = x
				// pos[1] = y
				// pos[2] = z
				// pos[3] = index.indexA
				// pos[4] = index.indexB
				// pos[5] = 42

				VertexIndex vindex((unsigned int)pos[3], (unsigned int)pos[4]);
				if ((unsigned int)pos[5] == 42)
					indexes.push_back(vindex.index);
			}
			delete[] pos;
			delete[] color;
		}
	}
	input.close();

	return indexes;
}