#ifndef RENDER_H_GUARD
#define RENDER_H_GUARD

#include "cg.h"
#include "texture.h"
#include "shader.h"
#include "vao.h"
#include "hitbox.h"
#include <vector>
#include <unordered_map>
#include <glm/mat4x4.hpp>

class Render
{
private:
	glm::mat4 projection, view;
	
	CG *cg;
	Vao vSprite;
	Vao vGeometry;
	enum{
		LINES = 0,
		BOXES,
		GEO_SIZE
	};
	int geoParts[GEO_SIZE];
	float imageVertex[6*4];
	std::vector<float> clientQuads;
	int quadsToDraw;

	int lProjectionS, lProjectionT;
	int lAlphaS;
	Shader sSimple;
	Shader sTextured;
	Texture texture;
	float colorRgba[4];

	int curImageId;
	

	void AdjustImageQuad(int x, int y, int w, int h);
	void SetModelView(glm::mat4&& view);
	void SetMatrix(int location);
	void SetBlendingMode();

public:
	int x, offsetX;
	int y, offsetY;
	float scale;
	float scaleX, scaleY;
	float rotX, rotY, rotZ;
	
	Render();
	void Draw();
	void UpdateProj(float w, float h);

	void GenerateHitboxVertices(const std::unordered_map<int, Hitbox> &hitboxes, int size);
	void SetCg(CG *cg);
	void SwitchImage(int id);
	void DontDraw();
	void SetImageColor(float *rgbaArr);

	enum blendType{
		normal,
		additive,
		substractive
	};

	blendType blendingMode;
};

#endif /* RENDER_H_GUARD */
