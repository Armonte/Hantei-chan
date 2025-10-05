#include "preset_effects.h"

// Get human-readable name for Effect Type 3 preset number
// Based on Effect_Type3_Implementation.md documentation
const char* GetPresetEffectName(int presetNumber)
{
	switch(presetNumber) {
		case 0:  return "Jump Effect";
		case 1:  return "Unknown (0x01)";
		case 2:  return "David Star";
		case 3:  return "Red Hitspark";
		case 4:  return "Force Field";
		case 5:  return "Fire";
		case 6:  return "Fire Variant";
		case 7:  return "Snow";
		case 8:  return "Blue Flash";
		case 9:  return "Blue Hitspark";
		case 10: return "Superflash";
		case 14: return "Unknown (0x0e)";
		case 15: return "Unknown (0x0f)";
		case 16: return "Unknown (0x10)";
		case 19: return "Unknown (0x13)";
		case 20: return "3D Waves";
		case 21: return "Foggy Rays";
		case 22: return "Particle Spray";
		case 23: return "Blind (Type 1)";
		case 24: return "Blind (Type 2)";
		case 27: return "Dust (Small)";
		case 28: return "Dust (Large)";
		case 29: return "Dust (Rotate)";
		case 30: return "Dust (Massive)";
		case 100: return "Aoko Circle";
		case 101: return "Aoko Circle 2";
		case 256: return "Unknown (0x100)";
		case 265: return "Spray (Large)";
		case 300: return "Unknown (0x12c)";
		case 301: return "Unknown (0x12d)";
		case 302: return "Complex";
		default: return "Unknown";
	}
}

// Get description of preset effect for tooltip
const char* GetPresetEffectDescription(int presetNumber)
{
	switch(presetNumber) {
		case 0:  return "Jump effect (circular expansion)";
		case 2:  return "David star (causes lag - unused)";
		case 3:  return "Red hitspark with sparks";
		case 8:  return "Blue flash wave effect";
		case 9:  return "Blue hitspark burst";
		case 10: return "Superflash freeze (no visual)";
		case 23:
		case 24: return "Blinding screen effect";
		case 27:
		case 28:
		case 29:
		case 30: return "Dust cloud particles";
		case 100:
		case 101: return "Boss Aoko fading circle";
		default: return "Procedural particle effect";
	}
}
