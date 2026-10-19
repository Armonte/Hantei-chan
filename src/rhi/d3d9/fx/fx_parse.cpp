#include "rhi/d3d9/fx/fx_parse.h"

#include <cctype>
#include <cstdint>
#include <string>

namespace fx {
namespace {

#include "fx_cp932_table.inc"

const uint16_t kUndefined = 0xFFFFu;

void AppendUtf8(std::string& out, uint32_t cp)
{
	if (cp < 0x80) {
		out += char(cp);
	} else if (cp < 0x800) {
		out += char(0xC0 | (cp >> 6));
		out += char(0x80 | (cp & 0x3F));
	} else {
		out += char(0xE0 | (cp >> 12));
		out += char(0x80 | ((cp >> 6) & 0x3F));
		out += char(0x80 | (cp & 0x3F));
	}
}

std::string Trim(const std::string& s)
{
	size_t b = 0, e = s.size();
	while (b < e && (unsigned char)s[b] <= ' ') ++b;
	while (e > b && (unsigned char)s[e - 1] <= ' ') --e;
	return s.substr(b, e - b);
}

bool IsIdStart(char c) { return std::isalpha((unsigned char)c) || c == '_'; }
bool IsIdChar(char c) { return std::isalnum((unsigned char)c) || c == '_'; }

// Drops comments, replacing them with spaces so byte offsets and line numbers
// stay the input's own. A UTF-8 continuation byte is never 0x2F or 0x2A, so byte
// scanning cannot mistake Japanese text for a comment.
std::string StripCommentsImpl(const std::string& text)
{
	std::string o = text;
	size_t i = 0;
	while (i < o.size()) {
		if (o[i] == '/' && i + 1 < o.size() && o[i + 1] == '/') {
			while (i < o.size() && o[i] != '\n') o[i++] = ' ';
		} else if (o[i] == '/' && i + 1 < o.size() && o[i + 1] == '*') {
			o[i] = ' ';
			o[i + 1] = ' ';
			i += 2;
			while (i < o.size() && !(o[i] == '*' && i + 1 < o.size() && o[i + 1] == '/')) {
				if (o[i] != '\n') o[i] = ' ';
				++i;
			}
			if (i < o.size()) {
				o[i] = ' ';
				o[i + 1] = ' ';
				i += 2;
			}
		} else {
			++i;
		}
	}
	return o;
}

struct Scanner {
	explicit Scanner(const std::string& text) : s(text) {}

	const std::string& s;
	size_t i = 0;
	int line = 1;

	bool eof() const { return i >= s.size(); }
	char cur() const { return i < s.size() ? s[i] : '\0'; }
	void adv()
	{
		if (i >= s.size()) return;
		if (s[i] == '\n') ++line;
		++i;
	}
	void ws()
	{
		while (i < s.size() && (unsigned char)s[i] <= ' ')
			adv();
	}
	std::string ident()
	{
		ws();
		std::string o;
		while (i < s.size() && IsIdChar(s[i])) {
			o += s[i];
			adv();
		}
		return o;
	}
	bool eat(char c)
	{
		ws();
		if (cur() != c) return false;
		adv();
		return true;
	}
	std::string err(int atLine, const std::string& what) const
	{
		return "line " + std::to_string(atLine) + ": " + what;
	}
	// Inner text of a bracket pair, starting on the opening bracket.
	std::string span(char open, char close, std::string& error, const char* what)
	{
		ws();
		if (cur() != open) {
			error = err(line, std::string("expected ") + what);
			return std::string();
		}
		const int startLine = line;
		adv();
		const size_t b = i;
		int depth = 1;
		while (!eof()) {
			const char c = cur();
			if (c == open) ++depth;
			else if (c == close && --depth == 0) {
				const std::string inner = s.substr(b, i - b);
				adv();
				return inner;
			}
			adv();
		}
		error = err(startLine, std::string("unterminated ") + what);
		return std::string();
	}
	// Reads to the ';' that closes an assignment or declaration, then eats it.
	// Nested () [] {} do not end it. Returns false with err on a stray '}'.
	bool toSemicolon(std::string& out, std::string& error)
	{
		ws();
		const size_t b = i;
		int depth = 0;
		while (!eof()) {
			const char c = cur();
			if (c == '(' || c == '[' || c == '{') ++depth;
			else if (c == ')' || c == ']') {
				if (depth > 0) --depth;
			} else if (c == '}') {
				if (depth == 0) {
					error = err(line, "expected ';' before '}'");
					return false;
				}
				--depth;
			} else if (c == ';' && depth == 0) {
				out = Trim(s.substr(b, i - b));
				adv();
				return true;
			}
			adv();
		}
		error = err(line, "expected ';' before end of file");
		return false;
	}
};

void AddAnnotation(std::vector<Annotation>& dst, const std::string& block)
{
	// D3DX annotation entries are TYPE NAME = VALUE; (string UIName = "..."),
	// so the first token is the annotation's type and the second its name.
	size_t i = 0;
	while (i < block.size()) {
		while (i < block.size() && (unsigned char)block[i] <= ' ') ++i;
		const size_t b = i;
		while (i < block.size() && IsIdChar(block[i])) ++i;
		const std::string first = block.substr(b, i - b);
		while (i < block.size() && (unsigned char)block[i] <= ' ') ++i;
		std::string name = first;
		if (i < block.size() && IsIdStart(block[i])) {
			const size_t nb = i;
			while (i < block.size() && IsIdChar(block[i])) ++i;
			name = block.substr(nb, i - nb);
			while (i < block.size() && (unsigned char)block[i] <= ' ') ++i;
		}
		std::string value;
		if (i < block.size() && block[i] == '=') {
			++i;
			const size_t vb = i;
			while (i < block.size() && block[i] != ';') ++i;
			value = Trim(block.substr(vb, i - vb));
		}
		if (i < block.size() && block[i] == ';') ++i;
		if (!first.empty()) dst.push_back({name, value});
	}
}

std::string StripAngles(const std::string& v)
{
	std::string o = Trim(v);
	if (o.size() >= 2 && o.front() == '<' && o.back() == '>') o = o.substr(1, o.size() - 2);
	return Trim(o);
}

// An effect-only angle annotation block (NAME < string UIName = "..."; >) is not
// valid HLSL, so it is dropped from a uniform declaration before MakePassHlsl
// emits it. A '<' only opens an annotation when the matching '>' is followed by
// the declaration's own ';', which leaves a comparison like (a < b) alone.
std::string DropAnnotations(const std::string& decl)
{
	std::string o;
	for (size_t i = 0; i < decl.size(); ++i) {
		if (decl[i] != '<') {
			o += decl[i];
			continue;
		}
		const size_t close = decl.find('>', i + 1);
		if (close == std::string::npos) {
			o += decl[i];
			continue;
		}
		size_t after = close + 1;
		while (after < decl.size() && (unsigned char)decl[after] <= ' ') ++after;
		if (after < decl.size() && decl[after] == ';') {
			i = close;   // skip the whole <...>
			continue;
		}
		if (after >= decl.size()) {
			i = close;
			continue;
		}
		o += decl[i];
	}
	return Trim(o);
}

bool ParseShaderRef(const std::string& value, ShaderRef& out)
{
	Scanner s(value);
	if (s.ident() != "compile") return false;
	out.profile = s.ident();
	out.entry = s.ident();
	s.ws();
	if (s.cur() != '(') return false;
	std::string error;
	out.args = Trim(s.span('(', ')', error, "'('"));
	out.present = !out.entry.empty();
	return out.present;
}

}  // namespace

std::string Cp932ToUtf8(const std::string& cp932)
{
	std::string out;
	out.reserve(cp932.size());
	size_t i = 0;
	while (i < cp932.size()) {
		const unsigned char b = (unsigned char)cp932[i];
		if (b < 0x80) {
			out += char(b);
			++i;
			continue;
		}
		if (kCp932Lead[b] != kUndefined && i + 1 < cp932.size()) {
			const unsigned char t = (unsigned char)cp932[i + 1];
			if (t >= 0x40 && t != 0x7F && t <= 0xFC) {
				const uint32_t cp = kCp932Double[kCp932Lead[b] + (t - 0x40)];
				if (cp != kUndefined) {
					AppendUtf8(out, cp);
					i += 2;
					continue;
				}
			}
		}
		AppendUtf8(out, kUndefined);   // U+FFFD
		++i;
	}
	return out;
}

std::string StripComments(const std::string& text) { return StripCommentsImpl(text); }

const Technique* FindTechnique(const Effect& e, const std::string& name)
{
	for (const Technique& t : e.techniques)
		if (t.name == name) return &t;
	return nullptr;
}

const Pass* FindPass(const Technique& t, const std::string& name)
{
	for (const Pass& p : t.passes)
		if (p.name == name) return &p;
	return nullptr;
}

bool ParseEffect(const std::string& utf8, Effect& out, std::string& err)
{
	const std::string src = StripCommentsImpl(utf8);
	Scanner s(src);

	while (true) {
		s.ws();
		if (s.eof()) break;

		// --- preprocessor -------------------------------------------------
		if (s.cur() == '#') {
			const size_t b = s.i;
			while (!s.eof() && s.cur() != '\n') {
				if (s.cur() == '\\' && s.i + 1 < s.s.size() && s.s[s.i + 1] == '\n') {
					s.adv();
					s.adv();
					continue;
				}
				s.adv();
			}
			Item it;
			it.kind = Item::Kind::Preproc;
			it.text = Trim(s.s.substr(b, s.i - b));
			out.items.push_back(it);
			continue;
		}

		const size_t start = s.i;
		const std::string kw = s.ident();
		if (kw.empty()) {
			err = s.err(s.line, std::string("unexpected character '") + s.cur() + "'");
			return false;
		}

		// --- technique ----------------------------------------------------
		if (kw == "technique") {
			Technique t;
			t.name = s.ident();
			if (t.name.empty()) {
				err = s.err(s.line, "technique without a name");
				return false;
			}
			s.ws();
			if (s.cur() == '<') {
				std::string block = s.span('<', '>', err, "technique annotation block");
				if (!err.empty()) return false;
				(void)block;
			}
			if (!s.eat('{')) {
				err = s.err(s.line, "expected '{' after technique " + t.name);
				return false;
			}
			while (true) {
				s.ws();
				if (s.eof()) {
					err = s.err(s.line, "unterminated technique " + t.name);
					return false;
				}
				if (s.cur() == '}') {
					s.adv();
					break;
				}
				const std::string what = s.ident();
				if (what == "pass") {
					Pass p;
					p.name = s.ident();
					if (p.name.empty()) {
						err = s.err(s.line, "pass without a name in technique " + t.name);
						return false;
					}
					s.ws();
					if (s.cur() == '<') {
						std::string block = s.span('<', '>', err, "pass annotation block");
						if (!err.empty()) return false;
						(void)block;
					}
					if (!s.eat('{')) {
						err = s.err(s.line, "expected '{' after pass " + p.name);
						return false;
					}
					while (true) {
						s.ws();
						if (s.eof()) {
							err = s.err(s.line, "unterminated pass " + p.name);
							return false;
						}
						if (s.cur() == '}') {
							s.adv();
							break;
						}
						const std::string key = s.ident();
						if (key.empty()) {
							err = s.err(s.line, std::string("unexpected '") + s.cur() + "' in pass " + p.name);
							return false;
						}
						if (!s.eat('=')) {
							err = s.err(s.line, "expected '=' after " + key + " in pass " + p.name);
							return false;
						}
						std::string value;
						if (!s.toSemicolon(value, err)) return false;
						if (key == "VertexShader" || key == "vertexShader") {
							if (!ParseShaderRef(value, p.vs)) {
								err = s.err(s.line, "unparsable VertexShader value: " + value);
								return false;
							}
						} else if (key == "PixelShader" || key == "pixelShader") {
							if (!ParseShaderRef(value, p.ps)) {
								err = s.err(s.line, "unparsable PixelShader value: " + value);
								return false;
							}
						} else {
							p.states.push_back({key, value});
						}
					}
					t.passes.push_back(p);
				} else if (what.empty()) {
					err = s.err(s.line, std::string("unexpected '") + s.cur() + "' in technique " + t.name);
					return false;
				} else {
					if (!s.eat('=')) {
						err = s.err(s.line, "expected '=' after " + what + " in technique " + t.name);
						return false;
					}
					std::string value;
					if (!s.toSemicolon(value, err)) return false;
				}
			}
			out.techniques.push_back(std::move(t));
			continue;
		}

		// --- struct -------------------------------------------------------
		if (kw == "struct") {
			s.ident();
			s.ws();
			if (s.cur() == '<') {
				std::string block = s.span('<', '>', err, "struct annotation block");
				if (!err.empty()) return false;
				(void)block;
			}
			s.ws();
			if (s.cur() != '{') {
				err = s.err(s.line, "expected '{' after struct name");
				return false;
			}
			(void)s.span('{', '}', err, "struct body");
			if (!err.empty()) return false;
			if (!s.eat(';')) {
				err = s.err(s.line, "expected ';' after struct body");
				return false;
			}
			Item it;
			it.kind = Item::Kind::Struct;
			it.text = Trim(src.substr(start, s.i - start));
			out.items.push_back(it);
			continue;
		}

		// --- texture ------------------------------------------------------
		if (kw == "texture") {
			Texture t;
			t.name = s.ident();
			if (t.name.empty()) {
				err = s.err(s.line, "texture without a name");
				return false;
			}
			s.ws();
			while (s.cur() == '<') {
				std::vector<Annotation> ann;
				AddAnnotation(ann, s.span('<', '>', err, "texture annotation block"));
				if (!err.empty()) return false;
			}
			if (s.eat('=')) {
				std::string value;
				if (!s.toSemicolon(value, err)) return false;
				t.defaultValue = value;
			} else if (!s.eat(';')) {
				err = s.err(s.line, "expected ';' after texture " + t.name);
				return false;
			}
			Item it;
			it.kind = Item::Kind::Texture;
			it.index = (int)out.textures.size();
			out.items.push_back(it);
			out.textures.push_back(std::move(t));
			continue;
		}

		// --- sampler ------------------------------------------------------
		if (kw == "sampler2D" || kw == "sampler") {
			Sampler sm;
			sm.keyword = kw;
			sm.name = s.ident();
			if (sm.name.empty()) {
				err = s.err(s.line, "sampler without a name");
				return false;
			}
			s.ws();
			while (s.cur() == '<') {
				AddAnnotation(sm.annotations, s.span('<', '>', err, "sampler annotation block"));
				if (!err.empty()) return false;
			}
			s.ws();
			if (s.eat('=')) {
				s.ws();
				const std::string rhs = (s.i < s.s.size() && IsIdStart(s.cur())) ? s.ident() : std::string();
				s.ws();
				if (rhs == "sampler_state" || s.cur() == '{') {
					std::string block = s.span('{', '}', err, "sampler_state block");
					if (!err.empty()) return false;
					sm.hasState = true;
					size_t p = 0;
					while (p < block.size()) {
						while (p < block.size() && (unsigned char)block[p] <= ' ') ++p;
						const size_t nb = p;
						while (p < block.size() && IsIdChar(block[p])) ++p;
						std::string name = block.substr(nb, p - nb);
						if (name.empty()) {
							++p;
							continue;
						}
						while (p < block.size() && (unsigned char)block[p] <= ' ') ++p;
						if (p >= block.size() || block[p] != '=') continue;
						++p;
						const size_t vb = p;
						while (p < block.size() && block[p] != ';') ++p;
						const std::string value = Trim(block.substr(vb, p - vb));
						sm.values.push_back({name, value});
						if (p < block.size()) ++p;
					}
					for (const SamplerValue& v : sm.values) {
						if (sm.texture.empty() && (v.name == "texture" || v.name == "Texture"))
							sm.texture = StripAngles(v.value);
					}
					if (!s.eat(';')) {
						err = s.err(s.line, "expected ';' after sampler_state of " + sm.name);
						return false;
					}
				} else {
					std::string value;
					if (!s.toSemicolon(value, err)) return false;
					if (!rhs.empty()) sm.values.push_back({rhs, value});
				}
			} else if (!s.eat(';')) {
				err = s.err(s.line, "expected ';' after sampler " + sm.name);
				return false;
			}
			Item it;
			it.kind = Item::Kind::Sampler;
			it.index = (int)out.samplers.size();
			out.items.push_back(it);
			out.samplers.push_back(std::move(sm));
			continue;
		}

		// --- global uniform or function -----------------------------------
		s.i = start;
		{
			int ln = 1;
			for (size_t k = 0; k < start; ++k)
				if (src[k] == '\n') ++ln;
			s.line = ln;
		}
		std::vector<std::string> toks;
		while (true) {
			s.ws();
			if (s.eof()) {
				err = s.err(s.line, "expected ';' before end of file");
				return false;
			}
			const char c = s.cur();
			if (c == '(' || c == '[' || c == '=' || c == ';' || c == ':' || c == '<') break;
			if (!IsIdChar(c)) {
				err = s.err(s.line, std::string("unexpected '") + c + "' in a global declaration");
				return false;
			}
			toks.push_back(s.ident());
		}
		if (toks.size() < 2) {
			err = s.err(s.line, "unrecognised top-level declaration");
			return false;
		}
		Uniform u;
		u.name = toks.back();
		std::string type;
		for (size_t k = 0; k + 1 < toks.size(); ++k) {
			if (!type.empty()) type += ' ';
			type += toks[k];
		}
		u.type = type;

		while (s.cur() == '<') {
			AddAnnotation(u.annotations, s.span('<', '>', err, "uniform annotation block"));
			if (!err.empty()) return false;
			s.ws();
		}

		if (s.cur() == '[') {
			u.arraySize = Trim(s.span('[', ']', err, "array size"));
			if (!err.empty()) return false;
			s.ws();
		}

		if (s.cur() == '(') {
			// A function or a prototype.
			s.span('(', ')', err, "function parameter list");
			if (!err.empty()) return false;
			s.ws();
			// An optional return semantic sits between ')' and the body. It has no
			// ';' when a body follows, so scan straight to '{' or ';'.
			while (s.cur() == ':') {
				s.adv();
				while (!s.eof() && s.cur() != '{' && s.cur() != ';') s.adv();
			}
			s.ws();
			if (s.cur() == ';') {
				s.adv();
				Item it;
				it.kind = Item::Kind::Raw;
				it.text = Trim(src.substr(start, s.i - start));
				out.items.push_back(it);
				continue;
			}
			s.ws();
			if (s.cur() != '{') {
				err = s.err(s.line, "expected '{' or ';' after " + u.name);
				return false;
			}
			const size_t bodyStart = s.i;
			s.span('{', '}', err, "function body");
			if (!err.empty()) return false;
			while (s.cur() == ';') s.adv();
			Function f;
			f.name = u.name;
			f.text = Trim(src.substr(start, bodyStart - start)) + " " + Trim(src.substr(bodyStart, s.i - bodyStart));
			Item it;
			it.kind = Item::Kind::Function;
			it.index = (int)out.functions.size();
			out.items.push_back(it);
			out.functions.push_back(std::move(f));
			continue;
		}

		// A plain uniform: optional default, optional semantic, then ';'.
		if (s.cur() == '=') {
			s.adv();
			std::string value;
			if (!s.toSemicolon(value, err)) return false;
			u.defaultValue = value;
		} else if (s.cur() == ':') {
			s.adv();
			std::string sem;
			if (!s.toSemicolon(sem, err)) return false;
			u.semantic = sem;
		} else if (s.cur() == ';') {
			s.adv();
		} else {
			err = s.err(s.line, "unrecognised declaration for " + u.name);
			return false;
		}
		u.decl = Trim(src.substr(start, s.i - start));
		if (!u.decl.empty() && u.decl.back() == ';') u.decl.pop_back();
		u.decl = Trim(u.decl);
		Item it;
		it.kind = Item::Kind::Uniform;
		it.index = (int)out.uniforms.size();
		out.items.push_back(it);
		out.uniforms.push_back(std::move(u));
	}

	return true;
}

std::string MakePassHlsl(const Effect& e, const Technique& t, const Pass& p)
{
	std::string out;
	out += "// hantei-chan effect pass " + t.name + " / " + p.name + "\n";

	for (const Item& it : e.items) {
		switch (it.kind) {
		case Item::Kind::Texture:
			break;
		case Item::Kind::Sampler: {
			const Sampler& sm = e.samplers[(size_t)it.index];
			out += "sampler2D " + sm.name + " : register(s" + std::to_string(it.index) + ");\n";
			break;
		}
		case Item::Kind::Preproc:
		case Item::Kind::Struct:
		case Item::Kind::Raw:
			out += it.text + "\n";
			break;
		case Item::Kind::Uniform:
			out += DropAnnotations(e.uniforms[(size_t)it.index].decl) + ";\n";
			break;
		case Item::Kind::Function:
			out += e.functions[(size_t)it.index].text + "\n";
			break;
		}
	}
	return out;
}

}  // namespace fx
