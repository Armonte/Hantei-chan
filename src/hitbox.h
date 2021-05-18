#ifndef HITBOX_H_GUARD
#define HITBOX_H_GUARD
#include <map>

struct Hitbox
{
	int xy[4];
};

using BoxList = std::map<int, Hitbox>;

#endif /* HITBOX_H_GUARD */
