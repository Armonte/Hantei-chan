#include "main.h"
#include "context_gl.h"
#include "main_frame.h"
#include "test.h"
#include "ini.h"
#include "version.h"

#include <iostream>
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <imgui.h>
#include "imsearch.h"

#include <imgui_impl_opengl3.h>
#include <imgui_impl_win32.h>
#include <windows.h>
#include <shellapi.h>

#include <glad/glad.h>

#include <res/resource.h>

#ifndef HA6GUIVERSION
#define HA6GUIVERSION " custom"
#endif

char iniLocation[512] {};

ImVec2 clientRect;

HWND mainWindowHandle;
ContextGl *context = nullptr;
static bool dragLeft = false, dragRight = false;
static POINT mousePos;
static bool justActivated = false;  // Track if window just became active
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
						printf("[Font] Noto Sans JP loaded with symbol support\n");
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
			printf("[Font] Meiryo loaded with symbol support (Noto not available)\n");
			
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
							printf("[Font] Noto merged into Meiryo for missing glyphs\n");
						}
					}
				}
			}
		}
	}
	
	// Set the Japanese font as the default font if it loaded successfully
	if(japaneseFont) {
		io.FontDefault = japaneseFont;
		printf("[Font] Japanese font loaded with symbol support and set as default\n");
		
		// Debug: Check if specific glyphs are available
		// Note: This check happens before Build(), so we can't verify glyphs yet
		// But we can verify the ranges were set correctly
		printf("[Font] Glyph ranges size: %d\n", g_fontGlyphRanges.Size);
		if(g_fontGlyphRanges.Size > 0) {
			printf("[Font] First few glyph ranges: ");
			for(int i = 0; i < g_fontGlyphRanges.Size && i < 20; i++) {
				printf("0x%04X ", g_fontGlyphRanges[i]);
			}
			printf("\n");
		}
	} else {
		printf("[Font] WARNING: Failed to load Japanese font! Japanese characters may not display correctly.\n");
	}
}


LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR pCmdLine, int nCmdShow)
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
	int argC;
	PWSTR* argV = CommandLineToArgvW(pCmdLine, &argC);
	for(int i=0; i<argC; i++)
	{
		char * arg = (char*)(argV[i]);
		wcstombs(arg, argV[i], wcslen(argV[i])+1);
		if(!strcmp(arg, "--test"))
		{
			std::ofstream coutFile, cerrFile;
			coutFile.open("cout.txt");
			auto cout_buf = std::cout.rdbuf(coutFile.rdbuf());
			TestHa6();
			LocalFree(argV);
			return 0;
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
			{
				char * arg = (char*)(argV[i+j]);
				wcstombs(arg, argV[i+j], wcslen(argV[i+j])+1);
				res[j] = atoi(arg);
			}
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
	
	WNDCLASSEX wc = {
		sizeof(WNDCLASSEX),
		CS_CLASSDC,
		WndProc,
		0L, 0L,
		hInstance,
		NULL, NULL, NULL, NULL,
		L"Main window", NULL 
	};

	
	::RegisterClassEx(&wc);
	// Build version string with build number and git hash
	std::wstring windowTitle = L"gonptéchan v" VERSION_WITH_COMMIT_W;
	HWND hwnd = ::CreateWindow(wc.lpszClassName, windowTitle.c_str(), WS_OVERLAPPEDWINDOW,
		gSettings.posX, gSettings.posY, gSettings.winSizeX, gSettings.winSizeY, NULL, NULL, wc.hInstance, nullptr);
	mainWindowHandle = hwnd;

	init = true;
	if(useIni)
		ShowWindow(hwnd, gSettings.maximized ? SW_SHOWMAXIMIZED : SW_NORMAL);
	else
		ShowWindow(hwnd, nCmdShow);
		
	UpdateWindow(hwnd);

	MSG msg = {};
	bool done = false;
	const double targetFrameTime = 0.0165; // Slightly faster than 60 FPS to compensate for overhead

	using namespace std::chrono;
	steady_clock::time_point lastFrameTime = steady_clock::now();

	while (!done)
	{
		// Process all pending messages
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			if (msg.message == WM_QUIT)
				done = true;
		}
		if (done)
			break;

		// Frame rate limiting - only render if enough time has passed
		steady_clock::time_point currentTime = steady_clock::now();
		duration<double> elapsed = duration_cast<duration<double>>(currentTime - lastFrameTime);
		
		if (elapsed.count() >= targetFrameTime)
		{
			MainFrame* mf = (MainFrame*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
			if(mf)
			{
				mf->Draw();
			}
			lastFrameTime = currentTime;
		}
		else
		{
			// Sleep for a very short time to avoid busy waiting
			std::this_thread::sleep_for(std::chrono::microseconds(1));
		}
	}

	DestroyWindow(hwnd);
	UnregisterClass(wc.lpszClassName, wc.hInstance);

	LocalFree(argV);
	return 0;
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		return true;

	MainFrame* mf = (MainFrame*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	switch (msg)
	{
	case WM_CREATE:
		{
			context = new ContextGl(hWnd);
			if(!gladLoadGL())
			{
				MessageBox(0, L"glad fail", L"gladLoadGL()", 0);
				PostQuitMessage(1);
				return 1;
			}
			IMGUI_CHECKVERSION();
			ImGui::CreateContext();
			ImSearch::CreateContext();
			ImGuiIO& io = ImGui::GetIO();
			io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
			io.IniFilename = iniLocation;
			InitIni();

			MainFrame* mf = new MainFrame(context);
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)mf);
			
			MoveWindow(hWnd, gSettings.posX, gSettings.posY, gSettings.winSizeX, gSettings.winSizeY, false);
			

			LoadJapaneseFonts(io);
			
			// Note: TexDesiredWidth was removed in ImGui 1.92.0
			// Font atlas now handles sizing automatically
			
			//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
			ImGui_ImplWin32_Init(hWnd);
			ImGui_ImplOpenGL3_Init("#version 330 core");
			
			// Enable VSync control for better performance testing
			typedef BOOL (WINAPI *wglSwapIntervalEXT_t)(int);
			wglSwapIntervalEXT_t wglSwapIntervalEXT = (wglSwapIntervalEXT_t)wglGetProcAddress("wglSwapIntervalEXT");
			if (wglSwapIntervalEXT) {
				wglSwapIntervalEXT(0);  // Disable VSync for maximum FPS
			}
			
			return 0;
		}
	case WM_MOVE:
		{
			RECT rect;
			GetWindowRect(hWnd, &rect);
			gSettings.posX = rect.left;
			gSettings.posY = rect.top;
		}
		break;
	case WM_SIZE:
		if(init)
		{
			if (wParam == SIZE_MAXIMIZED)
				gSettings.maximized = true;
			else if (wParam == SIZE_RESTORED)
				gSettings.maximized = false;
			if (wParam != SIZE_MINIMIZED)
			{
				RECT rect;
				GetWindowRect(hWnd, &rect);
				gSettings.posX = rect.left;
				gSettings.posY = rect.top;
				gSettings.winSizeX = rect.right-rect.left;
				gSettings.winSizeY = rect.bottom-rect.top;


				GetClientRect(hWnd, &rect);
				clientRect = ImVec2((float)rect.right, (float)rect.bottom);
				mf->UpdateBackProj(clientRect.x, clientRect.y);
			}
		}
		return 0;
	case WM_KEYDOWN:
		if(!ImGui::GetIO().WantCaptureKeyboard)
		{
			if(mf->HandleKeys(wParam))
				return 0;
		}
		break;
	case WM_RBUTTONDOWN:
		// Don't allow drag if window just became active - prevents dragging when clicking back into window
		if(!ImGui::GetIO().WantCaptureMouse && !justActivated)
		{
			dragRight = true;
			GetCursorPos(&mousePos);
			ScreenToClient(hWnd, &mousePos);
			SetCapture(hWnd);
			mf->RightClick(mousePos.x, mousePos.y);

			return 0;
		}
		justActivated = false;  // Clear flag after first click
		break;
	case WM_RBUTTONUP:
		if(dragRight)
		{
			if(!dragLeft)
				ReleaseCapture();
			dragRight = false;
			return 0;
		}
		break;
	case WM_LBUTTONDOWN:
		// Don't allow drag if window just became active - prevents dragging when clicking back into window
		if(!ImGui::GetIO().WantCaptureMouse && !justActivated)
		{
			dragLeft = true;
			GetCursorPos(&mousePos);
			ScreenToClient(hWnd, &mousePos);
			SetCapture(hWnd);

			return 0;
		}
		justActivated = false;  // Clear flag after first click
		break;
	case WM_LBUTTONUP:
		if(dragLeft)
		{
			if(!dragRight)
				ReleaseCapture();
			dragLeft = false;
			return 0;
		}
		break;
	case WM_MOUSEMOVE:
		if(dragLeft || dragRight)
		{
			POINT newMousePos;
			newMousePos.x = (short) LOWORD(lParam);
			newMousePos.y = (short) HIWORD(lParam);

			mf->HandleMouseDrag(newMousePos.x-mousePos.x, newMousePos.y-mousePos.y, dragRight, dragLeft);
			mousePos = newMousePos;
		}
		break;
	case WM_MOUSEWHEEL:
		if(mf && !ImGui::GetIO().WantCaptureMouse && !(dragLeft || dragRight))
		{
			int wheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);
			if (wheelDelta > 0) {
				mf->HandleMouseWheel(true);  // Scroll up = zoom in
			} else {
				mf->HandleMouseWheel(false); // Scroll down = zoom out
			}
			return 0;
		}
		break;
	case WM_ACTIVATE:
		// Set flag when window becomes active
		if(LOWORD(wParam) != WA_INACTIVE)
		{
			justActivated = true;
		}
		else
		{
			// Clear drag states when losing focus
			if(dragLeft || dragRight)
			{
				ReleaseCapture();
				dragLeft = false;
				dragRight = false;
			}
		}
		break;
	case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
			return 0;
		break;
	case WM_DESTROY:
		delete mf;
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImSearch::DestroyContext();
		ImGui::DestroyContext();
		delete context;
		SetWindowLongPtr(hWnd, GWLP_USERDATA, 0);
		::PostQuitMessage(0);
		return 0;
	}

	return ::DefWindowProc(hWnd, msg, wParam, lParam);
}
