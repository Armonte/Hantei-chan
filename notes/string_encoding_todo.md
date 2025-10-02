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

**Remove conversion system - keep everything in Shift-JIS:**

1. Remove `utf8` flag check
2. Remove `sj2utf8()` conversion
3. Don't set `header[31] = 0xFF`
4. Keep strings in original Shift-JIS encoding
5. (Optional) Add proper metadata section for extra data

## Benefits:

- Full compatibility with original Melty Blood
- No encoding confusion
- Files remain in original format
- Can still add metadata without breaking encoding

## Implementation:

1. Remove line 40 in `framedata.cpp`: `bool utf8 = ((unsigned char*)data)[31] == 0xFF;`
2. Remove line 670 in `framedata_load.cpp`: conversion call
3. Remove lines 112, 166 in `framedata.cpp`: `header[31] = 0xFF;`
4. Pass `utf8 = false` or remove parameter from `fd_main_load()`

## Note:
u4 also mentioned that we could add extra metadata to the file without breaking compatibility.
