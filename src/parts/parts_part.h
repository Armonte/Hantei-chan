#ifndef PARTS_PART_H_GUARD
#define PARTS_PART_H_GUARD

#include <vector>
#include <functional>
#include <glm/mat4x4.hpp>
#include <string>
#include <iostream>
#include "../misc.h"

#define VAL(X) ((const char*)&X)
#define PTR(X) ((const char*)X)

template<template<typename> class Allocator = std::allocator>
class CutOut {
public:
    int id = 0;
    std::basic_string<char, std::char_traits<char>, Allocator<char>> name;

    int uv[4]{};       // Texture coordinates (L, T, W, H)
    int xy[2]{};       // Origin point
    int wh[2]{};       // Width, Height
    int texture = 0;   // Texture/graphic ID
    int ppjp[2]{};     // Unknown grab offset
    int vaoIndex = 0;  // VAO index (for rendering)
    int colorSlot = -1; // Palette color slot (-1 = none)
    int shapeIndex = 0; // Shape index
    int ppte[2]{};     // Unknown (stored as short[2])
    int pptx = 0;      // Unknown

    static unsigned int* PpLoad(unsigned int* data, const unsigned int* data_end, int id, std::vector<CutOut<>>* cutOuts);
    static void Save(std::ofstream &file, const CutOut *cutOut);
    static bool IsModifiedData(const CutOut *cutOut);
    void CopyTo(CutOut *cutOut);
};

#endif // PARTS_PART_H_GUARD
