#include <filesystem>
#include <fstream>
#include <iostream>
#include "framedata.h"
#include "framedata_load.h"

void TestHa6()
{
	namespace fs = std::filesystem;

	int chunkSize = 8;
	int nChunks = 0;

	FrameData fd;

	std::string folderIn = "data/";
	for(const fs::directory_entry &file : fs::directory_iterator(folderIn))
	{

		std::string filename = file.path().filename().string();
		if(filename.back() == '6' ) //Ending in ha6
		{
			fd.load((folderIn + filename).c_str());
		}
	}

	std::cout<<"Max:"<<maxCount<<"\n";
	std::cout<<"Set:\n";
	for(int number : numberSet)
	{
		std::cout <<number<<"\n";
	}
}