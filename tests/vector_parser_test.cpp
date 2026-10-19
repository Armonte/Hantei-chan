#include "vectors.h"
#include <fstream>
#include <string>
#include <cstdio>

struct InspectVectors : VectorTXT {
    bool expected() const { return loaded && sub_vector_list && vector_list &&
        sub_vector_list->subvectors[0].x_vel == 256 && sub_vector_list->subvectors[0].y_vel == -512 &&
        vector_list->vectors[0].id == 0 && vector_list->vectors[0].VecNum == 0; }
    bool live() const { return loaded && vector_list && sub_vector_list &&
        vector_list->vectors[36].VecNum == 61 && sub_vector_list->subvectors[61].x_vel == 300 &&
        sub_vector_list->subvectors[61].y_vel == -1800; }
    bool empty() const { return !loaded && !sub_vector_list && !vector_list; }
};
int main(int argc, char** argv) {
    const std::string path = "vector_parser_bounds_fixture.txt";
    const std::string header = "\x2f\x2f\x20\x83\x78\x83\x4e\x83\x67\x83\x8b\x83\x8a\x83\x58\x83\x67";
    const std::string fixture = header + "\r\n0 256 -512 3 4 \r\n256 1 2 3 4 \r\n-1 1 2 3 4 \r\nEND\r\n"
        "0 hit 1 0 10 0 1\r\n0 0 0 0 0 00000000\r\n520\r\n520\r\nEND";
    auto write = [&](const std::string& bytes) { std::ofstream f(path, std::ios::binary|std::ios::trunc); f.write(bytes.data(), bytes.size()); };
    InspectVectors vectors;
    for (int run = 0; run < 20; ++run) {
        write(fixture);
        if (!vectors.load(path.c_str()) || !vectors.expected()) return 1;
        SubVector motion{};
        if(!vectors.GetPrimaryMotion(0,motion) || motion.y_vel!=-512 || vectors.GetPrimaryMotion(-1,motion) || vectors.GetPrimaryMotion(256,motion) || vectors.GetPrimaryMotion(255,motion)) return 6;
        vectors.free(); vectors.free();
        if (!vectors.empty()) return 2;
    }
    for (std::size_t n = 0; n < fixture.size(); ++n) {
        write(fixture.substr(0,n));
        if (vectors.load(path.c_str()) || !vectors.empty()) return 3;
    }
    write(header + "\r\n999999999999999999999999999 0 0 0 0 \r\nEND\r\nEND");
    if (!vectors.load(path.c_str())) return 4; // invalid numbers must not throw/access memory
    vectors.free();
    if (argc > 1 && (!vectors.load(argv[1]) || !vectors.live())) return 5;
    std::remove(path.c_str());
    std::printf("VECTOR PARSER BOUNDS PASS: reload, truncation, overflow, IDs; live=%d\n", argc > 1);
}
