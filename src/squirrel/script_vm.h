#ifndef SQUIRREL_SCRIPT_VM_H_GUARD
#define SQUIRREL_SCRIPT_VM_H_GUARD

// In-editor Squirrel 2.2.4 VM (the games' exact VM, third_party/squirrel-2.2.4 with SQ_FB_COMPAT and
// FB's Shift-JIS lexer), sandboxed: the game registers io and system, but here every native that
// could touch files or spawn processes (file, loadfile, writeclosuretofile, system, remove,
// rename, getenv) is a stub that only reports what the script would have done. __dofile__, dofile and
// SQ_LoadString read (never write) below the game root, like the game's archive search path.
//
// Natives come from the game's API profile (docs/squirrel/api/<game>_api.json, generated from the
// IDB): stubs that return a default for their inferred return type.

#include "chara_load_types.h"

#include <functional>
#include <memory>
#include <string>

struct SQVM;

namespace sqedit {

struct Diagnostic {
	bool ok = true;
	bool runtime = false; // false: compile error, true: error while running the top level
	int line = 0;         // 1-based, 0 = unknown
	int column = 0;
	std::string message;  // VM text (CP932 bytes from the script stay as-is)
	std::string source;
};

using LogFn = std::function<void(const std::string&)>;

class ScriptVM {
public:
	ScriptVM();
	~ScriptVM();
	ScriptVM(const ScriptVM&) = delete;
	ScriptVM& operator=(const ScriptVM&) = delete;

	// gameRoot: folder that holds script/ and data/. apiJsonPath: may be empty (compile-only).
	// Returns false (and fills *error) when the API profile cannot be parsed.
	bool open(const std::string& gameRoot, const std::string& apiJsonPath, const LogFn& log, std::string* error);
	void close();
	bool isOpen() const;

	// Runs the battle VM setup once (MBTL Sq_InitBattleVM: script/Battle_Init.txt, script/Battle_Std.txt, data/_combase.txt), so a file's
	// top level sees the shared move library. Returns the first failure, or ok.
	Diagnostic bootstrap();

	// Compiles the raw CP932 bytes exactly as the game does (sq_compilebuffer, no transcoding).
	Diagnostic compile(const std::string& bytes, const std::string& name);
	// Compiles, then runs the top level against the stubs (after bootstrap()).
	Diagnostic compileAndRun(const std::string& bytes, const std::string& name);

	// The game's character load (chara_load.h) in a fresh VM: the battle VM setup, then chrDirRel's sc/cmd/mv/com in game
	// order with load-time natives answering from ctx; overrideRel's bytes (the unsaved editor text) replace that file.
	CharaLoadResult checkCharacter(const std::string& chrDirRel, LoadContext ctx, const std::string& overrideRel,
		const std::string& overrideBytes);

	int nativeCount() const;
	int sandboxBlockedCount() const;

	struct Impl; // opaque; public so the VM callbacks in script_vm.cpp can reach it through the foreign pointer

private:
	std::unique_ptr<Impl> m;
};

} // namespace sqedit

#endif
