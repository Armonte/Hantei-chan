#include "main.h"
#include "context_gl.h"
#include "main_frame.h"
#include "test.h"

#include <iostream>
#include <fstream>
#include <cstring>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_win32.h>
#include <windows.h>
#include <shellapi.h>

#include <glad/glad.h>

#include <res/resource.h>

ImVec2 clientRect;

HWND mainWindowHandle;
static bool dragWindow;
static POINT mousePos;

void LoadJapaneseFonts(ImGuiIO& io)
{
	char winFolder[512]{};
	ImFontConfig config;
/* 	config.PixelSnapH = 1;
	config.OversampleH = 1;
	config.OversampleV = 1; */
	int appendAt = GetWindowsDirectoryA(winFolder, 512);
	strcpy(winFolder+appendAt, "\\Fonts\\meiryo.ttc");

	if(!io.Fonts->AddFontFromFileTTF(winFolder, 20.0f, &config, io.Fonts->GetGlyphRangesJapanese()))
	{
		auto res = FindResource((HMODULE)GetWindowLongPtr(mainWindowHandle, GWLP_HINSTANCE), MAKEINTRESOURCE(NOTO_SANS_JP_F), RT_RCDATA);
		void *notoFont = LockResource(LoadResource(nullptr, res));
		config.FontDataOwnedByAtlas = false;
		
		io.Fonts->AddFontFromMemoryTTF(notoFont, SizeofResource(nullptr, res), 20.0f, &config, io.Fonts->GetGlyphRangesJapanese());
	}
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR pCmdLine, int nCmdShow)
{
	
	int argC;
	PWSTR* argV = CommandLineToArgvW(pCmdLine, &argC);
	for(int i=0; i<argC; i++)
	{
		char * arg = (char*)(argV[i]);
		wcstombs(arg, argV[i], wcslen(argV[i])+1);
		if(!strcmp(arg, "--test"))
		{
			std::ofstream coutFile, cerrFile;
			coutFile.open("cout.txt"); cerrFile.open("cerr.txt");
			auto cout_buf = std::cout.rdbuf(coutFile.rdbuf());
			auto cerr_buf = std::cerr.rdbuf(cerrFile.rdbuf());
			TestHa6();
			LocalFree(argV);
			return 0;
		}
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
	HWND hwnd = ::CreateWindow(wc.lpszClassName, L"HA6GUI: 判定ちゃん６", WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, NULL, NULL, wc.hInstance, nullptr);
	mainWindowHandle = hwnd;
	::ShowWindow(hwnd, nCmdShow);
	::UpdateWindow(hwnd);

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		MainFrame* mf = (MainFrame*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
		if(mf)
			mf->Draw();
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
			ContextGl *context = new ContextGl(hWnd);
			if(!gladLoadGL())
			{
				MessageBox(0, L"glad fail", L"gladLoadGL()", 0);
				PostQuitMessage(1);
				return 1;
			}
			IMGUI_CHECKVERSION();
			ImGui::CreateContext();
			ImGuiIO& io = ImGui::GetIO();
			io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
			io.IniFilename = nullptr;
			//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
			LoadJapaneseFonts(io);
			ImGui_ImplWin32_Init(hWnd);
			ImGui_ImplOpenGL3_Init("#version 120");

			MainFrame* mf = new MainFrame(context);
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)mf);
			return 0;
		}
	case WM_SIZE:
		if (wParam != SIZE_MINIMIZED)
		{
			RECT rect;
			GetClientRect(mainWindowHandle, &rect);
			clientRect = ImVec2((float)rect.right, (float)rect.bottom);
			mf->UpdateBackProj(clientRect.x, clientRect.y);
		}
		return 0;
	case WM_KEYDOWN:
		if(!ImGui::GetIO().WantCaptureKeyboard)
		{
			if(mf->HandleKeys(wParam));
				return 0;
		}
		break;
	case WM_LBUTTONDOWN:
		if(!ImGui::GetIO().WantCaptureMouse)
		{
			dragWindow = true;
			GetCursorPos(&mousePos);
			ScreenToClient(hWnd, &mousePos);
			SetCapture(hWnd);
			
			return 0;
		}
		break;
	case WM_LBUTTONUP:
		if(dragWindow)
		{
			ReleaseCapture();
			dragWindow = false;
			return 0;
		}
		break;
	case WM_MOUSEMOVE:
		if(dragWindow)
		{
			POINT newMousePos;
			newMousePos.x = (short) LOWORD(lParam);
			newMousePos.y = (short) HIWORD(lParam);

			mf->HandleMouseDrag(newMousePos.x-mousePos.x, newMousePos.y-mousePos.y);
			mousePos = newMousePos;
		}
		break;
	case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
			return 0;
		break;
	case WM_DESTROY:
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
		delete mf;
		SetWindowLongPtr(hWnd, GWLP_USERDATA, 0);
		::PostQuitMessage(0);
		return 0;
	}

	return ::DefWindowProc(hWnd, msg, wParam, lParam);
}
