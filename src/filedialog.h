#include <string>

namespace fileType
{
enum {
	HA6,
	CG,
	PAL,
	TXT
};
}

std::string FileDialog(int fileType = -1);
