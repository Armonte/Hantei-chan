#ifndef RHI_D3D9_FX_PARSE_H_GUARD
#define RHI_D3D9_FX_PARSE_H_GUARD
// Parser for the games' HLSL effect text (Shader/*.txt as shipped by UNI2, MBTL
// and DFCI). PLAN.md P3 step 1.
//
// Pure C++: no D3D, no Windows header, so it builds and is tested on any
// platform. The files are CP932 (Shift-JIS), Japanese comments and a 0x5C trail
// byte among them, so decode first (fx::Cp932ToUtf8) and never scan raw bytes.
//
// The dialect is a subset of D3DX effect text:
//   #define NAME value
//   struct NAME { members; };
//   texture name = NULL;                       effect-only, dropped by MakePassHlsl
//   sampler2D name = sampler_state { texture = <tex>; MagFilter = Linear; };
//   sampler  name = sampler_state { Texture = <tex>; };     DFCI writes it this way
//   TYPE name[array] [= default] [: SEMANTIC];              global uniform
//   TYPE name( args ) [: SEMANTIC] { body }                 function, kept as HLSL
//   technique NAME { pass NAME { VertexShader = compile vs_3_0 F();
//                                PixelShader = compile ps_3_0 G();
//                                AlphaBlendEnable = True; } }
//
// Everything is kept as text: the parser never evaluates a value. Comments are
// stripped (the span offsets stay aligned with the input, so line numbers are
// the file's own) and the text handed out is comment-free, which is what the
// D3DCompile path wants.
//
// See docs/render/fx_parse.md for the construct inventory of the three games.

#include <string>
#include <vector>

namespace fx {

// CP932 (Shift-JIS) bytes to UTF-8. Undefined bytes and pairs become U+FFFD.
std::string Cp932ToUtf8(const std::string& cp932);

// Drops // and /* */ comments, replacing them run-for-run with spaces so byte
// offsets and line numbers still match the input.
std::string StripComments(const std::string& text);

struct Annotation {
	std::string name;
	std::string value;
};

struct Uniform {
	std::string type;          // "float", "float4", "float4x4", "int", "static const int", ...
	std::string name;
	std::string arraySize;     // "" or the text inside the brackets ("4", "MAX_BONEMATRICES")
	std::string defaultValue;  // "" when the declaration has none
	std::string semantic;      // "COLOR0" text after the colon, "" when absent
	std::vector<Annotation> annotations;
	std::string decl;          // the declaration as written (comment-free, no trailing ';')
};

struct SamplerValue {          // one NAME = VALUE inside a sampler_state block
	std::string name;
	std::string value;
};

struct Sampler {
	std::string keyword;       // "sampler2D" or "sampler", as written
	std::string name;
	bool hasState = false;     // had a sampler_state block
	std::string texture;       // the bound texture name (from texture = <x>), "" when none
	std::vector<SamplerValue> values;   // every assignment in the block, in file order
	std::vector<Annotation> annotations;
};

struct Texture {
	std::string name;
	std::string defaultValue;  // "NULL" in every game file so far
};

struct Function {
	std::string name;
	std::string text;          // head + body, comment-free
};

struct ShaderRef {
	bool present = false;      // had a compile <profile> <entry>(...) clause
	std::string profile;       // "vs_3_0", "ps_3_0", "ps_2_0"
	std::string entry;         // the entry point function name
	std::string args;          // the text inside the entry point's parentheses
};

struct StateAssign {           // a pass-state assignment (AlphaBlendEnable = True;)
	std::string name;
	std::string value;
};

struct Pass {
	std::string name;
	ShaderRef vs;
	ShaderRef ps;
	std::vector<StateAssign> states;   // every other assignment in the pass, in file order
};

struct Technique {
	std::string name;
	std::vector<Pass> passes;
};

// A top-level declaration in file order, so MakePassHlsl can emit the ones the
// compiler needs in their original position. Techniques live in
// Effect::techniques and are never emitted.
struct Item {
	enum class Kind { Preproc, Struct, Uniform, Texture, Sampler, Function, Raw };
	Kind kind = Kind::Preproc;
	std::string text;          // exact text for Kind::Preproc / Kind::Struct
	int index = -1;            // into uniforms / textures / samplers / functions
};

struct Effect {
	std::vector<Uniform> uniforms;
	std::vector<Texture> textures;
	std::vector<Sampler> samplers;
	std::vector<Function> functions;
	std::vector<Technique> techniques;
	std::vector<Item> items;   // declarations only, in file order
};

// Parses decoded (UTF-8) effect text. Returns false and sets err (with the
// file's own line number) on a syntax error.
bool ParseEffect(const std::string& utf8, Effect& out, std::string& err);

const Technique* FindTechnique(const Effect&, const std::string& name);
const Pass* FindPass(const Technique&, const std::string& name);

// A plain-HLSL translation unit for one pass: every declaration above, the
// samplers as sampler2D NAME : register(sN) (declaration order, the way D3DX
// numbers them) and no techniques. D3DCompile builds the pass's two entry
// points from it: Pass::vs.entry and Pass::ps.entry.
std::string MakePassHlsl(const Effect&, const Technique&, const Pass&);

}  // namespace fx
#endif
