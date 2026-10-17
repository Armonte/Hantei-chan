#ifndef S3D_FBXASCII_H_GUARD
#define S3D_FBXASCII_H_GUARD
// ASCII FBX 6.1.0 stage models: DFCI (bg\<stage>\bg.fbx) and the UNI[st]/[cl-r] arcade build (bg\bgNN\bg.fbx), into the same
// s3d::Model the fbxex reader fills, so the view and its camera are shared. Conventions match the fbxex converter: node 0 is
// the scene root, depth-first node order, one vertex per polygon corner, fan triangulation, takes baked to 30 fps tracks.
#include "s3d_model.h"

#include <string>

namespace s3d {

bool ParseFbxAscii(const std::string& text, Model& out, std::string& err);
bool LoadFbxAsciiFile(const std::string& utf8Path, Model& out, std::string& err);
// bg.fbx.bin (fbxex) when present, else bg.fbx (ASCII), from a stage folder or either file's path.
bool LoadStageModel(const std::string& utf8PathOrDir, Model& out, std::string& usedPath, std::string& err);

}  // namespace s3d
#endif
