#include "app_log.h"
#include "texture.h"
#include "cg.h"
#include "parts/parts_texture.h"
#include <glad/glad.h>

#include <fstream>
#include <iostream>
#include <string>
#include <cassert>
#include <cstring>

// S3TC/DXT compression constants
#ifndef GL_R8
#define GL_R8 0x8229
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT 0x83F1
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT3_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT 0x83F2
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3
#endif

int pal = 0; //temporary

Texture::Texture() : id(0), isLoaded(false), isApplied(false){}; //initializes as 0
Texture::Texture(Texture&& texture)
{
	image = std::move(texture.image);
	pickMask=std::move(texture.pickMask);
	isIndexed=texture.isIndexed;
	id = texture.id;
	isLoaded = texture.isLoaded;
	isApplied = texture.isApplied;
	//filename = std::move(texture.filename);

	// Transfer ownership - prevent the moved-from object from deleting the texture
	texture.id = 0;
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
	pickMask.reset();
	image.reset(data);
	isLoaded = true;
}

void Texture::LoadDirect(char* data, int width, int height, bool bgr)
{
	pickMask.reset();
	// Load uncompressed RGB/RGBA data
	ImageData* imgData = new ImageData();
	imgData->width = width;
	imgData->height = height;
	imgData->offsetX = 0;
	imgData->offsetY = 0;
	imgData->bgr = bgr;
	imgData->is8bpp = false;
	imgData->pixels = new unsigned char[width * height * 4];

	// Copy RGBA/BGRA data
	memcpy(imgData->pixels, data, width * height * 4);

	image.reset(imgData);
	isLoaded = true;
}

void Texture::LoadCompressed(char* data, int width, int height, size_t compressedSize, int type)
{
	pickMask.reset();
	image.reset();
	isIndexed=false;
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
	} else if (type == 3) {
		format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT; // DXT3 (0x83F2)
	} else if (type == 5) {
		format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT; // DXT5 (0x83F3)
	} else {
		assert(0 && "Unknown compression type");
		return;
	}

	glCompressedTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, compressedSize, data);
 // The source compressed bytes may be released immediately after upload.
 // Decode coverage once here; picking never reads GL textures or decodes on hover.
 if(data && width>0 && height>0 && width<=8192 && height<=8192 && compressedSize<=size_t(width)*height*4) {
  std::vector<uint32_t> dds((128+compressedSize+3)/4,0);
  memcpy(dds.data(),"DDS ",4); dds[1]=124; dds[3]=height; dds[4]=width; dds[19]=32; dds[20]=4;
  memcpy(&dds[21],type==1?"DXT1":type==3?"DXT3":"DXT5",4);
  memcpy(reinterpret_cast<char*>(dds.data())+128,data,compressedSize);
  std::vector<uint8_t> rgba; int w=0,h=0;
  if(PartGfx<>::DecodeDds(reinterpret_cast<const char*>(dds.data()),128+compressedSize,rgba,w,h).empty())
   pickMask=spritepick::Mask::Build(rgba.data(),w,h,false);
 }
	
	// Check for OpenGL errors after compressed texture upload
	GLenum err = glGetError();
	if (err != GL_NO_ERROR) {
		HC_LOG_ERROR("[LoadCompressed] GL error {:#x} uploading {}x{} type={} compressed texture (size={}, format={:#x})",
			(unsigned)err, width, height, type, compressedSize, (unsigned)format);
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

	isIndexed = image->is8bpp;
	if (isIndexed) {
		//8bpp indexed: upload raw indices as R8; the sprite shader resolves the
		//palette (and does its own bilinear), so sample NEAREST here.
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, image->width, image->height, 0, GL_RED, GL_UNSIGNED_BYTE, image->pixels);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		return;
	}

	GLenum extType = image->bgr ? GL_BGRA : GL_RGBA;
	GLenum intType = GL_RGBA8;

	glTexImage2D(GL_TEXTURE_2D, 0, intType, image->width, image->height, 0, extType, GL_UNSIGNED_BYTE, image->pixels);

}
void Texture::Unapply()
{
	if (!isApplied) {
		return;  // Already unapplied, nothing to do
	}

	isApplied = false;
	// Unbind texture before deleting to prevent GL_INVALID_OPERATION
	glBindTexture(GL_TEXTURE_2D, 0);
	glDeleteTextures(1, &id);
	id = 0;  // Reset ID after deletion
}
void Texture::Unload()
{
	isLoaded = false;
	pickMask.reset();
	image.reset();
}

std::shared_ptr<const spritepick::Mask> Texture::coverage() const
{
 if(!pickMask && image) pickMask=spritepick::Mask::Build(image->pixels,image->width,image->height,image->is8bpp);
 return pickMask;
}
