#include "ini.h"
#include <sstream>
#include <iomanip>
#include <string>
#include <windows.h>

bool LoadFromIni(FrameData *framedata, CG *cg, const std::string& iniPath)
{
	int fileNum = GetPrivateProfileIntA("DataFile", "FileNum", 0, iniPath.c_str());
	if(fileNum)
	{
		std::string folder = iniPath.substr(0, iniPath.find_last_of("\\/"));
		for(int i = 0; i < fileNum; i++)
		{
			char ha6file[256]{};
			std::stringstream ss;
			ss << "File" << std::setfill('0') << std::setw(2) << i;
			GetPrivateProfileStringA("DataFile", ss.str().c_str(), nullptr, ha6file, 256, iniPath.c_str());

			std::string fullpath = folder + "\\" + ha6file;
			if(!framedata->load(fullpath.c_str(), i))
				return false;
		}

		int cgNum = GetPrivateProfileIntA("BmpcutFile", "FileNum", 0, iniPath.c_str());
		if(cgNum == 1)
		{
			char cgFile[256]{};
			GetPrivateProfileStringA("BmpcutFile", "File00", nullptr, cgFile, 256, iniPath.c_str());

			std::string fullpath = folder + "\\" + cgFile;
			cg->load(fullpath.c_str());
		}

		return true;
	}
	else
		return false;
}