#ifndef HITBOX_H_GUARD
#define HITBOX_H_GUARD
#include <map>

struct Hitbox
{
	int xy[4];
};

template<template<typename> class Allocator = std::allocator>
using BoxList_T = std::map<int, Hitbox, std::less<int>, Allocator<std::pair<const int, Hitbox>>>;

using BoxList = BoxList_T<std::allocator>;

#endif /* HITBOX_H_GUARD */
