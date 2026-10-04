#ifndef ARCHIVE_BROWSER_STATE_H_GUARD
#define ARCHIVE_BROWSER_STATE_H_GUARD

// Internal state shared by archive_browser.cpp (model, worker thread, decoders) and archive_browser_ui.cpp (ImGui). Not for other modules.
#include "archive_browser.h"
#include "han2/pac_archive.h"
#include "han2/gof1_archive.h"
#include "fbarc/fb_archive.h"
#include "han2_character.h"

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace cgm { struct Bank; }

namespace abrowser {

enum class Type { Character, CharData, Image, CgBank, Parts, Script, Audio, Text, Palette, Model, Archive, Other };
const char *TypeName(Type t);
Type TypeOfName(const std::string &utf8Name, Game ctx, bool inArchive);

struct Source;
using SourceP = std::shared_ptr<Source>;

struct Item {
	std::string name;        // display name (UTF-8, path relative to the source)
	std::string key;         // what the readers take: raw CP932 name (PAC / GOF1), relative path (fbarc), UTF-8 absolute path (folder)
	uint64_t size = 0;
	Type type = Type::Other;
	uint32_t index = 0;      // entry index inside the owning archive
	uint16_t member = 0;     // merged source: index into Source::members of the archive that wins this name
};

struct Source {
	enum Kind { Folder, Pac, Gof1, Fb, Merged } kind = Folder;
	enum State { Loading, Ready, Failed };
	std::string path, label, describe, error;
	Game game = Game::None;
	int group = 0;
	std::atomic<int> state{Loading};
	std::atomic<bool> closed{false};
	std::vector<Item> items;                      // immutable once state == Ready (rebuilt by swapping a new Source content on reload: see Reload)
	std::vector<SourceP> members;                 // Merged: the archives that contribute, in priority order (later wins)
	std::shared_ptr<pac::Archive> pac;
	std::shared_ptr<gof1::Archive> g1;
	std::shared_ptr<fbarc::Archive> fb;
	bool IsArchive() const { return kind == Pac || kind == Gof1 || kind == Fb; }
};

struct Group {
	int id = 0;
	GameInfo info;
	std::vector<SourceP> archives;                // priority order
	SourceP merged;
	SourceP folder;
};

// A decoded preview (built on the worker, uploaded and drawn on the UI thread).
struct Preview {
	enum Kind { None, Loading, Image, Bank, Text, Hex, Message } kind = None;
	uint64_t serial = 0;
	std::string title;
	std::vector<std::pair<std::string, std::string>> facts;
	std::string text;                              // Text: UTF-8 body; Message: the explanation
	std::vector<uint8_t> bytes;                    // Hex: the first bytes
	uint64_t totalSize = 0;
	int w = 0, h = 0;
	std::vector<uint8_t> rgba;                     // Image
	std::vector<uint32_t> swatches;                // Image: palette of the source image (RGBA words), informational
	std::shared_ptr<cgm::Bank> bank;               // Bank
	bool isCharacter = false;                      // Image is a character thumbnail
	bool canOpen = false;
	std::string stage;                             // what the worker is doing (Loading)
};

struct Thumb { unsigned tex = 0; int w = 0, h = 0; int state = 0; /*0 none 1 pending 2 ready 3 failed*/ uint64_t used = 0; };

struct ViewState { float zoom = 1.f; float panX = 0, panY = 0; bool fit = true; bool checker = true; };

struct Progress { std::atomic<int> done{0}, total{0}, failed{0}; std::atomic<bool> running{false}, cancel{false}; std::string what, dest; };

struct State {
	std::vector<SourceP> sources;                  // the left list
	std::vector<std::shared_ptr<Group>> groups;
	int selSource = -1;
	// listing
	char filter[96] = "";
	int typeFilter = -1;                           // -1 all, else (int)Type
	int sortCol = 0; bool sortDesc = false;
	bool thumbs = true;
	std::vector<uint32_t> view;                    // item indices (into the selected source's items) after filter + sort
	bool viewDirty = true;
	std::set<uint32_t> sel;                        // selected item indices
	int anchor = -1, cursor = -1;                  // positions in `view`
	std::string typeBuf; double typeAt = 0;
	bool scrollToCursor = false;
	// preview
	Preview prev;
	uint64_t prevSerial = 0;
	int prevSource = -1; int prevItem = -1;       // what `prev` shows
	unsigned prevTex = 0; int prevTexW = 0, prevTexH = 0;
	int bankFrame = -1, bankPalette = 0; int bankTexFrame = -2, bankTexPalette = -1;
	ViewState view2;
	std::string status;
	Progress prog;
	// thumbnails
	std::unordered_map<std::string, Thumb> thumbCache;
	uint64_t useClock = 0;
	int thumbsPending = 0;
	// settings
	std::string lastBrowsed;
	std::vector<std::string> recent;
	bool restored = false;
	int nextGroup = 1;
};

State &S();
void PostJob(std::function<void()> f, bool lowPriority = false);
void PostMain(std::function<void()> f);
void PumpMain();                                   // run results on the UI thread
bool WorkerBusy();

// model
SourceP FindSource(int idx);
std::string ItemPathText(const Source &s, const Item &it);
bool ReadItem(const Source &s, const Item &it, std::vector<uint8_t> &out, std::string *err, size_t maxBytes = SIZE_MAX);
const Source &Owner(const Source &s, const Item &it, uint32_t &ownerIndex);   // merged -> the archive that holds the entry
void MountArchive(const std::string &utf8Path, int group, bool select);
void ReloadArchive(const std::string &utf8Path);
void CloseSource(int idx);
void RebuildMerged(Group &g);
void SaveSettings();
void LoadSettings();
void RememberBrowsed(const std::string &path);

// preview + thumbs
void RequestPreview(int sourceIdx, int itemIdx);
void RequestThumb(int sourceIdx, int itemIdx);
Thumb *ThumbFor(int sourceIdx, int itemIdx, bool request);
std::string ThumbKey(const Source &s, const Item &it);

// UI-thread appliers (archive_browser_ui.cpp): create the GL textures
void ApplyPreview(std::shared_ptr<Preview> pv);
void ApplyThumb(const std::string &key, std::shared_ptr<std::vector<uint8_t>> rgba, int w, int h);

// opening / extraction
bool BuildOpenRequest(int sourceIdx, int itemIdx, OpenRequest &req, std::string &msg);
void StartExtract(int sourceIdx, std::vector<uint32_t> items, const std::string &dir);   // empty items = everything in the source

} // namespace abrowser

#endif
