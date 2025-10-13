#include <string>

namespace fileType
{
enum {
	HA6,
	CG,
	PAL,
	TXT,
	VECTOR,
	HPROJ,
	PAT,
	DDS
};
}

std::string FileDialog(int fileType = -1, bool save = false, char* defaultName = nullptr);
