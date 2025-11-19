#ifndef FRAMEDATA_H_GUARD
#define FRAMEDATA_H_GUARD

#include <string>
#include <vector>
#include <cstdint>

#include "hitbox.h"

#include <set>
extern std::set<int> numberSet;
extern int maxCount;

// Layer structure for multi-layer support (UNI AFGX + MBAACC AFGP compatibility)
template<template<typename> class Allocator = std::allocator>
struct Layer {
	// Rendering data (per-layer)
	int spriteId = -1;
	bool usePat = false;

	int		offset_y = 0;
	int		offset_x = 0;

	int		blend_mode = 0;

	float rgba[4]{1,1,1,1};

	float rotation[3]{}; //XYZ

	float scale[2]{1,1};//xy

	int priority = 0; // Layer priority (UNI AFPL tag, not used in MBAACC)

	// Assignment operator for cross-allocator copying
	template<template<typename> class FromT>
	Layer<Allocator>& operator=(const Layer<FromT>& from) {
		spriteId = from.spriteId;
		usePat = from.usePat;
		offset_y = from.offset_y;
		offset_x = from.offset_x;
		blend_mode = from.blend_mode;
		memcpy(rgba, from.rgba, sizeof(rgba));
		memcpy(rotation, from.rotation, sizeof(rotation));
		memcpy(scale, from.scale, sizeof(scale));
		priority = from.priority;
		return *this;
	}

	// Same-allocator assignment operator (use default memberwise copy)
	Layer<Allocator>& operator=(const Layer<Allocator>& from) = default;
};

// Multi-layer Frame structure (supports both MBAACC and UNI formats)
template<template<typename> class Allocator = std::allocator>
struct Frame_AF_T {
	// Multi-layer rendering data
	std::vector<Layer<Allocator>, Allocator<Layer<Allocator>>> layers;

	// Frame-level properties (not per-layer)

	//Depends on aniflag.
	//If (0)end, it jumps to the number of the sequence
	//If (2)jump, it jumps to the number of the frame of the seq.
	//It seems to do nothing if the aniflag is 1(next).
	int jump = 0;

	int		duration = 0;

	/* Animation action
	0 (default): End
	1: Next
	2: Jump to frame
	*/
	int aniType = 0;

	// Bit flags. First 4 bits only
	unsigned int aniFlag = 0;

	int landJump = 0; //Jumps to this frame if landing.
	//1-5: Linear, Fast end, Slow end, Fast middle, Slow Middle. The last type is not used in vanilla
	int interpolationType = 0;
	int priority = 0; // Default is 0. Used in throws and dodge.
	int loopCount = 0; //Times to loop, it's the frame just before the loop.
	int loopEnd = 0; //The frame number is not part of the loop.

	bool AFRT = false; //Makes rotation respect EF scale.

	// UNI/Dengeki Squirrel script reference values
	int frameId = 0;        // AFID - Frame ID for code reference
	uint8_t param[4] = {0}; // AFPA - 4 separate parameter values (0-255 each)

	// Assignment operator for cross-allocator copying
	template<template<typename> class FromT>
	Frame_AF_T<Allocator>& operator=(const Frame_AF_T<FromT>& from) {
		// Copy layers
		layers.resize(from.layers.size());
		for (size_t i = 0; i < from.layers.size(); i++) {
			layers[i] = from.layers[i];
		}

		// Copy frame-level properties
		jump = from.jump;
		duration = from.duration;
		aniType = from.aniType;
		aniFlag = from.aniFlag;
		landJump = from.landJump;
		interpolationType = from.interpolationType;
		priority = from.priority;
		loopCount = from.loopCount;
		loopEnd = from.loopEnd;
		AFRT = from.AFRT;
		frameId = from.frameId;
		memcpy(param, from.param, sizeof(param));
		return *this;
	}

	// Same-allocator assignment operator (use default memberwise copy)
	Frame_AF_T<Allocator>& operator=(const Frame_AF_T<Allocator>& from) = default;
};

struct Frame_AS {
	
	//Acceleration is always set if their corresponding XY speed flags are set.
	//To only set accel, set the add flags with 0 speed. 
	unsigned int movementFlags;
	int speed[2];
	int accel[2];
	int maxSpeedX;

	bool canMove;

	int stanceState;
	int cancelNormal;
	int cancelSpecial;
	int counterType;

	int hitsNumber;
	int invincibility;
	unsigned int statusFlags[2];

	//sinewave thing
	//0 Flags - Similar to ASV0
	//1,2 Distance X,Y
	//3,4 Frames per cycle X,Y
	//5,6 Phases X,Y. Use (0.75, 0) for CCW circles
	unsigned int sineFlags;
	int sineParameters[4];
	float sinePhases[2];
	
	// UNI/Dengeki-specific
	int ascf; // ASCF - counter hit or cancel related flag
};

struct Frame_AT {
	unsigned int guard_flags;
	unsigned int otherFlags;

	int correction;
	//default = 0, is set only if lower
	//1 multiplicative
	//2 subtractive
	int correction_type; 

	int damage;
	int red_damage;
	int guard_damage;
	int meter_gain;

	//Stand, Air, Crouch
	int guardVector[3];
	int hitVector[3];
	int gVFlags[3];
	int hVFlags[3];

	int hitEffect;
	int soundEffect; //Changes the audio

	int addedEffect; //Lasting visual effect after being hit

	bool hitgrab;

	//Affects untech time and launch vector, can be negative.
	float extraGravity;

	int breakTime;
	int untechTime;
	int hitStopTime; //Default value zero. Overrides common values.
	int hitStop; //From common value list
	int blockStopTime; //Needs flag 16 (0 indexed) to be set

	// UNI/Dengeki-specific damage system parameters
	int damageProration; // ATHH - damage proration percentage (100 = no reduction, 95 = 5% reduction)
	int minDamage;       // ATAM - minimum damage percentage
	int addHitStun;      // ATSA - player stun time added
	int starterCorrection; // ATSH - damage correction if combo starter
	int hitStunDecay[3]; // ATC0 - [reduction, combopoint_set, combopoint_SMP_modifier]
};

struct Frame_EF {
	int		type;
	int		number;
	int		parameters[12];
};

struct Frame_IF {
	int		type;
	int		parameters[9]; //Max used value is 9. I don't know if parameters beyond have any effect..
};

// Helper function for copying vector contents (for EF/IF copy operations)
template<typename Type, typename A, typename B>
void CopyVectorContents(A& dst, const B& src)
{
	static_assert(sizeof(typename A::value_type) == sizeof(Type) && sizeof(typename B::value_type) == sizeof(Type), "Vector element types don't match");
	dst.resize(src.size());
	memcpy(dst.data(), src.data(), sizeof(Type)*src.size());
}

template<template<typename> class Allocator = std::allocator>
struct Frame_T {
	Frame_AF_T<Allocator> AF = {};
	Frame_AS AS = {};
	Frame_AT AT = {};

	std::vector<Frame_EF, Allocator<Frame_EF>> EF;
	std::vector<Frame_IF, Allocator<Frame_IF>> IF;

	BoxList_T<Allocator> hitboxes{};

	// Cross-allocator assignment operator
	template<template<typename> class FromT>
	Frame_T<Allocator>& operator=(const Frame_T<FromT>& from) {
		AF = from.AF;
		AS = from.AS;
		AT = from.AT;
		CopyVectorContents<Frame_EF>(EF, from.EF);
		CopyVectorContents<Frame_IF>(IF, from.IF);
		// Manually copy map contents for cross-allocator support
		hitboxes.clear();
		for (const auto& pair : from.hitboxes) {
			hitboxes[pair.first] = pair.second;
		}
		return *this;
	}

	// Same-allocator assignment operator (deep copy)
	Frame_T<Allocator>& operator=(const Frame_T<Allocator>& from) {
		if (this != &from) {
			AF = from.AF;
			AS = from.AS;
			AT = from.AT;
			EF = from.EF;
			IF = from.IF;
			hitboxes = from.hitboxes;
		}
		return *this;
	}
};

template<template<typename> class Allocator = std::allocator>
struct Sequence_T {
	// sequence property data
	std::basic_string<char, std::char_traits<char>, Allocator<char>> name;
	std::basic_string<char, std::char_traits<char>, Allocator<char>> codeName;

	int psts = 0;
	int level = 0;
	int flag = 0;
	int pups = 0; // Palette index (UNI: 0=default, 1=_p1.pal, 2=_p2.pal, etc.)

	bool empty = false;
	bool initialized = false;
	bool modified = false;  // Track if this sequence has been edited
	bool usedAFGX = false;  // Track if this sequence used UNI multi-layer format (AFGX) vs MBAACC (AFGP)
	bool usedATV2 = false;  // Track if this sequence used UNI attack format (ATV2) vs MBAACC (ATVV/ATHV/ATGV)

	std::vector<Frame_T<Allocator>, Allocator<Frame_T<Allocator>>> frames;

	// Cross-allocator assignment operator
	template<template<typename> class FromT>
	Sequence_T<Allocator>& operator=(const Sequence_T<FromT>& from) {
		name = decltype(name)(from.name.data(), from.name.size());
		codeName = decltype(codeName)(from.codeName.data(), from.codeName.size());
		psts = from.psts;
		level = from.level;
		flag = from.flag;
		pups = from.pups;
		empty = from.empty;
		initialized = from.initialized;
		modified = from.modified;
		usedAFGX = from.usedAFGX;
		usedATV2 = from.usedATV2;
		frames.resize(from.frames.size());
		for (size_t i = 0; i < from.frames.size(); i++) {
			frames[i] = from.frames[i];
		}
		return *this;
	}

	// Same-allocator assignment operator (deep copy)
	Sequence_T<Allocator>& operator=(const Sequence_T<Allocator>& from) {
		if (this != &from) {
			name = from.name;
			codeName = from.codeName;
			psts = from.psts;
			level = from.level;
			flag = from.flag;
			pups = from.pups;
			empty = from.empty;
			initialized = from.initialized;
			modified = from.modified;
			usedAFGX = from.usedAFGX;
			usedATV2 = from.usedATV2;
			frames = from.frames;
		}
		return *this;
	}

	Sequence_T() = default;
};

// Typedefs for non-templated use (default std::allocator)
using Layer_Type = Layer<std::allocator>;
using Frame_AF = Frame_AF_T<std::allocator>;
using Frame = Frame_T<std::allocator>;
using Sequence = Sequence_T<std::allocator>;

struct Command {
	int id;
	std::string input;      // e.g., "41236C", "6+A+B"
	std::string comment;    // e.g., "第七聖典", "ダッシュ"

	Command() : id(-1) {}
};

class FrameData {
private:
	unsigned int	m_nsequences;

public:

	bool		m_loaded;
	std::vector<Sequence> m_sequences;
	std::vector<Command> m_commands;

	void initEmpty();
	bool load(const char *filename, bool patch = false);
	void save(const char *filename);
	void save_modified_only(const char *filename);  // Save only modified sequences
	bool load_commands(const char *filename);

	//Probably unnecessary.
	//bool load_move_list(Pack *pack, const char *filename);

	int get_sequence_count();

	Sequence* get_sequence(int n);
	std::string GetDecoratedName(int n);
	Command* get_command(int id);
	void mark_modified(int sequence_index);

	void Free();

	FrameData();
	~FrameData();
};

void WriteSequence(std::ofstream &file, const Sequence *seq);

#endif /* FRAMEDATA_H_GUARD */
