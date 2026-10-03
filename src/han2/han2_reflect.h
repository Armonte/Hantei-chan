#pragma once
// Reflection records shared by the generated type headers (tools/ida/gen_cpp_types.py).
#include <cstdint>
struct Han2EnumValue { const char *name; int64_t value; };
struct Han2EnumInfo { const char *name; const Han2EnumValue *values; int count; bool flags; };
struct Han2FieldInfo { const char *name; uint16_t offset; uint8_t size; uint8_t count; uint8_t kind; const char *enumName; const char *comment; };
