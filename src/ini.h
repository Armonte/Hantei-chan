#ifndef INI_H_GUARD
#define INI_H_GUARD
#include "framedata.h"
#include "cg.h"

bool LoadFromIni(FrameData *framedata, CG *cg, const std::string& iniPath);

#endif /* INI_H_GUARD */
