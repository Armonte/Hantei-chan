#include "render.h"
#include "main.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/constants.hpp>
#include <algorithm>

//Error
#include <windows.h>
#include <glad/glad.h>
#include <sstream>

#include "hitbox.h"
#include "parts/parts.h"

constexpr int maxBoxes = 33;

const char* simpleSrcVert = R"(
#version 330 core
layout (location = 0) in vec3 Position;
layout (location = 1) in vec3 Color;

out vec4 Frag_Color;

uniform mat4 ProjMtx;
uniform float Alpha;

void main()
{
    
    Frag_Color = vec4(Color, Alpha);
    gl_Position = ProjMtx * vec4(Position, 1);
};
)";

const char* simpleSrcFrag = R"(
#version 330 core

in vec4 Frag_Color;
out vec4 FragColor;

void main()
{
    FragColor = Frag_Color;
};
)";

const char* texturedSrcVert = R"(
#version 330 core
layout (location = 0) in vec2 Position;
layout (location = 1) in vec2 UV;
layout (location = 2) in vec4 Color;

out vec2 Frag_UV;
out vec4 Frag_Color;

uniform mat4 ProjMtx;

void main()
{
    Frag_UV = UV;
    Frag_Color = Color;
    gl_Position = ProjMtx * vec4(Position.xy, 0, 1);
};
)";

const char* texturedSrcFrag = R"(
#version 330 core
uniform sampler2D Texture;

in vec2 Frag_UV;
in vec4 Frag_Color;
out vec4 FragColor;

void main()
{
    vec4 col = texture(Texture, Frag_UV.st);

    FragColor = col * Frag_Color;
};
)";

// Parts shader with flip support, additive color, and vertex color tinting
const char* partsSrcVert = R"(
#version 330 core
layout (location = 0) in vec3 Position;
layout (location = 1) in vec4 UV;
layout (location = 2) in vec4 Color;

out vec2 Frag_UV;
out vec4 Frag_Color;

uniform mat4 ProjMtx;
uniform int flip;

void main()
{
    // Choose between s,t or p,q based on flip
    if (flip != 0) {
        Frag_UV = UV.pq;
    } else {
        Frag_UV = UV.st;
    }
    Frag_Color = Color;
    gl_Position = ProjMtx * vec4(Position, 1);
}
)";

const char* partsSrcFrag = R"(
#version 330 core
uniform sampler2D Texture;
uniform vec3 addColor;

in vec2 Frag_UV;
in vec4 Frag_Color;
out vec4 FragColor;

void main()
{
    vec4 col = texture(Texture, Frag_UV);
    col.rgba *= Frag_Color;  // Apply vertex color tint (BGRA color from part)
    col.rgb += addColor;     // Apply additive color
    FragColor = col;
}
)";


Render::Render():
cg(nullptr),
m_parts(nullptr),
filter(false),
vSprite(Vao::F2F2, GL_DYNAMIC_DRAW),
vGeometry(Vao::F3F3, GL_STREAM_DRAW),
imageVertex{
	256, 256, 	0, 0,
	512, 256,  	1, 0, 
	512, 512,  	1, 1, 

	512, 512, 	1, 1,
	256, 512,  	0, 1,
	256, 256,  	0, 0,
},
colorRgba{1,1,1,1},
curImageId(-1),
quadsToDraw(0),
x(0), offsetX(0),
y(0), offsetY(0),
rotX(0), rotY(0), rotZ(0),
blendingMode(normal)
{
	sSimple.BindAttrib("Position", 0);
	sSimple.BindAttrib("Color", 1);
	//sSimple.LoadShader("src/simple.vert", "src/simple.frag");
	sSimple.LoadShader(simpleSrcVert, simpleSrcFrag, true);

	sSimple.Use();

	sTextured.BindAttrib("Position", 0);
	sTextured.BindAttrib("UV", 1);
	sTextured.BindAttrib("Color", 2);
	//sTextured.LoadShader("src/textured.vert", "src/textured.frag");
	sTextured.LoadShader(texturedSrcVert, texturedSrcFrag, true);

	sPartShader.BindAttrib("Position", 0);
	sPartShader.BindAttrib("UV", 1);
	sPartShader.BindAttrib("Color", 2);
	sPartShader.LoadShader(partsSrcVert, partsSrcFrag, true);

	lAlphaS = sSimple.GetLoc("Alpha");
	lProjectionS = sSimple.GetLoc("ProjMtx");
	lProjectionT = sTextured.GetLoc("ProjMtx");
	lProjectionParts = sPartShader.GetLoc("ProjMtx");
	lFlipParts = sPartShader.GetLoc("flip");
	lAddColorParts = sPartShader.GetLoc("addColor");

	vSprite.Prepare(sizeof(imageVertex), imageVertex);
	vSprite.Load();

	float lines[]
	{
		-10000, 0, -1,	1,1,1,
		10000, 0, -1,	1,1,1,
		0, 10000, -1,	1,1,1,
		0, -10000, -1,	1,1,1,
	};
	

	geoParts[LINES] = vGeometry.Prepare(sizeof(lines), lines);
	geoParts[BOXES] = vGeometry.Prepare(sizeof(float)*6*4*maxBoxes, nullptr);
	vGeometry.Load();
	vGeometry.InitQuads(geoParts[BOXES]);

	UpdateProj(clientRect.x, clientRect.y);

	glViewport(0, 0, clientRect.x, clientRect.y);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_DEPTH_TEST);
}

void Render::DrawGridLines()
{
	if(int err = glGetError())
	{
		std::stringstream ss;
		ss << "GL Error: 0x" << std::hex << err << "\n";
		MessageBoxA(nullptr, ss.str().c_str(), "GL Error", MB_ICONSTOP);
	}

	//Lines only
	glm::mat4 view = glm::mat4(1.f);
	view = glm::scale(view, glm::vec3(scale, scale, 1.f));
	view = glm::translate(view, glm::vec3(x,y,0.f));
	SetModelView(std::move(view));
	glUniform1f(lAlphaS, 0.25f);
	SetMatrix(lProjectionS);
	vGeometry.Bind();
	vGeometry.Draw(geoParts[LINES], 0, GL_LINES);
}

void Render::Draw()
{
	if(int err = glGetError())
	{
		std::stringstream ss;
		ss << "GL Error: 0x" << std::hex << err << "\n";
		MessageBoxA(nullptr, ss.str().c_str(), "GL Error", MB_ICONSTOP);
		//PostQuitMessage(1);
	}


	//Lines
	glm::mat4 view = glm::mat4(1.f);
	view = glm::scale(view, glm::vec3(scale, scale, 1.f));
	view = glm::translate(view, glm::vec3(x,y,0.f));
	SetModelView(std::move(view));
	glUniform1f(lAlphaS, 0.25f);
	SetMatrix(lProjectionS);
	vGeometry.Bind();
	vGeometry.Draw(geoParts[LINES], 0, GL_LINES);

	// Check if we should use Parts rendering
	if (usePat && m_parts && m_parts->loaded)
	{

		// Use Parts rendering with callbacks
		constexpr float tau = glm::pi<float>()*2.f;
		glm::mat4 baseView = glm::mat4(1.f);
		baseView = glm::scale(baseView, glm::vec3(scale, scale, 1.f));
		baseView = glm::translate(baseView, glm::vec3(x, y, 0.f));

		sPartShader.Use();
		
		// Disable vertex attribute array for color so glVertexAttrib4fv sets a constant value
		glDisableVertexAttribArray(2);

		// Callback to set matrix transform
		auto setMatrix = [this, &baseView, tau](glm::mat4 partMatrix) {
			glm::mat4 finalView = baseView * partMatrix;
			glUniformMatrix4fv(lProjectionParts, 1, GL_FALSE, glm::value_ptr(projection * finalView));
		};

		// Callback to set additive color
		auto setAddColor = [this](float r, float g, float b) {
			glUniform3f(lAddColorParts, r, g, b);
		};

		// Callback to set flip mode
		auto setFlip = [this](char flip) {
			glUniform1i(lFlipParts, (int)flip);
		};

		// Draw Parts with interpolation
		m_parts->Draw(curPattern, curNextPattern, curInterp, setMatrix, setAddColor, setFlip, colorRgba);

		// Draw hitboxes after Parts
		sSimple.Use();
		vGeometry.Bind();
		glUniform1f(lAlphaS, 0.6f);
		vGeometry.DrawQuads(GL_LINE_LOOP, quadsToDraw);
		glUniform1f(lAlphaS, 0.3f);
		vGeometry.DrawQuads(GL_TRIANGLE_FAN, quadsToDraw);

		return;  // Skip sprite rendering
	}

	//Sprite
	constexpr float tau = glm::pi<float>()*2.f;
	view = glm::mat4(1.f);
	view = glm::scale(view, glm::vec3(scale, scale, 1.f));
	view = glm::translate(view, glm::vec3(x,y,0.f));
	// Apply scale and rotations based on AFRT flag
	if (AFRT) {
		// AFRT=true: Scale → X → Y → Z (PR #39 order - fixes pattern 15)
		view = glm::scale(view, glm::vec3(scaleX,scaleY,0));
		view = glm::rotate(view, rotX*tau, glm::vec3(1.0, 0.f, 0.f));
		view = glm::rotate(view, rotY*tau, glm::vec3(0.0, 1.f, 0.f));
		view = glm::rotate(view, rotZ*tau, glm::vec3(0.0, 0.f, 1.f));
	} else {
		// AFRT=false: Scale → Z → Y → X (EXACT original pre-PR#39 order)
		view = glm::scale(view, glm::vec3(scaleX,scaleY,0));
		view = glm::rotate(view, rotZ*tau, glm::vec3(0.0, 0.f, 1.f));
		view = glm::rotate(view, rotY*tau, glm::vec3(0.0, 1.f, 0.f));
		view = glm::rotate(view, rotX*tau, glm::vec3(1.0, 0.f, 0.f));
	}
	view = glm::translate(view, glm::vec3(-128+offsetX,-224+offsetY,0.f));
	SetModelView(std::move(view));
	sTextured.Use();
	SetMatrix(lProjectionT);
	if(texture.isApplied)
	{
		SetBlendingMode();
		glDisableVertexAttribArray(2);
		glVertexAttrib4fv(2, colorRgba);
		vSprite.Bind();
		vSprite.Draw(0);
	}
	//Reset state
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBlendEquation(GL_FUNC_ADD);

	//Boxes
	sSimple.Use();
	vGeometry.Bind();
	glUniform1f(lAlphaS, 0.6f);
	vGeometry.DrawQuads(GL_LINE_LOOP, quadsToDraw);
	glUniform1f(lAlphaS, 0.3f);
	vGeometry.DrawQuads(GL_TRIANGLE_FAN, quadsToDraw);
}

void Render::DrawSpriteOnly()
{
	if(int err = glGetError())
	{
		std::stringstream ss;
		ss << "GL Error: 0x" << std::hex << err << "\n";
		MessageBoxA(nullptr, ss.str().c_str(), "GL Error", MB_ICONSTOP);
	}

	// Disable depth write so layers don't occlude each other
	// We still depth TEST against the background, but don't write to depth buffer
	glDepthMask(GL_FALSE);

	//Sprite (with full transform including offset)
	constexpr float tau = glm::pi<float>()*2.f;
	glm::mat4 view = glm::mat4(1.f);
	view = glm::scale(view, glm::vec3(scale, scale, 1.f));
	view = glm::translate(view, glm::vec3(x,y,0.f));
	// Apply scale and rotations based on AFRT flag
	if (AFRT) {
		// AFRT=true: Scale → X → Y → Z (PR #39 order - fixes pattern 15)
		view = glm::scale(view, glm::vec3(scaleX,scaleY,0));
		view = glm::rotate(view, rotX*tau, glm::vec3(1.0, 0.f, 0.f));
		view = glm::rotate(view, rotY*tau, glm::vec3(0.0, 1.f, 0.f));
		view = glm::rotate(view, rotZ*tau, glm::vec3(0.0, 0.f, 1.f));
	} else {
		// AFRT=false: Scale → Z → Y → X (EXACT original pre-PR#39 order)
		view = glm::scale(view, glm::vec3(scaleX,scaleY,0));
		view = glm::rotate(view, rotZ*tau, glm::vec3(0.0, 0.f, 1.f));
		view = glm::rotate(view, rotY*tau, glm::vec3(0.0, 1.f, 0.f));
		view = glm::rotate(view, rotX*tau, glm::vec3(1.0, 0.f, 0.f));
	}
	view = glm::translate(view, glm::vec3(-128+offsetX,-224+offsetY,0.f));
	SetModelView(std::move(view));
	sTextured.Use();
	SetMatrix(lProjectionT);
	if(texture.isApplied)
	{
		SetBlendingMode();
		glDisableVertexAttribArray(2);
		glVertexAttrib4fv(2, colorRgba);
		vSprite.Bind();
		vSprite.Draw(0);
	}
	//Reset state
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBlendEquation(GL_FUNC_ADD);

	//Boxes (without offsetX/offsetY - boxes should only be positioned by x,y)
	// Match the behavior of Draw() function which doesn't apply offset to boxes
	view = glm::mat4(1.f);
	view = glm::scale(view, glm::vec3(scale, scale, 1.f));
	view = glm::translate(view, glm::vec3(x,y,0.f));
	SetModelView(std::move(view));
	sSimple.Use();
	SetMatrix(lProjectionS);
	vGeometry.Bind();
	glUniform1f(lAlphaS, 0.6f);
	vGeometry.DrawQuads(GL_LINE_LOOP, quadsToDraw);
	glUniform1f(lAlphaS, 0.3f);
	vGeometry.DrawQuads(GL_TRIANGLE_FAN, quadsToDraw);

	// Re-enable depth write
	glDepthMask(GL_TRUE);
}

void Render::SetModelView(glm::mat4&& view_)
{
	view = std::move(view_);
}

void Render::SetMatrix(int lProjection)
{
	glUniformMatrix4fv(lProjection, 1, GL_FALSE, glm::value_ptr(projection*view));
}

void Render::UpdateProj(float w, float h)
{
	projection = glm::ortho<float>(0, w, h, 0, -1024.f, 1024.f);
}

void Render::SetCg(CG *cg_)
{
	cg = cg_;
	// Reset current image ID to force texture reload on next SwitchImage call
	curImageId = -1;
}

void Render::SetParts(Parts *parts)
{
	// Clear any existing texture when switching to/from Parts rendering
	// This prevents CG sprite textures from overlaying PAT textures
	if (m_parts != parts) {
		ClearTexture();
	}
	m_parts = parts;
}

void Render::SwitchImage(int id)
{
	// Always process when id == -1 to ensure texture clears, even if curImageId is already -1
	if(cg && (id != curImageId || id == -1) && cg->m_loaded)
	{
		curImageId = id;
		texture.Unapply();

		if(id>=0)
		{
			ImageData *image = cg->draw_texture(id, false, false);
			if(!image)
			{
				return;
			}

			texture.Load(image);

			// Validate image dimensions before applying to avoid GL_INVALID_VALUE
			if(texture.image->width > 0 && texture.image->height > 0)
			{
				texture.Apply(false, filter);
				AdjustImageQuad(texture.image->offsetX, texture.image->offsetY, texture.image->width, texture.image->height);
				vSprite.UpdateBuffer(0, imageVertex);
			}

			texture.Unload();
		}

	}
}

void Render::AdjustImageQuad(int x, int y, int w, int h)
{
	w+=x;
	h+=y;

	imageVertex[0] = imageVertex[16] = imageVertex[20] = x;
	imageVertex[4] = imageVertex[8] = imageVertex[12] = w;

	imageVertex[1] = imageVertex[5] = imageVertex[21] = y;
	imageVertex[9] = imageVertex[13] = imageVertex[17] = h;
}

void Render::GenerateHitboxVertices(const BoxList &hitboxes)
{
	int size = hitboxes.size();
	if(size <= 0)
	{
		quadsToDraw = 0;
		return;
	}
	static Hitbox **lastHitbox = 0;
	static int lastSize = 0;

	const float *color;
	//red, green, blue, z order
	constexpr float collisionColor[] 	{1, 1, 1, 1};
	constexpr float greenColor[] 		{0.2, 1, 0.2, 2};
	constexpr float shieldColor[] 		{0, 0, 1, 3}; //Not only for shield
	constexpr float clashColor[]		{1, 1, 0, 4};
	constexpr float projectileColor[] 	{0, 1, 1, 5}; //飛び道具
	constexpr float purple[] 			{0.5, 0, 1, 6}; //特別
	constexpr float redColor[] 			{1, 0.2, 0.2, 7};
	constexpr float hiLightColor[]		{1, 0.5, 1, 10};

	constexpr int tX[] = {0,1,1,0};
	constexpr int tY[] = {0,0,1,1};

	int floats = size*4*6; //4 Vertices with 6 attributes.
	if(clientQuads.size() < floats)
		clientQuads.resize(floats);
	
	int dataI = 0;
	for(const auto &boxPair : hitboxes)
	{
		int i = boxPair.first;
		const Hitbox& hitbox = boxPair.second;

		if (highLightN == i)
			color = hiLightColor;
		else if(i==0)
			color = collisionColor;
		else if (i >= 1 && i <= 8)
			color = greenColor;
		else if(i >=9 && i <= 10)
			color = shieldColor;
		else if(i == 11)
			color = clashColor;
		else if(i == 12)
			color = projectileColor;
		else if(i>12 && i<=24)
			color = purple;
		else
			color = redColor;

		
		for(int j = 0; j < 4*6; j+=6)
		{
			//X, Y, Z, R, G, B
			clientQuads[dataI+j+0] = hitbox.xy[0] + (hitbox.xy[2]-hitbox.xy[0])*tX[j/6];
			clientQuads[dataI+j+1] = hitbox.xy[1] + (hitbox.xy[3]-hitbox.xy[1])*tY[j/6];
			clientQuads[dataI+j+2] = color[3]+1000.f;
			clientQuads[dataI+j+3] = color[0];
			clientQuads[dataI+j+4] = color[1];
			clientQuads[dataI+j+5] = color[2];
		}
		dataI += 4*6;
	}
	quadsToDraw = size;
	vGeometry.UpdateBuffer(geoParts[BOXES], clientQuads.data(), dataI*sizeof(float));
}

void Render::DontDraw()
{
	quadsToDraw = 0;
}

void Render::ClearTexture()
{
	if (texture.isApplied) {
		texture.Unapply();
	}
	curImageId = -1;
}

void Render::SetImageColor(float *rgba)
{
	if(rgba)
	{
		for(int i = 0; i < 4; i++)
			colorRgba[i] = rgba[i];
	}
	else
	{
		for(int i = 0; i < 4; i++)
			colorRgba[i] = 1.f;
	}
}

void Render::SetBlendingMode()
{
	switch (blendingMode)
	{
	default:
	case normal:
		//glBlendEquation(GL_FUNC_ADD);
		//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		break;
	case additive:
		//glBlendEquation(GL_FUNC_ADD);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		break;
	case subtractive:
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
		break;
	}
}

// Multi-layer rendering support
void Render::ClearLayers()
{
	renderLayers.clear();
	currentLayerIndex = 0;
}

void Render::AddLayer(const RenderLayer& layer)
{
	renderLayers.push_back(layer);
}

void Render::SortLayersByZPriority(int mainPatternPriority)
{
	// Sort layers by Z-priority
	// Lower priority = draw first (behind)
	// Higher priority = draw last (in front)
	std::sort(renderLayers.begin(), renderLayers.end(),
		[mainPatternPriority](const RenderLayer& a, const RenderLayer& b) {
			// Calculate effective priorities based on game logic
			int priorityA = a.zPriority;
			int priorityB = b.zPriority;

			// Handle special projectile priority logic if needed
			// (For now using raw priority values)

			return priorityA < priorityB;  // Lower draws first
		});
}

void Render::DrawLayers()
{
	if (renderLayers.empty()) {
		return;
	}

	// Check if we should use Parts rendering
	if (usePat && m_parts && m_parts->loaded)
	{
		// Use Parts rendering with callbacks
		constexpr float tau = glm::pi<float>()*2.f;
		glm::mat4 baseView = glm::mat4(1.f);
		baseView = glm::scale(baseView, glm::vec3(scale, scale, 1.f));
		baseView = glm::translate(baseView, glm::vec3(x, y, 0.f));

		sPartShader.Use();
		
		// Disable vertex attribute array for color so glVertexAttrib4fv sets a constant value
		glDisableVertexAttribArray(2);

		// Callback to set matrix transform
		auto setMatrix = [this, &baseView, tau](glm::mat4 partMatrix) {
			glm::mat4 finalView = baseView * partMatrix;
			glUniformMatrix4fv(lProjectionParts, 1, GL_FALSE, glm::value_ptr(projection * finalView));
		};

		// Callback to set additive color
		auto setAddColor = [this](float r, float g, float b) {
			glUniform3f(lAddColorParts, r, g, b);
		};

		// Callback to set flip mode
		auto setFlip = [this](char flip) {
			glUniform1i(lFlipParts, (int)flip);
		};

		// Draw Parts with interpolation
		m_parts->Draw(curPattern, curNextPattern, curInterp, setMatrix, setAddColor, setFlip, colorRgba);

		return;
	}

	// Save original transform state
	int origX = x;
	int origY = y;
	int origOffsetX = offsetX;
	int origOffsetY = offsetY;
	float origScaleX = scaleX;
	float origScaleY = scaleY;
	float origRotX = rotX;
	float origRotY = rotY;
	float origRotZ = rotZ;
	bool origAFRT = AFRT;
	blendType origBlendMode = blendingMode;
	float origColorRgba[4] = {colorRgba[0], colorRgba[1], colorRgba[2], colorRgba[3]};

	// Save original CG
	CG* origCG = cg;

	// Draw each layer
	for (const auto& layer : renderLayers)
	{
		if (layer.spriteId < 0 || layer.alpha == 0.0f) continue;

		// Switch CG if this layer uses a different one (e.g., effect.ha6)
		if (layer.sourceCG && layer.sourceCG != cg) {
			SetCg(layer.sourceCG);
		}

		// Apply layer-specific transforms (spawn offset + frame offset)
		// TODO: Implement positioning flags:
		//   flagset1 & 0x10: Coordinates relative to camera
		//   flagset2 & 0x100: Position relative to opponent
		//   flagset2 & 0x200: Position relative to (-32768, 0)
		// For now, using basic relative positioning
		x = origX + layer.spawnOffsetX;
		y = origY + layer.spawnOffsetY;
		offsetX = layer.frameOffsetX;
		offsetY = layer.frameOffsetY;

		// Apply layer scale and rotation
		scaleX = layer.scaleX;
		scaleY = layer.scaleY;
		rotX = layer.rotX;
		rotY = layer.rotY;
		rotZ = layer.rotZ;
		AFRT = layer.AFRT;

		// Apply blend mode
		switch (layer.blendMode)
		{
		case 2:
			blendingMode = additive;
			break;
		case 3:
			blendingMode = subtractive;
			break;
		default:
			blendingMode = normal;
			break;
		}

		// Apply tint color and alpha
		colorRgba[0] = layer.tintColor.r * origColorRgba[0];
		colorRgba[1] = layer.tintColor.g * origColorRgba[1];
		colorRgba[2] = layer.tintColor.b * origColorRgba[2];
		colorRgba[3] = layer.alpha * origColorRgba[3];

		// Generate hitboxes for this layer
		GenerateHitboxVertices(layer.hitboxes);

		// Switch to this layer's sprite and draw sprite+boxes (no grid lines)
		SwitchImage(layer.spriteId);
		DrawSpriteOnly();
	}

	// Restore original CG
	if (cg != origCG) {
		SetCg(origCG);
	}

	// Restore original state
	x = origX;
	y = origY;
	offsetX = origOffsetX;
	offsetY = origOffsetY;
	scaleX = origScaleX;
	scaleY = origScaleY;
	rotX = origRotX;
	rotY = origRotY;
	rotZ = origRotZ;
	AFRT = origAFRT;
	blendingMode = origBlendMode;
	colorRgba[0] = origColorRgba[0];
	colorRgba[1] = origColorRgba[1];
	colorRgba[2] = origColorRgba[2];
	colorRgba[3] = origColorRgba[3];
}