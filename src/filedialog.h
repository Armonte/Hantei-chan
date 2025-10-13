#include <string>

namespace fileType
{
enum {
	HA6,
	HA4,
	CG,
	PAL,
	TXT,
	VECTOR,
	HPROJ,
	DAT
};
}

std::string FileDialog(int fileType = -1, bool save = false);
