#ifndef HITBOX_H_GUARD
#define HITBOX_H_GUARD
#include <map>

struct Hitbox {
	short x1, y1, x2, y2;
};

using BoxList = std::map<int, Hitbox>;

#endif /* HITBOX_H_GUARD */
