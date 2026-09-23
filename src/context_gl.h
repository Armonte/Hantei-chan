#ifndef CONTEXTGL_H_GUARD
#define CONTEXTGL_H_GUARD

#include <windows.h>

class ContextGl
{
private:
	HGLRC context;
	HWND hwnd;

public:
	HDC dc;
	HGLRC GetRenderContext() const { return context; }
	bool MakeCurrent() const { return wglMakeCurrent(dc, context) == TRUE; }
	ContextGl(HWND hWnd): hwnd(hWnd)
	{
		PIXELFORMATDESCRIPTOR pfd =
		{
			sizeof(PIXELFORMATDESCRIPTOR),
			1,
			PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,    // Flags
			PFD_TYPE_RGBA,        // The kind of framebuffer. RGBA or palette.
			32,                   // Colordepth of the framebuffer.
			0, 0, 0, 0, 0, 0,
			0,
			0,
			0,
			0, 0, 0, 0,
			24,                   // Number of bits for the depthbuffer
			8,                    // Number of bits for the stencilbuffer
			0,                    // Number of Aux buffers in the framebuffer.
			PFD_MAIN_PLANE,
			0,
			0, 0, 0
		};

		dc = GetDC(hWnd);
		int format = ChoosePixelFormat(dc, &pfd);
		SetPixelFormat(dc, format, &pfd);

		context = wglCreateContext (dc);
		if(!context)
			MessageBox(hWnd, L"Couldn't create context", L"wglCreateContext", MB_OK);
		wglMakeCurrent(dc, context);


		//
	}

	~ContextGl()
	{
		wglMakeCurrent (nullptr, nullptr);
		wglDeleteContext(context);
		ReleaseDC(hwnd, dc);
	}
};

#endif /* CONTEXTGL_H_GUARD */
