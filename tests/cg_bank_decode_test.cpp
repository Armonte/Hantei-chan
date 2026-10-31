// cg_bank_decode_test: every image a BMP Cutter bank decodes to has a size a texture upload accepts (issue #87).
// BMP Cutter3 stores an empty image (a blank frame slot) with inverted bounds: x1,y1 = the canvas size and x2,y2 = 32, no blocks
// (BOF's shermie.cg / kyo_lo.cg / iori.cg / angel.cg have hundreds). The CG decoder only rejected a zero side, so such an image
// decoded as -223 x -223; the CG manager uploaded that to glTexImage2D, which failed with GL_INVALID_VALUE (0x501): a black
// thumbnail and the "GL Error: 0x501" box on every scroll.
// Usage: cg_bank_decode_test [folder]  - with a folder, also decodes every image of every .cg in it (both decoders).
#include "cg.h"
#include "cgm/cgm_bank.h"
#include "cgm/cgm_io.h"
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static const int kMaxSide = 16384;   // the GL_MAX_TEXTURE_SIZE of current desktop GPUs; any real sheet is far below it

static cgm::Image MakeImage(const char *name, int x1, int y1, int x2, int y2) {
	cgm::Image im; im.present = true; im.uid = cgm::NewImageUid();
	std::snprintf(im.name, sizeof(im.name), "%s", name);
	im.type = 0; im.bpp = 8; im.w = 256; im.h = 256; im.x1 = x1; im.y1 = y1; im.x2 = x2; im.y2 = y2;
	return im;
}

// A bank with the blank-slot encoding BMP Cutter3 writes (image 0) next to a normal 16 x 16 image (image 1).
static std::vector<uint8_t> BlankSlotBank() {
	cgm::Bank b;
	memcpy(b.head.data(), "BMP Cutter3", 11);
	uint32_t pal[256] = {}; pal[1] = 0xFF0000FFu;   // index 1: opaque red
	b.setPaletteSlot(0, pal);
	b.H[0] = 0; b.H[4] = 16;
	b.images.push_back(MakeImage("0.png", 256, 256, 32, 32));   // as stored in shermie.cg images 0-29, 40-44, 57-64, ...
	cgm::Image im = MakeImage("1.png", 0, 0, 15, 15);
	cgm::Block blk; blk.w = 16; blk.h = 16;
	im.blocks.push_back(blk);
	im.blob = std::make_shared<const std::vector<uint8_t>>(256, (uint8_t)1);
	b.images.push_back(im);
	b.recomputeLayout();
	std::vector<uint8_t> bytes; b.serialize(bytes);
	return bytes;
}

static void CheckDecoded(const std::string &where, int n, int w, int h, size_t bytes) {
	const bool ok = w > 0 && h > 0 && w <= kMaxSide && h <= kMaxSide && bytes >= (size_t)w * h * 4;
	if (!ok) { std::printf("FAIL %s image %d decoded as %d x %d (%zu bytes)\n", where.c_str(), n, w, h, bytes); fails++; }
}

// Both decoders over every image of a bank: an image either fails to decode (shown empty) or decodes to an uploadable size.
static void ScanBank(const std::string &where, const std::vector<uint8_t> &bytes, int *decoded = nullptr, int *empty = nullptr) {
	CG cg;
	CHECK(cg.loadFromMemory(bytes.data(), (unsigned)bytes.size()));
	const int count = cg.get_image_count();
	for (int n = 0; n < count; n++) {
		cgm::Rgba r;
		if (cgm::DecodePreview(cg, nullptr, n, 0, 0, r)) { CheckDecoded(where + " (CG)", n, r.w, r.h, r.px.size()); if (decoded) (*decoded)++; }
		else if (empty) (*empty)++;
	}
	cgm::Bank b; std::string err;
	if (!cgm::Bank::parse(bytes.data(), bytes.size(), b, &err)) { std::printf("note: %s: bank model: %s\n", where.c_str(), err.c_str()); return; }
	for (int n = 0; n < (int)b.images.size(); n++) {
		cgm::Rgba r;
		if (cgm::DecodePreview(cg, &b, n, 0, 0, r)) CheckDecoded(where + " (model)", n, r.w, r.h, r.px.size());
	}
}

int main(int argc, char **argv) {
	// the reported shape, both decoders
	const std::vector<uint8_t> bytes = BlankSlotBank();
	{
		CG cg;
		CHECK(cg.loadFromMemory(bytes.data(), (unsigned)bytes.size()));
		CHECK(cg.get_image_count() == 2);
		int bpp, ty, x1, y1, x2, y2;
		CHECK(cg.image_info(0, bpp, ty, x1, y1, x2, y2) && x1 == 256 && x2 == 32);
		ImageData *blank = cg.draw_texture(0, false, false);
		CHECK(blank == nullptr);   // was a -223 x -223 image
		if (blank) { std::printf("  blank slot decoded as %d x %d\n", blank->width, blank->height); delete blank; }
		cgm::Rgba r;
		CHECK(!cgm::DecodePreview(cg, nullptr, 0, 0, 0, r));
		CHECK(cgm::DecodePreview(cg, nullptr, 1, 0, 0, r));
		CHECK(r.w == 16 && r.h == 16 && r.px.size() == 16u * 16 * 4);
		CHECK(r.px.size() >= 4 && r.px[0] == 0xFF && r.px[3] == 0xFF);
		cgm::CgIO io(cg); cgm::ImgInfo info;
		CHECK(io.info(0, info) && info.present && !info.drawable);
		CHECK(!io.decode(0, r, nullptr));
	}
	{
		cgm::Bank b; std::string err;
		CHECK(cgm::Bank::parse(bytes.data(), bytes.size(), b, &err));
		CHECK(b.images.size() == 2 && !b.images[0].drawable() && b.images[1].drawable());
		cgm::Rgba r;
		CHECK(!b.decode(0, r));
		CHECK(b.decode(1, r) && r.w == 16 && r.h == 16);
	}
	ScanBank("blank-slot bank", bytes);

	// optional: every .cg in a folder (e.g. a custom character pack)
	if (argc > 1) {
		std::error_code ec; int banks = 0, decoded = 0, empty = 0;
		for (const auto &e : std::filesystem::directory_iterator(std::filesystem::u8path(argv[1]), ec)) {
			std::string ext = e.path().extension().u8string();
			for (char &c : ext) c = (char)tolower((unsigned char)c);
			if (ext != ".cg") continue;
			std::FILE *f = _wfopen(e.path().wstring().c_str(), L"rb");
			if (!f) continue;
			std::vector<uint8_t> data; uint8_t buf[65536]; size_t got;
			while ((got = std::fread(buf, 1, sizeof(buf), f)) > 0) data.insert(data.end(), buf, buf + got);
			std::fclose(f);
			ScanBank(e.path().filename().u8string(), data, &decoded, &empty);
			banks++;
		}
		std::printf("%d banks, %d images decoded, %d empty / undrawable\n", banks, decoded, empty);
		CHECK(banks > 0);
	}
	if (fails) { std::printf("%d failure(s)\n", fails); return 1; }
	std::printf("cg_bank_decode_test: ok\n");
	return 0;
}
