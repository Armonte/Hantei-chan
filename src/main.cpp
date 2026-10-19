#include "main.h"
#include "startup_args.h"
#include "platform/platform.h"
#include "main_frame.h"
#include "test.h"
#include "ini.h"
#include "version.h"
#include "platform/viewports.h"

#include <iostream>
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <cwctype>
#include <imgui.h>
#include "imsearch.h"

#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>
#include <SDL3/SDL.h>
// SDL provides the Windows entry point (wWinMain under UNICODE) and renames our
// main to SDL_main; including this is what wires the two together.
#include <SDL3/SDL_main.h>
#include <windows.h>
#include <shellapi.h>
#include "png_writer.h"
#include "app_log.h"

#include <glad/glad.h>

#include <res/resource.h>

#ifndef HA6GUIVERSION
#define HA6GUIVERSION " custom"
#endif

char iniLocation[512] {};

ImVec2 clientRect;
bool gAppQuit = false;

HWND mainWindowHandle;
bool init = false;

// Static storage for glyph ranges (must persist until font atlas is built)
static ImVector<ImWchar> g_fontGlyphRanges;

void LoadJapaneseFonts(ImGuiIO& io)
{
	char winFolder[512]{};
	ImFontConfig config;
/* 	config.PixelSnapH = 1;
	config.OversampleH = 1;
	config.OversampleV = 1; */
	
	// Build custom glyph ranges: Japanese + special symbols (★☆●◆■ etc.)
	// These symbols are used in pattern names but aren't in GetGlyphRangesJapanese()
	// Store in static vector so it persists until Build() is called
	if(g_fontGlyphRanges.empty()) {
		ImFontGlyphRangesBuilder builder;
		builder.AddRanges(io.Fonts->GetGlyphRangesJapanese()); // Add Japanese characters
		
		// Add special symbol ranges manually
		static const ImWchar symbol_ranges[] = {
			0x2000, 0x206F, // General Punctuation (includes ※ etc.)
			0x2190, 0x21FF, // Arrows (includes ←↑→↓ etc.)
			0x25A0, 0x25FF, // Geometric Shapes (includes ■●◆▲▼ etc.)
			0x2600, 0x26FF, // Miscellaneous Symbols (includes ★☆☀☁ etc.)
			0x3000, 0x303F, // CJK Symbols and Punctuation (includes full-width space, 、, 〜, etc.)
			0xFF00, 0xFFEF, // Halfwidth and Fullwidth Forms (includes ？, ＠, full-width letters/numbers)
			0,
		};
		builder.AddRanges(symbol_ranges); // Add special symbols
		
		// Explicitly add common symbols used in pattern names
		// These are the most frequently used symbols in .ha6 files
		builder.AddChar(0x203B); // ※ (REFERENCE MARK)
		builder.AddChar(0x2190); // ← (LEFTWARDS ARROW)
		builder.AddChar(0x2191); // ↑ (UPWARDS ARROW)
		builder.AddChar(0x2192); // → (RIGHTWARDS ARROW)
		builder.AddChar(0x2193); // ↓ (DOWNWARDS ARROW)
		builder.AddChar(0x2605); // ★ (BLACK STAR)
		builder.AddChar(0x2606); // ☆ (WHITE STAR)
		builder.AddChar(0x25CF); // ● (BLACK CIRCLE)
		builder.AddChar(0x25CB); // ○ (WHITE CIRCLE)
		builder.AddChar(0x25A0); // ■ (BLACK SQUARE) - explicitly add this one
		builder.AddChar(0x25A1); // □ (WHITE SQUARE)
		builder.AddChar(0x25C6); // ◆ (BLACK DIAMOND)
		builder.AddChar(0x25C7); // ◇ (WHITE DIAMOND)
		builder.AddChar(0x25B2); // ▲ (BLACK UP-POINTING TRIANGLE)
		builder.AddChar(0x25B3); // △ (WHITE UP-POINTING TRIANGLE)
		builder.AddChar(0x25BC); // ▼ (BLACK DOWN-POINTING TRIANGLE)
		builder.AddChar(0x25BD); // ▽ (WHITE DOWN-POINTING TRIANGLE)
		builder.AddChar(0x3000); // 　 (IDEOGRAPHIC SPACE - full-width space)
		
		// Explicitly add full-width characters that are commonly used
		builder.AddChar(0xFF00); // 　 (FULLWIDTH SPACE)
		builder.AddChar(0xFF01); // ！ (FULLWIDTH EXCLAMATION MARK)
		builder.AddChar(0xFF08); // （ (FULLWIDTH LEFT PARENTHESIS)
		builder.AddChar(0xFF09); // ） (FULLWIDTH RIGHT PARENTHESIS)
		builder.AddChar(0xFF0B); // ＋ (FULLWIDTH PLUS SIGN)
		builder.AddChar(0xFF0D); // － (FULLWIDTH HYPHEN-MINUS)
		builder.AddChar(0xFF10); // ０ (FULLWIDTH DIGIT ZERO)
		builder.AddChar(0xFF11); // １ (FULLWIDTH DIGIT ONE)
		builder.AddChar(0xFF12); // ２ (FULLWIDTH DIGIT TWO)
		builder.AddChar(0xFF13); // ３ (FULLWIDTH DIGIT THREE)
		builder.AddChar(0xFF14); // ４ (FULLWIDTH DIGIT FOUR)
		builder.AddChar(0xFF15); // ５ (FULLWIDTH DIGIT FIVE)
		builder.AddChar(0xFF16); // ６ (FULLWIDTH DIGIT SIX)
		builder.AddChar(0xFF17); // ７ (FULLWIDTH DIGIT SEVEN)
		builder.AddChar(0xFF18); // ８ (FULLWIDTH DIGIT EIGHT)
		builder.AddChar(0xFF19); // ９ (FULLWIDTH DIGIT NINE)
		builder.AddChar(0xFF1F); // ？ (FULLWIDTH QUESTION MARK)
		builder.AddChar(0xFF20); // ＠ (FULLWIDTH COMMERCIAL AT) - explicitly add this one
		builder.AddChar(0xFF21); // Ａ (FULLWIDTH LATIN CAPITAL LETTER A)
		builder.AddChar(0xFF22); // Ｂ (FULLWIDTH LATIN CAPITAL LETTER B)
		builder.AddChar(0xFF23); // Ｃ (FULLWIDTH LATIN CAPITAL LETTER C)
		builder.AddChar(0xFF24); // Ｄ (FULLWIDTH LATIN CAPITAL LETTER D)
		builder.AddChar(0xFF25); // Ｅ (FULLWIDTH LATIN CAPITAL LETTER E)
		builder.AddChar(0xFF26); // Ｆ (FULLWIDTH LATIN CAPITAL LETTER F)
		builder.AddChar(0xFF2A); // Ｊ (FULLWIDTH LATIN CAPITAL LETTER J)
		builder.AddChar(0xFF33); // Ｓ (FULLWIDTH LATIN CAPITAL LETTER S)
		builder.AddChar(0xFF38); // Ｘ (FULLWIDTH LATIN CAPITAL LETTER X)
		
		builder.BuildRanges(&g_fontGlyphRanges);
	}
	
	// Ensure ranges are valid (should not be empty)
	const ImWchar* rangesToUse = g_fontGlyphRanges.empty() ? io.Fonts->GetGlyphRangesJapanese() : g_fontGlyphRanges.Data;
	
	// Try embedded Noto font first (it definitely has all the symbols we need)
	ImFont* japaneseFont = nullptr;
	HMODULE hModule = GetModuleHandle(nullptr);
	if(hModule) {
		HRSRC res = FindResource(hModule, MAKEINTRESOURCE(NOTO_SANS_JP_F), RT_RCDATA);
		if(res) {
			HGLOBAL hRes = LoadResource(hModule, res);
			if(hRes) {
				void *notoFont = LockResource(hRes);
				if(notoFont) {
					config.FontDataOwnedByAtlas = false;
					japaneseFont = io.Fonts->AddFontFromMemoryTTF(notoFont, SizeofResource(hModule, res), gSettings.fontSize, &config, rangesToUse);
					if(japaneseFont) {
						HC_LOG_INFO("[Font] Noto Sans JP loaded with symbol support");
					}
				}
			}
		}
	}
	
	// If Noto failed, try Meiryo as fallback
	if(!japaneseFont) {
		int appendAt = GetWindowsDirectoryA(winFolder, 512);
		strcpy(winFolder+appendAt, "\\Fonts\\meiryo.ttc");
		japaneseFont = io.Fonts->AddFontFromFileTTF(winFolder, gSettings.fontSize, &config, rangesToUse);
		if(japaneseFont) {
			HC_LOG_INFO("[Font] Meiryo loaded with symbol support (Noto not available)");
			
			// Try to merge Noto for missing glyphs if available
			if(hModule) {
				HRSRC res = FindResource(hModule, MAKEINTRESOURCE(NOTO_SANS_JP_F), RT_RCDATA);
				if(res) {
					HGLOBAL hRes = LoadResource(hModule, res);
					if(hRes) {
						void *notoFont = LockResource(hRes);
						if(notoFont) {
							ImFontConfig mergeConfig;
							mergeConfig.MergeMode = true;
							mergeConfig.FontDataOwnedByAtlas = false;
							io.Fonts->AddFontFromMemoryTTF(notoFont, SizeofResource(hModule, res), gSettings.fontSize, &mergeConfig, rangesToUse);
							HC_LOG_INFO("[Font] Noto merged into Meiryo for missing glyphs");
						}
					}
				}
			}
		}
	}
	
	// Set the Japanese font as the default font if it loaded successfully
	if(japaneseFont) {
		io.FontDefault = japaneseFont;
		HC_LOG_INFO("[Font] Japanese font loaded with symbol support and set as default");
		
		// Debug: Check if specific glyphs are available
		// Note: This check happens before Build(), so we can't verify glyphs yet
		// But we can verify the ranges were set correctly
		HC_LOG_INFO("[Font] Glyph ranges size: {}", g_fontGlyphRanges.Size);
	} else {
		HC_LOG_WARN("[Font] Failed to load Japanese font; Japanese characters may not display correctly");
	}
}


// ---- App-side SDL input (the WndProc switch) --------------------------------

// Pan / draw drags in progress, and the physical button (0 L, 1 R, 2 M) that
// holds each. dragLeft is the pan role, dragRight the draw role; the mouse
// button preference maps physical buttons onto them (issue #9).
bool g_dragLeft = false, g_dragRight = false;
int g_dragButtonLeft = 0, g_dragButtonRight = 1;
// Last mouse position in main-window client coordinates. SDL reports window
// coordinates, so the old ScreenToClient step is gone.
float g_mouseX = 0.0f, g_mouseY = 0.0f;
bool g_justActivated = false;

// WM_LBUTTONDOWN / RBUTTONDOWN / MBUTTONDOWN.
void BeginDrag(MainFrame* mf, int button, float x, float y)
{
	const bool draw = button == gSettings.mouseDrawButton;
	const bool pan = button == gSettings.mousePanButton || (button == 2 && !draw);
	// Do not start a drag when the window just became active: the click that
	// activates a window would otherwise pan the view.
	if ((pan || draw) && !ImGui::GetIO().WantCaptureMouse && !g_justActivated &&
	    !(draw ? g_dragRight : g_dragLeft)) {
		g_mouseX = x;
		g_mouseY = y;
		SDL_CaptureMouse(true);
		if (draw) {
			g_dragRight = true;
			g_dragButtonRight = button;
			if (mf) mf->RightClick((int)x, (int)y);
		} else {
			g_dragLeft = true;
			g_dragButtonLeft = button;
			if (mf) mf->HandleMouseDown(false, true);
			if (mf) mf->LeftClick((int)x, (int)y);
		}
		if (mf) mf->SetMouseDragActive(true);
		return;
	}
	g_justActivated = false;  // clear after the first click
}

// WM_LBUTTONUP / RBUTTONUP / MBUTTONUP.
void EndDrag(MainFrame* mf, int button)
{
	if (g_dragRight && button == g_dragButtonRight) {
		if (!g_dragLeft) SDL_CaptureMouse(false);
		g_dragRight = false;
		if (mf) mf->HandleMouseUp(true, false);
		if (mf) mf->SetMouseDragActive(g_dragLeft);
		return;
	}
	if (g_dragLeft && button == g_dragButtonLeft) {
		if (!g_dragRight) SDL_CaptureMouse(false);
		g_dragLeft = false;
		if (mf) mf->HandleMouseUp(false, true);
		if (mf) mf->SetMouseDragActive(g_dragRight);
	}
}

// WM_ACTIVATE(WA_INACTIVE) / WM_CANCELMODE: close any gesture in progress.
void CancelDrags(MainFrame* mf)
{
	if (!g_dragLeft && !g_dragRight) return;
	if (mf) mf->CancelViewportGestures();
	SDL_CaptureMouse(false);
	g_dragLeft = g_dragRight = false;
	if (mf) mf->SetMouseDragActive(false);
}

void AppSdlEvent(const SDL_Event& e, void* user)
{
	MainFrame* mf = (MainFrame*)user;
	ImGuiIO& io = ImGui::GetIO();
	switch (e.type) {
	case SDL_EVENT_KEY_DOWN: {
		// The shortcut router decides: a focused text field keeps its keys.
		const uint32_t vk = platform::VkFromScancode((int)e.key.scancode);
		if (vk && mf && mf->HandleKeys(vk, e.key.repeat, io.WantCaptureKeyboard, io.WantTextInput))
			return;
		break;
	}
	case SDL_EVENT_TEXT_INPUT:
		// A key that already ran a shortcut from a focused text field (Num* / Num/
		// keyframe step) must not also type its character (the WM_CHAR rule).
		if (MainFrame::s_swallowChar && e.text.text[0] &&
		    (wchar_t)towlower((wint_t)(unsigned char)e.text.text[0]) == MainFrame::s_swallowChar) {
			MainFrame::s_swallowChar = 0;
		}
		break;
	case SDL_EVENT_MOUSE_BUTTON_DOWN: {
		const int button = e.button.button == SDL_BUTTON_LEFT ? 0 :
		                   e.button.button == SDL_BUTTON_RIGHT ? 1 :
		                   e.button.button == SDL_BUTTON_MIDDLE ? 2 : -1;
		if (button >= 0) {
			g_mouseX = e.button.x;
			g_mouseY = e.button.y;
			BeginDrag(mf, button, e.button.x, e.button.y);
		}
		break;
	}
	case SDL_EVENT_MOUSE_BUTTON_UP: {
		const int button = e.button.button == SDL_BUTTON_LEFT ? 0 :
		                   e.button.button == SDL_BUTTON_RIGHT ? 1 :
		                   e.button.button == SDL_BUTTON_MIDDLE ? 2 : -1;
		if (button >= 0) EndDrag(mf, button);
		break;
	}
	case SDL_EVENT_MOUSE_MOTION:
		// Only the active pan/draw drag moves the scene. The main window is the
		// only window a drag can start in, so an event from another window is
		// not ours (a detached window never pans the main view).
		if ((g_dragLeft || g_dragRight) && mf) {
			mf->HandleMouseDrag((int)(e.motion.x - g_mouseX), (int)(e.motion.y - g_mouseY),
			                    g_dragRight, g_dragLeft);
		}
		g_mouseX = e.motion.x;
		g_mouseY = e.motion.y;
		break;
	case SDL_EVENT_MOUSE_WHEEL:
		// WM_MOUSEWHEEL: only when ImGui does not capture the mouse and nothing is
		// being dragged. The zoom point is the cursor, and SDL already gives it in
		// window coordinates.
		if (mf && !io.WantCaptureMouse && !(g_dragLeft || g_dragRight)) {
			float wx = 0, wy = 0;
			SDL_GetMouseState(&wx, &wy);
			mf->HandleMouseWheel((e.wheel.y > 0) != gSettings.invertWheelZoom, (int)wx, (int)wy);
		}
		break;
	case SDL_EVENT_WINDOW_FOCUS_GAINED:
	case SDL_EVENT_WINDOW_MOUSE_ENTER:
		g_justActivated = true;
		break;
	case SDL_EVENT_WINDOW_FOCUS_LOST:
		// SDL_EVENT_KEY_UP is irrelevant here; a lost focus closes the gesture,
		// like WM_ACTIVATE(WA_INACTIVE).
		CancelDrags(mf);
		break;
	default:
		break;
	}
}

int main(int argC, char** argV)
{
	// Allocate console for debug output with Shift-JIS support (DISABLED FOR RELEASE)
	//AllocConsole();
	//FILE* dummy;
	//freopen_s(&dummy, "CONOUT$", "w", stdout);
	//freopen_s(&dummy, "CONOUT$", "w", stderr);
	//SetConsoleOutputCP(65001);  // UTF-8 code page
	//SetConsoleCP(65001);  // UTF-8 code page for input too
	//printf("=== Hantei-chan Debug Console (UTF-8) ===\n\n");
	
	bool useIni = true;
	// SDL3 hands over UTF-8 argv (the Windows backend converts the command line
	// with CP_UTF8; libc's argv is UTF-8 on Linux), so no wide conversion and no
	// CommandLineToArgvW/LocalFree anymore.
	for(int i=0; i<argC; i++)
	{
		const char * arg = argV[i];
		if(!strcmp(arg, "--test"))
		{
			std::ofstream coutFile, cerrFile;
			coutFile.open("cout.txt");
			auto cout_buf = std::cout.rdbuf(coutFile.rdbuf());
			TestHa6();
			return 0;
		}
		else if(i+1<argC && !strcmp(arg, "--fxr-selftest"))
		{
			// Effect recolour tab self-test: programmatic load/save/assign/new-rule + report, see fxr_panel.cpp. Pair with --capture <png>.
			const std::string v = argV[i+1];
			_putenv_s("HANTEI_FXR_SELFTEST", v.c_str()); _putenv_s("HANTEI_CGM_TAB", "fxr");
			gStartup.tool = "cgm";
			i++;
		}
		else if(i+1<argC && (!strcmp(arg, "--open") || !strcmp(arg, "--open-any") || !strcmp(arg, "--capture") || !strcmp(arg, "--cg") || !strcmp(arg, "--pattern")
		                     || !strcmp(arg, "--frame") || !strcmp(arg, "--palette") || !strcmp(arg, "--game-link")
		                     || !strcmp(arg, "--zoom") || !strcmp(arg, "--game")
		                     || !strcmp(arg, "--compare") || !strcmp(arg, "--tool") || !strcmp(arg, "--tag-ini")
		                     || !strcmp(arg, "--tag-char") || !strcmp(arg, "--tag-tab")
		                     || !strncmp(arg, "--authoring-", 12)) && strcmp(arg, "--authoring-link") && strcmp(arg, "--authoring-load")
		                     && strcmp(arg, "--authoring-unlock"))
		{
			// startup actions, see startup_args.h
			const std::string v = argV[i+1];
			if(!strcmp(arg, "--open")) gStartup.open = v;
			else if(!strcmp(arg, "--open-any")) gStartup.openAny = v;
			else if(!strcmp(arg, "--capture")) gStartup.capture = v;
			else if(!strcmp(arg, "--cg")) gStartup.cg = v;
			else if(!strcmp(arg, "--pattern")) gStartup.pattern = atoi(v.c_str());
			else if(!strcmp(arg, "--frame")) gStartup.frame = atoi(v.c_str());
			else if(!strcmp(arg, "--game-link")) gStartup.gameLinkSlot = atoi(v.c_str());
			else if(!strcmp(arg, "--zoom")) gStartup.zoom = (float)atof(v.c_str());
			else if(!strcmp(arg, "--game")) gStartup.game = v;
			else if(!strcmp(arg, "--compare")) gStartup.compare = atoi(v.c_str());
			else if(!strcmp(arg, "--tool")) gStartup.tool = v;
			else if(!strcmp(arg, "--tag-ini")) gStartup.tagIni = v;
			else if(!strcmp(arg, "--tag-char")) gStartup.tagChar = v;
			else if(!strcmp(arg, "--tag-tab")) gStartup.tagTab = v;
			else if(!strcmp(arg, "--authoring-tab")) gStartup.authoring.tab = v;
			else if(!strcmp(arg, "--authoring-setup")) gStartup.authoring.setupHex = v;
			else if(!strcmp(arg, "--authoring-game")) gStartup.authoring.gameDir = v;
			else if(!strcmp(arg, "--authoring-tag-root")) gStartup.authoring.tagRoot = v;
			else if(!strcmp(arg, "--authoring-char")) gStartup.authoring.charFile = v;
			else if(!strcmp(arg, "--authoring-layer")) gStartup.authoring.layer = v;
			else if(!strcmp(arg, "--authoring-view")) gStartup.authoring.subTab = v;
			else if(!strcmp(arg, "--authoring-ab")) gStartup.authoring.abDemo = v;
			else if(!strcmp(arg, "--authoring-pid")) gStartup.authoring.pid = (uint32_t)strtoul(v.c_str(), nullptr, 0);
			else if(!strncmp(arg, "--authoring-", 12)) {}
			else gStartup.palette = atoi(v.c_str());
			i++;
		}
		else if(!strcmp(arg, "--tag-link"))
		{
			gStartup.tagLink = true;
		}
		else if(!strcmp(arg, "--authoring-link"))
		{
			gStartup.authoring.link = true;
		}
		else if(!strcmp(arg, "--authoring-load"))
		{
			gStartup.authoring.loadInGame = true;
		}
		else if(!strcmp(arg, "--authoring-unlock"))
		{
			gStartup.authoring.sessionUnlock = true;
		}
		else if(!strcmp(arg, "--tag-apply"))
		{
			gStartup.tagApply = true;
		}
		else if(!strcmp(arg, "--no-pups"))
		{
			gStartup.noPups = true;
		}
		else if(ParseWave2StartupArg(arg, i+1<argC ? argV[i+1] : nullptr, i))
		{
			// --tick/--onion/--detach/--capture-view/--export-*/--save-project
			// (render checks, see startup_args.h); consumed its value if any.
		}
		else if(!strcmp(arg, "-i"))
		{
			useIni = false;
		}
		else if(!strcmp(arg, "-r") && i+2<argC)
		{
			i+=1;
			int res[2];
			for(int j = 0; j < 2; j++)
				res[j] = atoi(argV[i+j]);
			gSettings.winSizeX = res[0];
			gSettings.winSizeY = res[1];
		}
	}

	//If it fails... Well, that works too.
	if(useIni)
	{
		int appendAt = GetCurrentDirectoryA(512, iniLocation);
		strcpy(iniLocation+appendAt, "\\hanteichan.ini");
	}
	
	// Build version string with build number and git hash
	std::string windowTitle = "gonpt\xc3\xa9chan v" VERSION_WITH_COMMIT;

	// --headless: a hidden window and a hidden GL context, never shown and with
	// no taskbar entry (SDL_WINDOW_HIDDEN | SDL_WINDOW_UTILITY, the old
	// WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE). Views render into their FBOs and
	// MainFrame::Draw sends the frame to an offscreen target.
	platform::InitDesc desc;
	desc.title = windowTitle;
	desc.w = gSettings.winSizeX;
	desc.h = gSettings.winSizeY;
	desc.x = gSettings.posX;
	desc.y = gSettings.posY;
	desc.headless = gStartup.headless;
	desc.maximized = useIni && gSettings.maximized;
	desc.openGL = true;
	desc.allowViewports = gSettings.detachableWindows && !gStartup.headless;
	std::string platformError;
	if (!platform::Init(desc, platformError)) {
		HC_LOG_ERROR("platform: {}", platformError);
		MessageBoxA(nullptr, platformError.c_str(), "gonptechan", MB_ICONERROR | MB_OK);
		return 3;
	}
	mainWindowHandle = (HWND)platform::NativeWindowHandle();
	init = true;

	int cw = 0, ch = 0;
	platform::ClientSize(cw, ch);
	clientRect = ImVec2((float)cw, (float)ch);

	// ImGui platform + GL backend. The window and context are already up, so
	// this is the WM_CREATE body without the wgl/glad part.
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImSearch::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		io.IniFilename = useIni ? iniLocation : nullptr;
		if (useIni) InitIni();
		if (gStartup.headless)
			io.IniFilename = nullptr;   // settings and layout are read, never written back
		io.ConfigWindowsMoveFromTitleBarOnly = true;

		LoadJapaneseFonts(io);
		ImGui_ImplSDL3_InitForOpenGL((SDL_Window*)platform::Window(), platform::GlContextHandle());
		ImGui_ImplOpenGL3_Init("#version 330 core");
		// Detached windows: the SDL3 + GL3 backends create a native window with
		// a shared GL context per viewport, so ViewportsEnable is only turned on
		// once both backends have registered their platform/renderer hooks.
		if (desc.allowViewports && viewports::Initialize(true))
			io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
		// vsync off, as wglSwapIntervalEXT(0) did.
		platform::SetSwapInterval(0);
	}

	MainFrame* frame = new MainFrame();

	// The SDL event pump owns the input translation; the app's own switch runs
	// after ImGui has seen each event (the old WndProc order). PostQuitMessage is
	// replaced by setting gAppQuit, which the loop below reads.
	platform::EventHandlers handlers;
	handlers.user = frame;
	handlers.onResize = [](int fbW, int fbH, void* user) {
		MainFrame* mf = (MainFrame*)user;
		int ww = 0, wh = 0; platform::WindowSize(ww, wh);
		gSettings.winSizeX = ww; gSettings.winSizeY = wh;
		int px = 0, py = 0; platform::WindowPos(px, py);
		gSettings.posX = px; gSettings.posY = py;
		gSettings.maximized = platform::IsMaximized();
		clientRect = ImVec2((float)fbW, (float)fbH);
		if (mf) mf->UpdateBackProj(clientRect.x, clientRect.y);
	};
	handlers.onMove = [](int x, int y, void*) { gSettings.posX = x; gSettings.posY = y; };
	handlers.onDropPoint = [](float sx, float sy, float cx, float cy, void* user) {
		if (user) ((MainFrame*)user)->setDropPoint(sx, sy, cx, cy);
	};
	handlers.onDropFile = [](const char* path, void* user) {
		if (user) ((MainFrame*)user)->queueDroppedFile(path);
	};
	// ImGui first (the SDL backend's ProcessEvent is called from here), then
	// the app's own WndProc-style switch. Nothing else pumps ImGui's input.
	handlers.onEvent = [](const void* ev, void* user) {
		const SDL_Event& e = *(const SDL_Event*)ev;
		ImGui_ImplSDL3_ProcessEvent(&e);
		AppSdlEvent(e, user);
	};
	platform::SetEventHandlers(handlers);

	const double targetFrameTime = 0.0165; // Slightly faster than 60 FPS to compensate for overhead
	using namespace std::chrono;
	steady_clock::time_point lastFrameTime = steady_clock::now();

	for (;;)
	{
		if (!platform::PollEvents()) break;
		if (gAppQuit) break;

		steady_clock::time_point currentTime = steady_clock::now();
		duration<double> elapsed = duration_cast<duration<double>>(currentTime - lastFrameTime);

		if (gStartup.headless || elapsed.count() >= targetFrameTime)   // headless: no frame pacing
		{
			frame->Draw();
			lastFrameTime = currentTime;
		}
		else
		{
			// Sleep for a very short time to avoid busy waiting
			std::this_thread::sleep_for(std::chrono::microseconds(1));
		}
	}

	delete frame;
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImSearch::DestroyContext();
	ImGui::DestroyContext();
	platform::Shutdown();


	return gStartup.exitCode;
}
