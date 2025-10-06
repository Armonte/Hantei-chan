# Effect Type Categories

This directory contains the split implementation of DrawSmartEffectUI() from frame_disp_ef.h.

Each file handles specific effect type(s):

- **effect_spawn.h** - Types 1, 101, 11, 111 (pattern spawning)
- **effect_visual.h** - Type 2 (visual effects - superflash, heat, speed lines)
- **effect_preset.h** - Type 3 (preset effects - dust, sparks, force fields)
- **effect_state.h** - Types 4, 14 (opponent state changes, knockback)
- **effect_damage.h** - Type 5 (direct damage)
- **effect_misc.h** - Type 6 (HUGE - invuln, teleports, screen effects, movement, etc.)
- **effect_actor.h** - Type 8 (actor spawning from effect.ha6)
- **effect_audio.h** - Type 9 (sound playback)
- **effect_unknown.h** - Types 257, 1000, 10002 (special/unknown types)

All files are included by frame_disp_ef.h and called from DrawSmartEffectUI().
