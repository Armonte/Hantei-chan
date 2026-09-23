#ifndef COPY_DATA_H_GUARD
#define COPY_DATA_H_GUARD

// Cross-instance clipboard. It lives in a named file mapping shared by every
// gonptechan process and allocates from tinyalloc inside that mapping, so its
// containers are valid in every process that maps the view at the same base.
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

// Maps (or creates) the shared clipboard and initialises tinyalloc for this
// process. Idempotent. `mappingName` overrides the default name (tests).
// Never returns null: if the shared view cannot be mapped at the common base
// address, a process-private clipboard is used instead (no cross-instance
// paste, but no crash).
CopyData* AcquireSharedCopyData(const wchar_t* mappingName = nullptr);
// True when this process's clipboard is the shared one.
bool SharedCopyDataIsShared();

#endif /* COPY_DATA_H_GUARD */
