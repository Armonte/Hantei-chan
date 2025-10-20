#include "texture.h"
#include "cg.h"
#include <glad/glad.h>

#include <fstream>
#include <iostream>
#include <string>
#include <cassert>
#include <cstring>

// S3TC/DXT compression constants
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT 0x83F1
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3
#endif

int pal = 0; //temporary

Texture::Texture() : id(0), isLoaded(false), isApplied(false){}; //initializes as 0
Texture::Texture(Texture&& texture)
{
	image = std::move(texture.image);
	id = texture.id;
	isLoaded = texture.isLoaded;
	isApplied = texture.isApplied;
	//filename = std::move(texture.filename);

	texture.isApplied = false;
	texture.isLoaded = false;
}

Texture::~Texture()
{
	if(isApplied) //This gets triggered by vector.resize() when the class instance is NOT a pointer.
		Unapply();
	if(isLoaded)
		Unload();
}

void Texture::Load(ImageData *data)
{
	const char *palette = nullptr;
	image.reset(data);
	isLoaded = true;
}

void Texture::LoadDirect(char* data, int width, int height, bool bgr)
{
	// Load uncompressed RGB/RGBA data
	ImageData* imgData = new ImageData();
	imgData->width = width;
	imgData->height = height;
	imgData->offsetX = 0;
	imgData->offsetY = 0;
	imgData->bgr = bgr;
	imgData->pixels = new unsigned char[width * height * 4];

	// Copy RGBA/BGRA data
	memcpy(imgData->pixels, data, width * height * 4);

	image.reset(imgData);
	isLoaded = true;
}

void Texture::LoadCompressed(char* data, int width, int height, size_t compressedSize, int type)
{
	// For compressed textures (DXT1/DXT5), we need to upload directly to OpenGL
	// without going through ImageData
	isLoaded = true;
	isApplied = true;

	glGenTextures(1, &id);
	glBindTexture(GL_TEXTURE_2D, id);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	// Determine OpenGL format
	GLenum format;
	if (type == 1) {
		format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT; // DXT1 (0x83F1)
	} else if (type == 5) {
		format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT; // DXT5 (0x83F3)
	} else {
		assert(0 && "Unknown compression type");
		return;
	}

	glCompressedTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, compressedSize, data);
	
	// Check for OpenGL errors after compressed texture upload
	GLenum err = glGetError();
	if (err != GL_NO_ERROR) {
		printf("[LoadCompressed] GL ERROR 0x%X uploading %dx%d type=%d compressed texture (size=%zu)\n",
			err, width, height, type, compressedSize);
		printf("[LoadCompressed] Format=0x%X, data=%p\n", format, data);
	} else {
		printf("[LoadCompressed] SUCCESS: %dx%d type=%d DXT%d texture (GL ID=%u, size=%zu)\n",
			width, height, type, (type == 1 ? 1 : 5), id, compressedSize);
	}

	// Create minimal ImageData for compatibility
	ImageData* imgData = new ImageData();
	imgData->width = width;
	imgData->height = height;
	imgData->offsetX = 0;
	imgData->offsetY = 0;
	imgData->pixels = nullptr;
	image.reset(imgData);
}

void Texture::Apply(bool repeat, bool linearFilter)
{
	// Don't apply twice - prevents texture ID leaks and nullptr upload
	if (isApplied) {
		return;
	}
	
	isApplied = true;
	glGenTextures(1, &id);

	glBindTexture(GL_TEXTURE_2D, id);
	if(repeat)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
	}
	else
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	}
	if(linearFilter)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}
	else
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	}

	GLenum extType = image->bgr ? GL_BGRA : GL_RGBA;
	GLenum intType = GL_RGBA8;

	glTexImage2D(GL_TEXTURE_2D, 0, intType, image->width, image->height, 0, extType, GL_UNSIGNED_BYTE, image->pixels);

}
void Texture::Unapply()
{
	isApplied = false;
	// Unbind texture before deleting to prevent GL_INVALID_OPERATION
	glBindTexture(GL_TEXTURE_2D, 0);
	glDeleteTextures(1, &id);
}
void Texture::Unload()
{
	isLoaded = false;
	image.reset();
}
