#ifndef COPY_MANAGER_H_GUARD
#define COPY_MANAGER_H_GUARD
#include "framedata.h"
#include "parts/parts.h"
#include "parts/parts_partset.h"
#include "parts/parts_part.h"
#include "parts/parts_shape.h"
#include "parts/parts_texture.h"

// CopyManager holds static clipboard data for PatEditor copy/paste operations
struct CopyManager
{
	static struct CopyData{
		Frame_AS as{};
		Frame_AF_T<std::allocator> af{};
		Frame_T<std::allocator> frame{};
		Sequence_T<std::allocator> pattern{};
		std::vector<Frame_T<std::allocator>, std::allocator<Frame_T<std::allocator>>> frames{};

		Frame_AT at {};
		std::vector<Frame_EF,std::allocator<Frame_EF>> efGroup{};
		std::vector<Frame_IF,std::allocator<Frame_IF>> ifGroup{};
		Frame_IF ifSingle{};
		Frame_EF efSingle{};

		BoxList_T<std::allocator> boxes;
		Hitbox box;
	} *copied;

	// PatEditor clipboard for Parts data
	static struct CopyParts {
		PartSet<> partSet{};
		PartProperty partProperty{};
		CutOut<> cutOut{};
		Shape<> shape{};
		PartGfx<> gfx{};
	} *copiedParts;
};

#endif /* COPY_MANAGER_H_GUARD */
