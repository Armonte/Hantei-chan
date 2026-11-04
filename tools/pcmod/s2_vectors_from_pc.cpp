// Prints PovertyCaster's OWN schema-2 digests for tests/fixtures/pcmod_s2 (bit-exactness oracle for pcmod_s2_test).
// Build against PC branch mods/pcmod-schema2 headers (tools/pcmod/s2_vectors_from_pc.sh does it):
//   g++ -std=c++17 -I<pc>/pc-core/include s2_vectors_from_pc.cpp -o s2v && ./s2v tests/fixtures/pcmod_s2
#include "pc/mods/Manifest.hpp"
#include <cstdio>
#include <fstream>
#include <iterator>
static std::string rd(const std::string& p) { std::ifstream f(p, std::ios::binary); return std::string(std::istreambuf_iterator<char>(f), {}); }
int main(int argc, char** argv) {
    namespace pm = pc::mods; namespace s2 = pc::mods::s2;
    const std::string dir = argc > 1 ? argv[1] : "tests/fixtures/pcmod_s2";
    // The PC test profile (tests/pcmod_resolve s2 block).
    s2::Profile prof;
    prof.abiId = "test-prim-1";
    prof.imageBegin = 0x401000; prof.imageEnd = 0x560000; prof.textBegin = 0x401000; prof.textEnd = 0x54B000;
    prof.reserved = { { 0x00450000, 8, "boot patch: test site" } };
    prof.callPrimitives = { "test_hook" };
    prof.features = { { "custom-vars", 1, { { "slots", 1, 1024 } } } };
    prof.rules = { { "hp", 1, 99999 } };
    pm::ParseResult r = pm::parseManifest(rd(dir + "/pcmod.json"));
    if (!r.ok) { std::printf("manifest refused: %s\n", r.error.c_str()); return 1; }
    const std::string e = pm::loadSchema2(r.manifest, rd(dir + "/patches.toml"), rd(dir + "/features.toml"), rd(dir + "/rules.toml"), prof);
    if (!e.empty()) { std::printf("schema2 refused: %s\n", e.c_str()); return 1; }
    std::printf("canon:\n%s", r.manifest.s2Canon.c_str());
    std::printf("pkg %s\n", pm::detail::hex(pm::packageDigest(r.manifest)).c_str());
    std::printf("sim %s\n", pm::detail::hex(pm::packageSimDigest(r.manifest)).c_str());
    pm::SimEntry se; se.id = r.manifest.id; se.version = r.manifest.version; se.priority = 0; se.digest = pm::packageSimDigest(r.manifest); se.schema = 2;
    std::printf("set %s\n", pm::detail::hex(pm::sessionSimDigest(r.manifest.exe[0], { se }, prof.abiId)).c_str());
    return 0;
}
