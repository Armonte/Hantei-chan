#pragma once
#include <cstdint>
namespace mbaacc::extended {
constexpr bool isTypedHitgrab(std::uint32_t raw) {
    return (raw & ~0x00FF0000u) == 0x58000001u && (raw & 0x00FF0000u) != 0;
}
constexpr int typedHitgrabProfile(std::uint32_t raw) { return isTypedHitgrab(raw) ? int((raw >> 16) & 255u) : 0; }
constexpr std::uint32_t makeTypedHitgrab(unsigned profile) { return profile == 0 ? 1u : (0x58000001u | ((profile & 255u) << 16)); }
}
