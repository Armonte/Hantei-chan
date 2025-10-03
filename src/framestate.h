#ifndef FRAMESTATE_H_GUARD
#define FRAMESTATE_H_GUARD

#include "framedata.h"
#include "hitbox.h"
#include <linear_allocator.hpp>
#include <vector>

struct CopyData {
	Frame_AS as{};
	Frame_AF_T<LinearAllocator> af{};
	Frame_T<LinearAllocator> frame{};
	Sequence_T<LinearAllocator> pattern{};
	std::vector<Frame_T<LinearAllocator>, LinearAllocator<Frame_T<LinearAllocator>>> frames{};
	Frame_AT at{};
	std::vector<Frame_EF, LinearAllocator<Frame_EF>> efGroup{};
	std::vector<Frame_IF, LinearAllocator<Frame_IF>> ifGroup{};
	Frame_IF ifSingle{};
	Frame_EF efSingle{};
	BoxList_T<LinearAllocator> boxes;
	Hitbox box;
};

struct FrameState
{
	int pattern;
	int frame;
	int spriteId;
	int selectedLayer = 0;  // Track currently selected layer in UI

	// Animation playback
	bool animating = false;
	int animeSeq = 0;

	// Shared memory clipboard for cross-instance copy/paste
	CopyData *copied;

	FrameState();
	~FrameState();

private:
	void *sharedMemHandle = nullptr;
	void *sharedMem = nullptr;
};

#endif /* FRAMESTATE_H_GUARD */
