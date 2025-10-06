# Condition Type Categories

This directory contains the split implementation of IfDisplay() condition handlers.

Each file handles specific condition type(s) organized by functionality:

- **cond_input.h** - Types 1, 6, 7, 11, 31, 35 (input checks - lever, button, command)
- **cond_physics.h** - Types 4, 12, 13, 26, 29, 70 (position, vector, distance, borders)
- **cond_combat.h** - Types 3, 17, 27, 38, 51, 52, 54 (hit checks, damage, shield, throw)
- **cond_collision.h** - Types 14, 15, 19, 20 (box collisions, captures, reflections)
- **cond_state.h** - Types 5, 18, 21, 28, 30, 32, 36, 37, 41 (KO, character, facing, meter)
- **cond_control.h** - Types 8, 9, 10, 40 (random, loops, timers)
- **cond_misc.h** - Types 2, 16, 22, 23, 24, 25, 33, 34, 39, 42, 53, 55, 60 (misc/unknown)

All files are included by frame_disp_if.h and called from the main switch statement.
