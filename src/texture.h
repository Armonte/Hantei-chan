#ifndef IMAGE_H_INCLUDED
#define IMAGE_H_INCLUDED
#include <string>
#include "cg.h"
#include <memory>

extern int pal;

class Texture
{
public: //public only for ease of reading access, do not write anything.
	std::unique_ptr<ImageData> image;
	unsigned int id; //OpenGL texture id.
	bool isLoaded;
	bool isApplied;
	
public:
	Texture();
	Texture(Texture&& texture);
	~Texture();

	
	void Load(ImageData *data);
	void LoadDirect(char* data, int width, int height, bool bgr = false);
	void LoadCompressed(char* data, int width, int height, size_t compressedSize, int type);
	void Apply(bool repeat = false, bool linearFilter = false);
	void Unapply();
	void Unload();
};

#endif // IMAGE_H_INCLUDED
