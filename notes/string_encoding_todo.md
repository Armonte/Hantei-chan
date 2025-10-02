# String Encoding Issue - TODO

## Problem (reported by u4):

Hantei-chan currently converts strings between Shift-JIS and UTF-8, using a modified byte (`header[31] = 0xFF`) to track if a file was saved by this tool.

## Current Behavior:

**Loading** (`framedata.cpp:40`):
- Checks `header[31] == 0xFF`
  - If true: strings are UTF-8 (saved by Hantei-chan)
  - If false: strings are Shift-JIS (original game file)
- Converts Shift-JIS → UTF-8 if needed (`framedata_load.cpp:670`)

**Saving** (`framedata.cpp:112, 166`):
- Sets `header[31] = 0xFF` (marks as modified)
- Saves strings as UTF-8

## u4's Recommendation:

**Stop converting new files, but maintain backwards compatibility:**

The goal is to:
1. Stop forcing UTF-8 conversion on new saves
2. Keep original Shift-JIS encoding when saving
3. Still support reading files that were already converted to UTF-8 by older Hantei-chan versions

## Backwards Compatibility Requirement:

Some users have files where strings were already converted to UTF-8 by older Hantei-chan versions. We need to:
- **Keep reading** the `header[31] == 0xFF` flag to detect UTF-8 files
- **Keep the conversion** when loading UTF-8 files
- **Stop setting** `header[31] = 0xFF` when saving NEW files
- **Preserve the original encoding** when re-saving files

## Benefits:

- Full compatibility with original Melty Blood format
- Backwards compatible with UTF-8 files from old Hantei-chan
- No encoding confusion going forward
- Files remain in original format when saved

## Implementation:

**What to change:**
1. **KEEP** line 40 in `framedata.cpp`: `bool utf8 = ((unsigned char*)data)[31] == 0xFF;` (for reading old files)
2. **KEEP** line 670 in `framedata_load.cpp`: conversion call (for old UTF-8 files)
3. **REMOVE** lines 112, 166 in `framedata.cpp`: `header[31] = 0xFF;` (stop marking new saves)
4. **PRESERVE** the original `header[31]` value when saving (don't modify it)

**Result:**
- Original game files (Shift-JIS): Load as Shift-JIS, save as Shift-JIS
- Old Hantei-chan files (UTF-8): Load as UTF-8, save as UTF-8 (preserve marker)
- New edits to original files: Preserve Shift-JIS encoding

## Note:
u4 also mentioned that we could add extra metadata to the file without breaking compatibility.
