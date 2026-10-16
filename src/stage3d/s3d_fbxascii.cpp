// ASCII FBX 6.1.0 stage reader (DFCI ships its stages only in this form, the UNI[st]/[cl-r] arcade build too; UNI2 keeps six
// beside their .bin). Ported from PovertyCaster's pc-stage3d FbxAscii.cpp (our own code, same project family) onto s3d::Model;
// the parser is unchanged apart from names. stage3d_test checks it against the shipped .bin on the UNI2 stages with both forms.
#include "s3d_fbxascii.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <unordered_map>

namespace s3d {
namespace {

constexpr double kKTimePerSecond = 46186158000.0;   // FBX time units
constexpr double kFps = 30.0;
constexpr double kPi = 3.14159265358979323846;

// ---------------------------------------------------------------- generic tree
struct FNode {
    std::string name, value;
    std::vector<FNode> kids;
    const FNode* find(const char* n) const {
        for (const FNode& k : kids) if (k.name == n) return &k;
        return nullptr;
    }
};

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) --b;
    return s.substr(a, b - a);
}

bool parseTree(const std::string& text, FNode& root, std::string& err) {
    std::vector<FNode*> stack{&root};
    size_t pos = 0;
    const size_t n = text.size();
    auto nextLine = [&](std::string& out) {
        if (pos >= n) return false;
        size_t e = text.find('\n', pos);
        if (e == std::string::npos) e = n;
        out.assign(text, pos, e - pos);
        pos = e + 1;
        return true;
    };
    std::string line;
    while (nextLine(line)) {
        std::string t = trim(line);
        if (t.empty() || t[0] == ';') continue;
        // continuation lines (an array wrapped onto the next line) start with a comma
        for (;;) {
            size_t save = pos;
            std::string nl;
            if (!nextLine(nl)) break;
            const std::string nt = trim(nl);
            if (!nt.empty() && nt[0] == ',') t += nt;
            else { pos = save; break; }
        }
        if (t == "}") {
            if (stack.size() <= 1) { err = "unbalanced '}'"; return false; }
            stack.pop_back();
            continue;
        }
        const size_t c = t.find(':');
        if (c == std::string::npos) continue;
        FNode node;
        node.name = t.substr(0, c);
        std::string rest = trim(t.substr(c + 1));
        const bool open = !rest.empty() && rest.back() == '{';
        if (open) rest = trim(rest.substr(0, rest.size() - 1));
        node.value = rest;
        FNode* parent = stack.back();
        parent->kids.push_back(std::move(node));
        if (open) stack.push_back(&parent->kids.back());
    }
    if (stack.size() != 1) { err = "unterminated block"; return false; }
    return true;
}

std::vector<std::string> splitTokens(const std::string& v) {
    std::vector<std::string> out;
    std::string cur;
    bool q = false;
    for (char ch : v) {
        if (ch == '"') { q = !q; continue; }
        if (ch == ',' && !q) { out.push_back(trim(cur)); cur.clear(); continue; }
        cur += ch;
    }
    out.push_back(trim(cur));
    return out;
}

std::vector<double> parseNums(const std::string& v) {
    std::vector<double> out;
    const char* p = v.c_str();
    while (*p) {
        while (*p == ',' || *p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
        if (!*p) break;
        char* end = nullptr;
        const double d = std::strtod(p, &end);
        if (end == p) break;
        out.push_back(d);
        p = end;
    }
    return out;
}

std::string stripPrefix(const std::string& s) {
    const size_t k = s.find("::");
    return k == std::string::npos ? s : s.substr(k + 2);
}

// ---------------------------------------------------------------- math (row-vector, D3D conventions)
Mat4 scaleM(double x, double y, double z) { Mat4 m = MatIdentity(); m[0] = float(x); m[5] = float(y); m[10] = float(z); return m; }
Mat4 transM(double x, double y, double z) { Mat4 m = MatIdentity(); m[12] = float(x); m[13] = float(y); m[14] = float(z); return m; }
Mat4 rotX(double a) { Mat4 m = MatIdentity(); const float c = float(std::cos(a)), s = float(std::sin(a)); m[5] = c; m[6] = s; m[9] = -s; m[10] = c; return m; }
Mat4 rotY(double a) { Mat4 m = MatIdentity(); const float c = float(std::cos(a)), s = float(std::sin(a)); m[0] = c; m[2] = -s; m[8] = s; m[10] = c; return m; }
Mat4 rotZ(double a) { Mat4 m = MatIdentity(); const float c = float(std::cos(a)), s = float(std::sin(a)); m[0] = c; m[1] = s; m[4] = -s; m[5] = c; return m; }
Mat4 eulerXYZ(const double d[3]) {   // FBX XYZ order: X is applied first
    return MatMul(MatMul(rotX(d[0] * kPi / 180.0), rotY(d[1] * kPi / 180.0)), rotZ(d[2] * kPi / 180.0));
}

// ---------------------------------------------------------------- per-model data
struct Key { double t = 0, v = 0, rs = 0, nls = 0; char interp = 'U'; };
struct Channel {
    double def = 0;
    std::vector<Key> keys;
};

struct FbxModel {
    std::string name, type;
    int parent = -1;   // index into models, -1 = scene root
    std::vector<int> children;
    double lcl[3][3] = {{0, 0, 0}, {0, 0, 0}, {1, 1, 1}};   // T, R(deg), S
    double rotPivot[3] = {}, scalePivot[3] = {}, rotOff[3] = {}, scaleOff[3] = {}, preRot[3] = {}, postRot[3] = {};
    double adding = 0;
    Channel ch[3][3];   // [T/R/S][X/Y/Z]
    std::vector<int> materials, textures;   // indices into the stage-wide lists
    // geometry
    std::vector<double> verts;
    std::vector<int> pvi;
    const FNode* geom = nullptr;
};

double frameSeconds(const FbxModel& m, double& t0, double& t1, bool& any) {
    any = false;
    t0 = 1e300; t1 = -1e300;
    for (auto& g : m.ch) for (auto& c : g)
        if (!c.keys.empty()) { any = true; t0 = std::min(t0, c.keys.front().t); t1 = std::max(t1, c.keys.back().t); }
    return any ? (t1 - t0) / kKTimePerSecond : 0.0;
}

double evalChannel(const Channel& c, double tKt) {
    const auto& k = c.keys;
    if (k.empty()) return c.def;
    if (tKt <= k.front().t) return k.front().v;
    if (tKt >= k.back().t) return k.back().v;
    size_t i = 0;
    while (i + 1 < k.size() && k[i + 1].t <= tKt) ++i;
    const Key &a = k[i], &b = k[i + 1];
    const double dt = (b.t - a.t) / kKTimePerSecond;
    const double u = (tKt - a.t) / (b.t - a.t);
    if (a.interp == 'C') return a.v;
    if (a.interp == 'L') return a.v + (b.v - a.v) * u;
    // cubic Hermite: slopes are per second; key a carries (right slope, next-left slope)
    const double m0 = a.rs * dt, m1 = a.nls * dt;
    const double u2 = u * u, u3 = u2 * u;
    return (2 * u3 - 3 * u2 + 1) * a.v + (u3 - 2 * u2 + u) * m0 + (-2 * u3 + 3 * u2) * b.v + (u3 - u2) * m1;
}

Mat4 localMatrix(const FbxModel& m, double tKt, bool animated) {
    double v[3][3];
    for (int g = 0; g < 3; ++g)
        for (int c = 0; c < 3; ++c) v[g][c] = animated ? evalChannel(m.ch[g][c], tKt) : m.ch[g][c].def;
    const double* pv = m.scalePivot;
    const Mat4 spInv = transM(-pv[0], -pv[1], -pv[2]);
    // row-vector order = reverse of the column formula T Roff Rp Rpre R Rpost^-1 Rp^-1 Soff Sp S Sp^-1
    Mat4 r = MatMul(spInv, scaleM(v[2][0], v[2][1], v[2][2]));
    r = MatMul(r, transM(pv[0], pv[1], pv[2]));
    r = MatMul(r, transM(m.scaleOff[0], m.scaleOff[1], m.scaleOff[2]));
    r = MatMul(r, transM(-m.rotPivot[0], -m.rotPivot[1], -m.rotPivot[2]));
    double post[3] = {-m.postRot[0], -m.postRot[1], -m.postRot[2]};
    if (m.postRot[0] != 0 || m.postRot[1] != 0 || m.postRot[2] != 0) r = MatMul(r, eulerXYZ(post));
    r = MatMul(r, eulerXYZ(v[1]));
    if (m.preRot[0] != 0 || m.preRot[1] != 0 || m.preRot[2] != 0) r = MatMul(r, eulerXYZ(m.preRot));
    r = MatMul(r, transM(m.rotPivot[0], m.rotPivot[1], m.rotPivot[2]));
    r = MatMul(r, transM(m.rotOff[0], m.rotOff[1], m.rotOff[2]));
    return MatMul(r, transM(v[0][0], v[0][1], v[0][2]));
}

// ---------------------------------------------------------------- keys
void parseKeys(const std::string& value, Channel& ch) {
    const std::vector<std::string> tk = splitTokens(value);
    auto isNum = [](const std::string& s) { return !s.empty() && (std::isdigit(static_cast<unsigned char>(s[0])) || s[0] == '-' || s[0] == '.' || s[0] == '+'); };
    auto isKeyStart = [&](size_t i) { return i + 2 < tk.size() && isNum(tk[i]) && isNum(tk[i + 1]) && (tk[i + 2] == "C" || tk[i + 2] == "L" || tk[i + 2] == "U"); };
    size_t i = 0;
    while (i < tk.size()) {
        if (!isKeyStart(i)) { ++i; continue; }
        Key k;
        k.t = std::strtod(tk[i].c_str(), nullptr);
        k.v = std::strtod(tk[i + 1].c_str(), nullptr);
        k.interp = tk[i + 2][0];
        size_t j = i + 3;
        std::vector<double> nums;
        while (j < tk.size() && !isKeyStart(j)) {
            if (isNum(tk[j])) nums.push_back(std::strtod(tk[j].c_str(), nullptr));
            ++j;
        }
        if (k.interp == 'U' && nums.size() >= 2) { k.rs = nums[0]; k.nls = nums[1]; }
        ch.keys.push_back(k);
        i = j;
    }
}

// ---------------------------------------------------------------- geometry
struct Layer {
    const FNode* node = nullptr;
    std::string mapping, reference;
    std::vector<double> data;
    std::vector<double> index;
    bool has() const { return node != nullptr; }
};

Layer loadLayer(const FNode& geom, const char* name, const char* dataKey, const char* indexKey) {
    Layer l;
    for (const FNode& k : geom.kids) {
        if (k.name != name) continue;
        l.node = &k;
        if (const FNode* m = k.find("MappingInformationType")) l.mapping = splitTokens(m->value)[0];
        if (const FNode* r = k.find("ReferenceInformationType")) l.reference = splitTokens(r->value)[0];
        if (const FNode* d = k.find(dataKey)) l.data = parseNums(d->value);
        if (indexKey) if (const FNode* x = k.find(indexKey)) l.index = parseNums(x->value);
        break;
    }
    return l;
}

// element index for (polygon, polygon-vertex running index, control point)
long layerIndex(const Layer& l, long poly, long pvIdx, long cp) {
    long i = 0;
    if (l.mapping == "ByPolygonVertex") i = pvIdx;
    else if (l.mapping == "ByVertice" || l.mapping == "ByControlPoint") i = cp;
    else if (l.mapping == "ByPolygon") i = poly;
    else i = 0;   // AllSame
    if (l.reference == "IndexToDirect") {
        if (i < 0 || i >= static_cast<long>(l.index.size())) return -1;
        return static_cast<long>(l.index[i]);
    }
    return i;
}

// element index when the layer is a plain per-polygon list (material ids): AllSame -> its single entry
long layerIndexDirect(const Layer& l, long poly) {
    if (l.data.empty()) return 0;
    if (l.mapping == "ByPolygon") return poly < static_cast<long>(l.data.size()) ? static_cast<long>(l.data[poly]) : 0;
    return static_cast<long>(l.data[0]);
}



struct MatInfo { std::string name; float v[17] = {}; };
struct Scene {
    std::vector<FbxModel> models;
    std::map<std::string, int> modelByName;
    std::vector<MatInfo> mats;
    std::map<std::string, int> matByName;
    std::vector<std::string> texFile;   // per Texture object: dds name
    std::map<std::string, int> texByName;
};

void readObjects(const FNode* objects, Scene& sc) {
    auto& models = sc.models;
    auto& modelByName = sc.modelByName;
    auto& mats = sc.mats;
    auto& matByName = sc.matByName;
    auto& texFile = sc.texFile;
    auto& texByName = sc.texByName;
    for (const FNode& o : objects->kids) {
        const auto tk = splitTokens(o.value);
        if (o.name == "Model" && tk.size() >= 2) {
            FbxModel m;
            m.name = stripPrefix(tk[0]);
            m.type = tk[1];
            if (const FNode* props = o.find("Properties60")) {
                for (const FNode& p : props->kids) {
                    if (p.name != "Property") continue;
                    const auto pt = splitTokens(p.value);
                    if (pt.size() < 4) continue;
                    const std::string& pn = pt[0];
                    auto vec3 = [&](double* d) { for (int i = 0; i < 3 && 3 + i < static_cast<int>(pt.size()); ++i) d[i] = std::strtod(pt[3 + i].c_str(), nullptr); };
                    if (pn == "Lcl Translation") vec3(m.lcl[0]);
                    else if (pn == "Lcl Rotation") vec3(m.lcl[1]);
                    else if (pn == "Lcl Scaling") vec3(m.lcl[2]);
                    else if (pn == "RotationPivot") vec3(m.rotPivot);
                    else if (pn == "ScalingPivot") vec3(m.scalePivot);
                    else if (pn == "RotationOffset") vec3(m.rotOff);
                    else if (pn == "ScalingOffset") vec3(m.scaleOff);
                    else if (pn == "PreRotation") vec3(m.preRot);
                    else if (pn == "PostRotation") vec3(m.postRot);
                    else if (pn == "adding") m.adding = std::strtod(pt[3].c_str(), nullptr);
                }
            }
            for (int g = 0; g < 3; ++g) for (int c = 0; c < 3; ++c) m.ch[g][c].def = m.lcl[g][c];
            if (const FNode* v = o.find("Vertices")) m.verts = parseNums(v->value);
            if (const FNode* v = o.find("PolygonVertexIndex")) { for (double d : parseNums(v->value)) m.pvi.push_back(static_cast<int>(d)); }
            m.geom = &o;
            modelByName[m.name] = static_cast<int>(models.size());
            models.push_back(std::move(m));
        } else if (o.name == "Material" && !tk.empty()) {
            MatInfo mi;
            mi.name = stripPrefix(tk[0]);
            if (const FNode* props = o.find("Properties60")) {
                auto col = [&](const char* key, float* d, int n) {
                    for (const FNode& p : props->kids) {
                        const auto pt = splitTokens(p.value);
                        if (p.name == "Property" && pt.size() >= 3 + static_cast<size_t>(n) && pt[0] == key)
                            for (int i = 0; i < n; ++i) d[i] = std::strtof(pt[3 + i].c_str(), nullptr);
                    }
                };
                // A colour the material does not list is black (D3DMATERIAL9 zero), not the previous colour.
                auto col3 = [&](const char* key, int base) { float c3[3] = {0.f, 0.f, 0.f}; col(key, c3, 3); mi.v[base] = c3[0]; mi.v[base + 1] = c3[1]; mi.v[base + 2] = c3[2]; mi.v[base + 3] = 1; };
                col3("DiffuseColor", 0);
                col3("AmbientColor", 4);
                col3("SpecularColor", 8);
                col3("EmissiveColor", 12);
                col("Shininess", &mi.v[16], 1);
            }
            matByName[mi.name] = static_cast<int>(mats.size());
            mats.push_back(mi);
        } else if (o.name == "Texture" && !tk.empty()) {
            std::string file;
            if (const FNode* f = o.find("FileName")) file = splitTokens(f->value)[0];
            const size_t sl = file.find_last_of("\\/");
            if (sl != std::string::npos) file = file.substr(sl + 1);
            texByName[stripPrefix(tk[0])] = static_cast<int>(texFile.size());
            texFile.push_back(file);
        }
    }

}

void readConnections(const FNode* conns, Scene& sc) {
    auto& models = sc.models;
    auto& modelByName = sc.modelByName;
    auto& matByName = sc.matByName;
    auto& texByName = sc.texByName;
    // connections
    for (const FNode& c : conns->kids) {
        if (c.name != "Connect") continue;
        const auto tk = splitTokens(c.value);
        if (tk.size() < 3) continue;
        const std::string a = tk[1], b = tk[2];
        if (a.rfind("Model::", 0) == 0) {
            const auto ia = modelByName.find(stripPrefix(a));
            if (ia == modelByName.end()) continue;
            if (b == "Model::Scene") models[ia->second].parent = -1;
            else if (auto ib = modelByName.find(stripPrefix(b)); ib != modelByName.end()) models[ia->second].parent = ib->second;
        } else if (a.rfind("Material::", 0) == 0) {
            const auto ib = modelByName.find(stripPrefix(b));
            const auto im = matByName.find(stripPrefix(a));
            if (ib != modelByName.end() && im != matByName.end()) models[ib->second].materials.push_back(im->second);
        } else if (a.rfind("Texture::", 0) == 0) {
            const auto ib = modelByName.find(stripPrefix(b));
            const auto it = texByName.find(stripPrefix(a));
            if (ib != modelByName.end() && it != texByName.end()) models[ib->second].textures.push_back(it->second);
        }
    }
}

void readTakes(const FNode& root, Scene& sc) {
    auto& models = sc.models;
    auto& modelByName = sc.modelByName;
    // takes
    if (const FNode* takes = root.find("Takes")) {
        if (const FNode* take = takes->find("Take")) {
            for (const FNode& mn : take->kids) {
                if (mn.name != "Model") continue;
                const auto it = modelByName.find(stripPrefix(splitTokens(mn.value)[0]));
                if (it == modelByName.end()) continue;
                FbxModel& m = models[it->second];
                for (const FNode& tr : mn.kids) {
                    if (tr.name != "Channel" || splitTokens(tr.value)[0] != "Transform") continue;
                    for (const FNode& g : tr.kids) {
                        if (g.name != "Channel") continue;
                        const std::string gn = splitTokens(g.value)[0];
                        const int gi = gn == "T" ? 0 : gn == "R" ? 1 : gn == "S" ? 2 : -1;
                        if (gi < 0) continue;
                        for (const FNode& a : g.kids) {
                            if (a.name != "Channel") continue;
                            const std::string an = splitTokens(a.value)[0];
                            const int ai = an == "X" ? 0 : an == "Y" ? 1 : an == "Z" ? 2 : -1;
                            if (ai < 0) continue;
                            Channel& ch = m.ch[gi][ai];
                            if (const FNode* d = a.find("Default")) ch.def = std::strtod(d->value.c_str(), nullptr);
                            if (const FNode* k = a.find("Key")) parseKeys(k->value, ch);
                        }
                    }
                }
            }
        }
    }

}

// Triangulates one mesh model into `n` (one vertex per polygon corner, fan triangles, one submesh per material slot).
bool buildMesh(const Scene& sc, const FbxModel& m, Node& n, Model& out, const std::function<int(int)>& slotOf, std::string& err) {
    const auto& mats = sc.mats;
    // geometry
    const Layer nrm = loadLayer(*m.geom, "LayerElementNormal", "Normals", nullptr);
    const Layer uv = loadLayer(*m.geom, "LayerElementUV", "UV", "UVIndex");
    const Layer col = loadLayer(*m.geom, "LayerElementColor", "Colors", "ColorIndex");
    const Layer mat = loadLayer(*m.geom, "LayerElementMaterial", "Materials", nullptr);
    const Layer tex = loadLayer(*m.geom, "LayerElementTexture", "TextureId", nullptr);
    const long ncp = static_cast<long>(m.verts.size() / 3);
    struct Sub { std::vector<uint32_t> idx; int material = 0, texture = -1; };
    std::map<int, Sub> subs;   // by material slot, ascending
    long polyStart = 0, poly = 0;
    for (size_t i = 0; i < m.pvi.size(); ++i) {
        if (m.pvi[i] >= 0) continue;
        const long cnt = static_cast<long>(i) - polyStart + 1;
        if (cnt >= 3) {
            const int slot = mat.has() ? static_cast<int>(std::max<long>(0, layerIndexDirect(mat, poly))) : 0;
            Sub& sb = subs[slot];
            sb.material = slot;
            if (sb.texture < 0 && tex.has() && !tex.data.empty()) {
                const long ti = std::min<long>(static_cast<long>(tex.data.size()) - 1, layerIndex(tex, poly, polyStart, 0) < 0 ? 0 : layerIndex(tex, poly, polyStart, 0));
                const int local = static_cast<int>(tex.data[tex.reference == "IndexToDirect" ? ti : std::min<long>(ti, tex.data.size() - 1)]);
                if (local >= 0 && local < static_cast<int>(m.textures.size())) sb.texture = slotOf(m.textures[local]);
            }
            const uint32_t base = static_cast<uint32_t>(n.verts.size());
            float fn[3] = {0, 0, 1};
            {   // geometric face normal as the fallback
                auto cpOf = [&](long k) { const int v = m.pvi[polyStart + k]; return v < 0 ? -v - 1 : v; };
                const long c0 = cpOf(0), c1 = cpOf(1), c2 = cpOf(2);
                if (c0 < ncp && c1 < ncp && c2 < ncp) {
                    double a[3], b[3];
                    for (int k = 0; k < 3; ++k) { a[k] = m.verts[c1 * 3 + k] - m.verts[c0 * 3 + k]; b[k] = m.verts[c2 * 3 + k] - m.verts[c0 * 3 + k]; }
                    double x = a[1] * b[2] - a[2] * b[1], y = a[2] * b[0] - a[0] * b[2], z = a[0] * b[1] - a[1] * b[0];
                    const double l = std::sqrt(x * x + y * y + z * z);
                    if (l > 1e-12) { fn[0] = float(x / l); fn[1] = float(y / l); fn[2] = float(z / l); }
                }
            }
            for (long k = 0; k < cnt; ++k) {
                const long pv = polyStart + k;
                const int v = m.pvi[pv];
                const long cp = v < 0 ? -v - 1 : v;
                Vertex vx{};
                if (cp >= ncp) { err = "polygon index past the vertex list in " + m.name; return false; }
                for (int c = 0; c < 3; ++c) vx.pos[c] = static_cast<float>(m.verts[cp * 3 + c]);
                vx.normal[0] = fn[0]; vx.normal[1] = fn[1]; vx.normal[2] = fn[2];
                if (nrm.has()) {
                    const long e = layerIndex(nrm, poly, pv, cp);
                    if (e >= 0 && (e * 3 + 2) < static_cast<long>(nrm.data.size())) for (int c = 0; c < 3; ++c) vx.normal[c] = static_cast<float>(nrm.data[e * 3 + c]);
                }
                vx.color[0] = vx.color[1] = vx.color[2] = 1.0f;
                vx.alpha = 1.0f;
                if (col.has()) {
                    const long e = layerIndex(col, poly, pv, cp);
                    if (e >= 0 && (e * 4 + 3) < static_cast<long>(col.data.size())) {
                        for (int c = 0; c < 3; ++c) vx.color[c] = static_cast<float>(col.data[e * 4 + c]);
                        vx.alpha = static_cast<float>(col.data[e * 4 + 3]);
                    }
                }
                if (uv.has()) {
                    const long e = layerIndex(uv, poly, pv, cp);
                    if (e >= 0 && (e * 2 + 1) < static_cast<long>(uv.data.size())) { vx.uv[0] = static_cast<float>(uv.data[e * 2]); vx.uv[1] = static_cast<float>(uv.data[e * 2 + 1]); }
                }
                n.verts.push_back(vx);
            }
            for (long k = 1; k + 1 < cnt; ++k) { sb.idx.push_back(base); sb.idx.push_back(base + static_cast<uint32_t>(k)); sb.idx.push_back(base + static_cast<uint32_t>(k + 1)); }
        }
        ++poly;
        polyStart = static_cast<long>(i) + 1;
    }
    for (auto& [slot, sb] : subs) {
        SubMesh sm;
        Material mt;
        const int mobj = slot >= 0 && slot < static_cast<int>(m.materials.size()) ? m.materials[slot] : (m.materials.empty() ? -1 : m.materials[0]);
        if (mobj >= 0) { mt.name = mats[mobj].name; std::memcpy(mt.v, mats[mobj].v, sizeof mt.v); }
        mt.texture = sb.texture >= 0 ? sb.texture : (m.textures.empty() ? -1 : slotOf(m.textures[0]));
        sm.material = static_cast<uint32_t>(out.materials.size());
        out.materials.push_back(mt);
        sm.indices = std::move(sb.idx);
        n.subs.push_back(std::move(sm));
    }
    return true;
}

bool buildStage(Scene& sc, Model& out, std::string& err) {
    auto& models = sc.models;
    const auto& texFile = sc.texFile;
    std::vector<int> roots;
    for (size_t i = 0; i < models.size(); ++i) {
        if (models[i].parent >= 0) models[models[i].parent].children.push_back(static_cast<int>(i));
        else roots.push_back(static_cast<int>(i));
    }

    // ---- build the Model: node 0 = scene root, then a depth-first walk in declaration order
    out.nodes.emplace_back();
    out.nodes[0].type = 0;
    std::vector<int> order;   // model index per stage node (index = node - 1)
    std::vector<int> parentNode;
    {
        std::vector<std::pair<int, int>> st;   // (model, parent node)
        for (auto it = roots.rbegin(); it != roots.rend(); ++it) st.emplace_back(*it, 0);
        while (!st.empty()) {
            const auto [mi, pn] = st.back();
            st.pop_back();
            const int node = static_cast<int>(out.nodes.size());
            out.nodes.emplace_back();
            order.push_back(mi);
            parentNode.push_back(pn);
            out.nodes[node].parent = pn;
            Node& n = out.nodes[node];
            n.type = models[mi].type == "Mesh" ? 1 : 0;
            for (auto it = models[mi].children.rbegin(); it != models[mi].children.rend(); ++it) st.emplace_back(*it, node);
        }
    }
    for (size_t i = 0; i < out.nodes.size(); ++i) out.nodes[i].firstChild = out.nodes[i].nextSibling = -1;
    {
        std::vector<int> lastChild(out.nodes.size(), -1);
        for (size_t i = 1; i < out.nodes.size(); ++i) {
            const int p = out.nodes[i].parent;
            if (lastChild[p] < 0) out.nodes[p].firstChild = static_cast<int>(i);
            else out.nodes[lastChild[p]].nextSibling = static_cast<int>(i);
            lastChild[p] = static_cast<int>(i);
        }
    }

    // textures (dedup by file in first-use order) and the per-submesh material list
    std::map<std::string, int> texSlot;
    auto slotOf = [&](int texObj) {
        if (texObj < 0 || texObj >= static_cast<int>(texFile.size())) return -1;
        auto [it, fresh] = texSlot.emplace(texFile[texObj], static_cast<int>(out.textures.size()));
        if (fresh) out.textures.push_back(texFile[texObj]);
        return it->second;
    };

    out.tracks.assign(out.nodes.size(), {});
    out.tracks[0] = {MatIdentity()};
    for (size_t ni = 1; ni < out.nodes.size(); ++ni) {
        FbxModel& m = models[order[ni - 1]];
        Node& n = out.nodes[ni];
        // track
        double t0, t1;
        bool any;
        const double span = frameSeconds(m, t0, t1, any);
        auto& tr = out.tracks[ni];
        if (!any) {
            tr = {localMatrix(m, 0, false)};
        } else {
            const int frames = std::max(1, static_cast<int>(std::lround(span * kFps)));
            tr.resize(frames);
            for (int f = 0; f < frames; ++f) tr[f] = localMatrix(m, t0 + (f / kFps) * kKTimePerSecond, true);
            if (frames == 1) tr[0] = localMatrix(m, t0, true);
        }
        if (n.type != 1) continue;
        n.additive = m.adding != 0.0;
        if (!buildMesh(sc, m, n, out, slotOf, err)) return false;
    }

    // static world matrix at the first frame, as the fbxex converter records it
    std::vector<Mat4> world;
    EvaluateWorld(out, 0, world);
    for (size_t i = 0; i < out.nodes.size(); ++i) out.nodes[i].world = world[i];
    return true;
}

}  // namespace

bool ParseFbxAscii(const std::string& text, Model& out, std::string& err) {
    out = Model{};
    if (text.compare(0, 5, "; FBX") != 0 || text.find("FBXVersion: 61") == std::string::npos) { err = "not an ASCII FBX 6.1 file"; return false; }
    FNode root;
    if (!parseTree(text, root, err)) return false;
    const FNode* objects = root.find("Objects");
    const FNode* conns = root.find("Connections");
    if (!objects || !conns) { err = "no Objects / Connections"; return false; }
    Scene sc;
    readObjects(objects, sc);
    readConnections(conns, sc);
    readTakes(root, sc);
    return buildStage(sc, out, err);
}

}  // namespace pc::stage3d

#include <filesystem>
#include <fstream>
#include <sstream>

namespace s3d {

bool LoadFbxAsciiFile(const std::string& utf8Path, Model& out, std::string& err)
{
	std::ifstream f(std::filesystem::u8path(utf8Path), std::ios::binary);
	if (!f) { err = "cannot read " + utf8Path; return false; }
	std::ostringstream ss;
	ss << f.rdbuf();
	const std::string text = ss.str();
	if (text.compare(0, 6, "; FBX ") != 0 && text.find("FBXHeaderExtension") == std::string::npos) { err = "not an ASCII FBX file: " + utf8Path; return false; }
	return ParseFbxAscii(text, out, err);
}

bool LoadStageModel(const std::string& utf8PathOrDir, Model& out, std::string& usedPath, std::string& err)
{
	namespace fs = std::filesystem;
	fs::path p = fs::u8path(utf8PathOrDir);
	std::error_code ec;
	fs::path dir = fs::is_directory(p, ec) ? p : p.parent_path();
	const fs::path bin = dir / "bg.fbx.bin", txt = dir / "bg.fbx";
	if (!fs::is_directory(p, ec) && p.extension() == ".fbx") {
		usedPath = p.u8string();
		return LoadFbxAsciiFile(usedPath, out, err);
	}
	if (fs::exists(bin, ec)) { usedPath = bin.u8string(); return LoadFbxExFile(usedPath, out, err); }
	if (fs::exists(txt, ec)) { usedPath = txt.u8string(); return LoadFbxAsciiFile(usedPath, out, err); }
	err = "no bg.fbx.bin or bg.fbx in " + dir.u8string();
	return false;
}

}  // namespace s3d
