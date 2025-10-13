#ifndef PARTS_SHAPE_H_GUARD
#define PARTS_SHAPE_H_GUARD

#include <vector>
#include <functional>
#include <glm/mat4x4.hpp>
#include <string>
#include <iostream>
#include "../misc.h"

#define VAL(X) ((const char*)&X)
#define PTR(X) ((const char*)X)

enum class ShapeType {
    PLANE = 1,   // Flat quad
    UNK2 = 2,    // Unknown (treated like plane)
    RING = 3,    // Cylindrical section
    ARC = 4,     // Curved surface
    SPHERE = 5,  // Spherical surface
    CONE = 6     // Conical surface
};

template<template<typename> class Allocator = std::allocator>
class Shape {
public:
    int id = 0;
    std::basic_string<char, std::char_traits<char>, Allocator<char>> name;
    ShapeType type = ShapeType::PLANE;
    int vertexCount = 0;   // Vertex subdivisions
    int vertexCount2 = 0;  // Second dimension (for 2D shapes)
    int length = 0;        // Arc length (in 1/10000ths)
    int length2 = 0;       // Second arc length
    int radius = 0;        // Radius
    int dRadius = 0;       // Radius delta
    int width = 0;         // Width
    int dz = 0;            // Depth delta

    // Loading
    static unsigned int* VeLoad(unsigned int* data, const unsigned int* data_end, int amount, int len, std::vector<Shape<>>* shapes);
    static unsigned int* VnLoad(unsigned int* data, const unsigned int* data_end, int amount, std::vector<Shape<>>* shapes);

    // Saving
    static void Save(std::ofstream &file, const Shape *shape, bool saveName);
    static bool IsModifiedData(const Shape *shape);

    // Utilities
    void CopyTo(Shape* shape);
};

#endif // PARTS_SHAPE_H_GUARD
