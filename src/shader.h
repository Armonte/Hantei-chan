#ifndef SHADER_H_INCLUDED
#define SHADER_H_INCLUDED

#include <glm/mat4x4.hpp>

class Shader
{
public:
	unsigned int program;

	Shader();
	~Shader();
	Shader(const char *vertex_path, const char *fragment_path);

	void LoadShader(const char *vertex_path, const char *fragment_path, bool sourceString = false);
	void Use();
	int GetLoc(const char* name);
	int GetAttribLoc(const char* name);
	void BindAttrib(const char* name, int index);
};

#endif // SHADER_H_INCLUDED
