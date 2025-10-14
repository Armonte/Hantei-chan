#ifndef PARTS_PARTSET_H_GUARD
#define PARTS_PARTSET_H_GUARD

#include <vector>
#include <functional>
#include <glm/mat4x4.hpp>
#include <string>
#include <iostream>
#include "../misc.h"

#define VAL(X) ((const char*)&X)
#define PTR(X) ((const char*)X)

struct PartProperty {
    int propId = -1;
    float priority = 0.f;  // Float to preserve sub-ordering from propId
    float rotation[4]{}; // PRA3: 3D rotation (4 floats)
    int x = 0;
    int y = 0;
    float scaleX = 1.f;
    float scaleY = 1.f;
    int ppId = -1; // CutOut ID. Use this, and if -1, don't save in .pat
    bool additive = false;
    bool filter = false;
    int flip = 0; // 0=none, 1=H, 2=V, 3=both (PRRV)
    unsigned char bgra[4] = { 255, 255, 255, 255 }; // Color tint (BGRA byte order like Eiton)
    float addColor[4] = { 0.f, 0.f, 0.f, 0.f }; // Additive color (PRSP)
};

template<template<typename> class Allocator = std::allocator>
class PartSet {
public:
    int partId = -1;
    std::basic_string<char, std::char_traits<char>, Allocator<char>> name;
    std::vector<PartProperty> groups; // Part properties in this set

    // Loading
    static unsigned int* P_Load(unsigned int* data, const unsigned int* data_end, int id, PartSet* partSet);
    static unsigned int* PrLoad(unsigned int* data, const unsigned int* data_end, int groupId, int propId, PartSet<>* partSet);

    // Saving
    static void Save(std::ofstream &file, const PartSet *partSet);
    static bool IsModifiedData(const PartSet *partSet);
    static bool IsModifiedPropData(const PartProperty *prop);

    // Utilities
    static void CopyPropertyTo(PartProperty *propDst, PartProperty *propSrc);
    void CopyPartSetTo(PartSet *partSet);
};

#endif // PARTS_PARTSET_H_GUARD
