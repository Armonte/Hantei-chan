#include <string>

namespace fileType
{
enum {
	HA6,
	CG,
	PAL
};
}

std::string FileDialog(int fileType = -1);
