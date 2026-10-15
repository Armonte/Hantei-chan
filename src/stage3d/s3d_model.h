#ifndef S3D_MODEL_H_GUARD
#define S3D_MODEL_H_GUARD
// French-Bread 3D stage model ("fbxex", bg\bgNNN\bg.fbx.bin) as shipped by UNDER NIGHT IN-BIRTH II, UNI[st]/[cl-r],
// MBTL (after archive decryption) and, via FBX conversion, DFCI. Presentation data only. Written for Hantei-chan from the
// format description in Zanaylo's UNI2-Improvement-Mod wiki (Stage-models.md, credited in docs/stage3d/SCOPE.md) and
// checked against the shipped UNI2 files; no code is taken from that GPL-3.0 project.
//
// Layout (little endian): "fbxex\0\0\0", u32 0, u32 0, then four blocks { u32 size (incl. its 8-byte prefix), u32 count, body }:
//   0 textures : count x char[128] DDS file name (same folder)
//   1 materials: count x 204 B = char name[128], u32 index, u32 textureIndex, float v[17] (D3DMATERIAL9 order)
//   2 nodes    : count x { u32 size (incl. 16-byte prefix), u32 type, i32 firstChild, i32 nextSibling, payload }
//                type 1 (mesh): u32 additive, u32 flag2, float world[16], u32 nv, nv x 12 floats
//                (pos3 normal3 colour3 alpha uv2), u32 nsub, nsub x { u32 material, u32 ni, u32 idx[ni] }
//   3 anime    : count x { u32 frames, frames x float[16] } local matrices, 30 fps, each track loops on its own length
// Matrices are D3D row-vector (v' = v * M); world = local * parentWorld.
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace s3d {

using Mat4 = std::array<float, 16>;   // row-major, row-vector (D3D)

struct Vertex {
	float pos[3];
	float normal[3];
	float color[3];   // baked lighting
	float alpha;
	float uv[2];      // D3D top-left origin, tiles
};
static_assert(sizeof(Vertex) == 48, "fbxex vertex is 12 floats");

struct SubMesh {
	uint32_t material = 0;
	std::vector<uint32_t> indices;   // triangle list into the node's vertices
};

struct Material {
	std::string name;
	int32_t texture = -1;
	float v[17] = {};   // diffuse4 ambient4 specular4 emissive4 power
};

struct Node {
	uint32_t type = 0;   // 0 transform, 1 mesh
	int32_t firstChild = -1, nextSibling = -1, parent = -1;
	bool additive = false;
	uint32_t flag2 = 0;
	Mat4 world{};
	std::vector<Vertex> verts;
	std::vector<SubMesh> subs;
};

struct Model {
	std::vector<std::string> textures;
	std::vector<Material> materials;
	std::vector<Node> nodes;
	std::vector<std::vector<Mat4>> tracks;   // one per node
};

// Never throws, never reads out of bounds. false + err on malformed input.
bool ParseFbxEx(const uint8_t* data, size_t size, Model& out, std::string& err);
bool LoadFbxExFile(const std::string& utf8Path, Model& out, std::string& err);

Mat4 MatIdentity();
Mat4 MatMul(const Mat4& a, const Mat4& b);   // row-vector: a first, then b
// World matrices at a 30 fps frame number (each track wraps on its own length).
void EvaluateWorld(const Model& m, uint64_t frame30, std::vector<Mat4>& out);

struct Stats {
	uint32_t nodes = 0, meshes = 0, additive = 0, vertices = 0, triangles = 0, animated = 0;
	float bmin[3] = {}, bmax[3] = {};   // world space, frame 0
};
Stats ComputeStats(const Model& m);

}  // namespace s3d
#endif
