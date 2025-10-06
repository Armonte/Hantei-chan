#ifndef FRAME_DISP_H_GUARD
#define FRAME_DISP_H_GUARD

// ============================================================================
// MBAACC Frame Display UI - Modular Header
// ============================================================================
// This is the modular structure for frame display UI
// Each display system is in its own file for better organization
//
// Architecture:
// - frame_disp/frame_disp_common.h - Common helpers and utilities
// - frame_disp/frame_disp_if.h - Condition (IF) display
// - frame_disp/frame_disp_ef.h - Effect (EF) display (with sub-modules in effects/)
// - frame_disp/frame_disp_as.h - Action State (AS) display
// - frame_disp/frame_disp_at.h - Attack (AT) display
// - frame_disp/frame_disp_af.h - Animation Frame (AF) display
// ============================================================================

// Common helpers and utilities
#include "frame_disp/frame_disp_common.h"

// Individual display systems
#include "frame_disp/frame_disp_if.h"      // Condition (IF) display
#include "frame_disp/frame_disp_ef.h"      // Effect (EF) display
#include "frame_disp/frame_disp_as.h"      // Action State (AS) display
#include "frame_disp/frame_disp_at.h"      // Attack (AT) display
#include "frame_disp/frame_disp_af.h"      // Animation Frame (AF) display

#endif /* FRAME_DISP_H_GUARD */
