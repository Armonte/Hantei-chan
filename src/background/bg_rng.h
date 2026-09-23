// Stage RNGs, ported 1:1 from the games so a preview with a given seed makes
// the same random spawn / velocity / weather choices as the game.
//
// MBAACC (MBAA.exe): the stage code draws from stream 0 of the game's RNG bank
// (0x563780: 11 streams x 57 dwords = {index, seed[56]}).
//   RngState_Initialize  0x421af0  seed a stream
//   Rng_UpdateState      0x421c10  next int of stream N
//   Rng_GetFloat01       0x421c50  next int * 4.656612875028957e-10
//   Rng_LaggedFibonacci  0x4b4f70  next int of stream 0 (inlined copy)
// The generator is Knuth's subtractive generator (the one .NET's
// System.Random uses): MBIG = 0x7FFFFFFF, MSEED = 161803398, lags 55/21.
// Stage-code callers: BgCmd_SpawnRandomObject, BgCmd_RandomizeVelocity,
// DropObject_InitializeParticles, DropObject_UpdateParticles. A few effect
// presets also use stream 0, so an in-match sequence matches this preview
// only while none of those run.
//
// MBAC (mbacPC.exe): Rng_NextForStream(0) 0x45ef40, a per-stream LCG
// state = state * 0x41C64E6D + 0x3039, result (state >> 16) & 0x7FFF.
#ifndef BG_RNG_H_GUARD
#define BG_RNG_H_GUARD

#include <cstdint>

namespace bg {

enum class Game { MBAACC = 0, MBAC = 1 };

class Rng {
public:
	Rng() { Seed(0, Game::MBAACC); }

	void Seed(int32_t s, Game g) {
		seed = s;
		game = g;
		lcg = (uint32_t)s;
		// RngState_Initialize, verbatim (the .NET Random constructor).
		uint32_t a = (uint32_t)s;
		if ((int32_t)a < 0) a = 0u - a;           // x86 neg, INT_MIN stays INT_MIN
		int32_t mj = (int32_t)(161803398u - a);
		arr[55] = mj;
		int32_t mk = 1;
		for (int i = 21; i < 1155; i += 21) {
			arr[i % 55] = mk;
			mk = mj - mk;
			if (mk < 0) mk += 0x7FFFFFFF;
			mj = arr[i % 55];
		}
		for (int k = 0; k < 4; ++k)
			for (int i = 1; i < 56; ++i) {
				arr[i] -= arr[1 + (i + 30) % 55];
				if (arr[i] < 0) arr[i] += 0x7FFFFFFF;
			}
		inext = 0;
		calls = 0;
	}

	// Next raw int: MBAACC 0..0x7FFFFFFE, MBAC 0..0x7FFF.
	int32_t Next() {
		++calls;
		if (game == Game::MBAC) {
			lcg = lcg * 0x41C64E6Du + 0x3039u;
			return (int32_t)((lcg >> 16) & 0x7FFF);
		}
		int n = inext + 1;
		if (n >= 56) n = 1;
		inext = n;
		int p = (n > 34) ? n - 34 : n + 21;
		int32_t v = arr[n] - arr[p];
		if (v < 0) v += 0x7FFFFFFF;
		arr[n] = v;
		return v;
	}

	// Rng_GetFloat01 (MBAACC only; the MBAC stage code has no float draws).
	double Float01() { return (double)Next() * 4.656612875028957e-10; }

	int32_t GetSeed() const { return seed; }
	Game    GetGame() const { return game; }
	uint64_t Calls()  const { return calls; }

private:
	int32_t  seed = 0;
	Game     game = Game::MBAACC;
	int32_t  arr[56] = {0};
	int      inext = 0;
	uint32_t lcg = 0;
	uint64_t calls = 0;
};

} // namespace bg

#endif // BG_RNG_H_GUARD
