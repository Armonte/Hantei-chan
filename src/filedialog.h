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
	DDS,
	DAT,
	CMDTXT,
	HA4,
	HAN2,       // French-Bread RBO / GOF2 character (.dat/.dt2) or PAC archive
	HAN2SAVE,
	HA4SAVE,
	OPENANY     // File > Open...: every supported character / archive / project file
};
}

std::string FileDialog(int fileType = -1, bool save = false, char* defaultName = nullptr);
