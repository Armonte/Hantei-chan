#include <string>

namespace fileType
{
enum {
	HA6,
	CG,
	PAL,
	TXT,
	VECTOR,
	HPROJ
};
}

std::string FileDialog(int fileType = -1, bool save = false);
