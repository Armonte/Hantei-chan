// PNG -> GL texture for the HUD preview window (issue #56).
#include <glad/glad.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_WINDOWS_UTF8 // paths are UTF-8
#include "../third_party/stb_image/stb_image.h"

#include <string>

unsigned LoadPngTexture(const std::string& path, int* w, int* h)
{
	int n = 0;
	unsigned char* px = stbi_load(path.c_str(), w, h, &n, 4);
	if (!px) return 0;
	GLuint tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, *w, *h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
	glBindTexture(GL_TEXTURE_2D, 0);
	stbi_image_free(px);
	return tex;
}

void FreeTexture(unsigned tex)
{
	GLuint t = tex;
	if (t) glDeleteTextures(1, &t);
}
