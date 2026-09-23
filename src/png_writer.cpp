#include "png_writer.h"

#include <windows.h>
#include <wincodec.h>
#include <shobjidl.h>
#include <filesystem>
#include <system_error>
#include <vector>

namespace {

template <typename T> void ReleaseCom(T*& p)
{
	if (p) { p->Release(); p = nullptr; }
}

// CoInitializeEx for the duration of one call; balanced only if it succeeded
// (S_FALSE = already initialised on this thread, still needs the uninit).
struct ComScope {
	bool owns = false;
	ComScope() { owns = SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)); }
	~ComScope() { if (owns) CoUninitialize(); }
};

} // namespace

std::wstring Utf8ToWide(const std::string& s)
{
	if (s.empty()) return std::wstring();
	const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
	std::wstring w(n > 0 ? n : 0, L'\0');
	if (n > 0) MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
	return w;
}

std::string WideToUtf8(const std::wstring& w)
{
	if (w.empty()) return std::string();
	const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
	std::string s(n > 0 ? n : 0, '\0');
	if (n > 0) WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
	return s;
}

std::string AnsiToUtf8(const std::string& s)
{
	if (s.empty()) return std::string();
	const int n = MultiByteToWideChar(CP_ACP, 0, s.data(), (int)s.size(), nullptr, 0);
	std::wstring w(n > 0 ? n : 0, L'\0');
	if (n > 0) MultiByteToWideChar(CP_ACP, 0, s.data(), (int)s.size(), w.data(), n);
	return WideToUtf8(w);
}

bool CreateDirectoriesUtf8(const std::string& utf8Dir, std::string& error)
{
	std::error_code ec;
	const std::filesystem::path dir(Utf8ToWide(utf8Dir));
	if (dir.empty()) { error = "No output folder was given."; return false; }
	std::filesystem::create_directories(dir, ec);   // never throws
	if (ec || !std::filesystem::is_directory(dir, ec)) {
		error = "Could not create the output folder: " + utf8Dir;
		return false;
	}
	return true;
}

bool WritePngRgba(const std::string& utf8Path, const uint8_t* rgba, int width, int height, std::string& error)
{
	if (!rgba || width <= 0 || height <= 0) { error = "The image is empty."; return false; }

	// WIC wants BGRA.
	std::vector<uint8_t> bgra((size_t)width * height * 4);
	for (size_t i = 0; i < (size_t)width * height; ++i) {
		bgra[i * 4 + 0] = rgba[i * 4 + 2];
		bgra[i * 4 + 1] = rgba[i * 4 + 1];
		bgra[i * 4 + 2] = rgba[i * 4 + 0];
		bgra[i * 4 + 3] = rgba[i * 4 + 3];
	}

	const std::wstring finalPath = Utf8ToWide(utf8Path);
	const std::wstring tempPath = finalPath + L".tmp";

	ComScope com;
	IWICImagingFactory* factory = nullptr;
	IWICStream* stream = nullptr;
	IWICBitmapEncoder* encoder = nullptr;
	IWICBitmapFrameEncode* frame = nullptr;
	IPropertyBag2* properties = nullptr;
	HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
		IID_IWICImagingFactory, reinterpret_cast<void**>(&factory));
	if (SUCCEEDED(hr)) hr = factory->CreateStream(&stream);
	if (SUCCEEDED(hr)) hr = stream->InitializeFromFilename(tempPath.c_str(), GENERIC_WRITE);
	if (SUCCEEDED(hr)) hr = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder);
	if (SUCCEEDED(hr)) hr = encoder->Initialize(stream, WICBitmapEncoderNoCache);
	if (SUCCEEDED(hr)) hr = encoder->CreateNewFrame(&frame, &properties);
	if (SUCCEEDED(hr)) hr = frame->Initialize(properties);
	if (SUCCEEDED(hr)) hr = frame->SetSize((UINT)width, (UINT)height);
	WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
	if (SUCCEEDED(hr)) hr = frame->SetPixelFormat(&format);
	if (SUCCEEDED(hr) && !IsEqualGUID(format, GUID_WICPixelFormat32bppBGRA)) hr = E_FAIL;
	if (SUCCEEDED(hr)) hr = frame->WritePixels((UINT)height, (UINT)width * 4, (UINT)bgra.size(), bgra.data());
	if (SUCCEEDED(hr)) hr = frame->Commit();
	if (SUCCEEDED(hr)) hr = encoder->Commit();
	ReleaseCom(properties);
	ReleaseCom(frame);
	ReleaseCom(encoder);
	ReleaseCom(stream);   // closes the file
	ReleaseCom(factory);

	if (FAILED(hr)) {
		DeleteFileW(tempPath.c_str());
		char buf[64];
		snprintf(buf, sizeof(buf), " (HRESULT 0x%08lX)", (unsigned long)hr);
		error = "Windows Imaging Component could not write " + utf8Path + buf;
		return false;
	}
	if (!MoveFileExW(tempPath.c_str(), finalPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
		DeleteFileW(tempPath.c_str());
		error = "Could not move the finished PNG into place: " + utf8Path;
		return false;
	}
	return true;
}

std::string BrowseForFolderUtf8(const std::string& initialUtf8)
{
	ComScope com;
	IFileOpenDialog* dialog = nullptr;
	std::string result;
	if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
		IID_IFileOpenDialog, reinterpret_cast<void**>(&dialog))))
		return result;
	DWORD options = 0;
	dialog->GetOptions(&options);
	dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR);
	dialog->SetTitle(L"Export PNG to folder");
	if (!initialUtf8.empty()) {
		IShellItem* folder = nullptr;
		const std::wstring w = Utf8ToWide(initialUtf8);
		if (SUCCEEDED(SHCreateItemFromParsingName(w.c_str(), nullptr, IID_IShellItem, reinterpret_cast<void**>(&folder)))) {
			dialog->SetFolder(folder);
			folder->Release();
		}
	}
	if (SUCCEEDED(dialog->Show(nullptr))) {
		IShellItem* item = nullptr;
		if (SUCCEEDED(dialog->GetResult(&item))) {
			PWSTR path = nullptr;
			if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path) {
				result = WideToUtf8(path);
				CoTaskMemFree(path);
			}
			item->Release();
		}
	}
	dialog->Release();
	return result;
}
