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
#include "enums.h"  // For RenderMode enum

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

	// Initialize LINES buffer with 8 vertices (4 lines):
	// - 2 grid lines (4 vertices)
	// - 2 origin marker lines (4 vertices, initially hidden at 0,0)
	float lines[]
	{
		-10000, 0, -1,	1,1,1,
		10000, 0, -1,	1,1,1,
		0, 10000, -1,	1,1,1,
		0, -10000, -1,	1,1,1,
		0, 0, -1,	1,1,1,
		0, 0, -1,	1,1,1,
		0, 0, -1,	1,1,1,
		0, 0, -1,	1,1,1,
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
	// Debug: Only print when renderMode changes
	static int lastRenderMode = -1;
	static int debugDrawCount = 0;
	if (debugDrawCount < 5 && m_parts && m_parts->renderMode && *m_parts->renderMode != lastRenderMode) {
		printf("[Render::Draw] usePat=%d, m_parts=%p, loaded=%d\n",
			usePat, (void*)m_parts, m_parts ? m_parts->loaded : -1);
		printf("  renderMode=%d (0=DEFAULT, 1=TEXTURE_VIEW, 2=UV_SETTING)\n",
			(int)*m_parts->renderMode);
		lastRenderMode = *m_parts->renderMode;
		debugDrawCount++;
	}

	if (usePat && m_parts && m_parts->loaded)
	{

		// Use Parts rendering with callbacks
		constexpr float tau = glm::pi<float>()*2.f;

		sPartShader.Use();

		// Disable vertex attribute array for color so glVertexAttrib4fv sets a constant value
		glDisableVertexAttribArray(2);

		// Callback to set matrix transform
		// Use perspective projection for PAT rendering (like sosfiro)
		auto setMatrix = [this](glm::mat4 partMatrix) {
			// Build the pre-transform matrix with screen-space transforms only
			glm::mat4 rview = projection;
			rview = glm::scale(rview, glm::vec3(scale, scale, 1.f));
			rview = glm::translate(rview, glm::vec3(x, y, 0));
			// Don't multiply by partMatrix here - pass it separately to SetMatrixPersp
			rview = glm::translate(rview, glm::vec3(0, 0, 1024.f));
			rview *= invOrtho;

			// Apply perspective projection - pass partMatrix separately (like sosfiro)
			SetMatrixPersp(lProjectionParts, partMatrix, rview);
		};

		// Callback to set additive color
		auto setAddColor = [this](float r, float g, float b) {
			glUniform3f(lAddColorParts, r, g, b);
		};

		// Callback to set flip mode
		auto setFlip = [this](char flip) {
			glUniform1i(lFlipParts, (int)flip);
		};

		// Draw Parts with interpolation (wrapped in try-catch for crash diagnosis)
		try {
			m_parts->Draw(curPattern, curNextPattern, curInterp, setMatrix, setAddColor, setFlip, colorRgba);
		} catch (const std::exception& e) {
			printf("[Render] EXCEPTION in Parts::Draw: %s\n", e.what());
			return;
		} catch (...) {
			printf("[Render] UNKNOWN EXCEPTION in Parts::Draw\n");
			return;
		}

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

void Render::SetMatrixPersp(int lProjection, glm::mat4 partView, glm::mat4 pre)
{
	// Apply perspective projection for PAT rendering (like sosfiro)
	glUniformMatrix4fv(lProjection, 1, GL_FALSE, glm::value_ptr(pre * perspective * partView));
}

void Render::UpdateProj(float w, float h)
{
	if(w == 0 || h == 0)
		return;

	projection = glm::ortho<float>(0, w, h, 0, -1024.f, 1024.f);
	invOrtho = glm::inverse(projection);

	// Setup perspective projection for PAT rendering (matches sosfiro's implementation)
	constexpr float dist = 1024;
	constexpr float dist2 = dist * 2;
	constexpr float f = 1.3;
	perspective = glm::translate(glm::mat4(1.f), glm::vec3(-1, 1, 0)) *
		glm::frustum<float>(-w/dist2, w/dist2, h/dist2, -h/dist2, 1*f, dist*2*f);
	perspective = glm::translate(perspective, glm::vec3(0.f, 0.f, -dist*f));
}

void Render::SetCg(CG *cg_)
{
	cg = cg_;
	// Reset current image ID to force texture reload on next SwitchImage call
	curImageId = -1;
}

void Render::SetParts(Parts *parts)
{
	// When switching between different Parts instances, clear CG sprite texture
	// This prevents CG sprite textures from overlaying PAT textures
	if (m_parts != parts) {
		// Only clear if switching to/from Parts rendering, or between different Parts
		if ((m_parts == nullptr) != (parts == nullptr)) {
			// Switching between PAT and non-PAT mode
			ClearTexture();
		} else if (m_parts != nullptr && parts != nullptr && m_parts != parts) {
			// Switching between different PAT files
			ClearTexture();
		}
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

bool Render::GeneratePartCenterVertices()
{
	// This function draws a colored plus sign at the origin (x, y) of the selected part
	// This helps visualize the transform origin when editing parts in the PatEditor

	// Safety: m_parts must be loaded to access part properties
	if (!m_parts || !m_parts->loaded || !m_parts->currState) {
		return false;
	}

	// Only draw in DEFAULT mode when not animating
	if (m_parts->currState->animating || m_parts->currState->renderMode != RenderMode::DEFAULT) {
		return false;
	}

	// Get the selected part property to draw its origin
	int partSetIndex = m_parts->currState->partSet;
	int partPropIndex = m_parts->currState->partProp;

	auto* partProp = m_parts->GetPartProp(partSetIndex, partPropIndex);
	if (!partProp) {
		return false;
	}

	// Draw a colored plus sign at the part's origin (x, y)
	// This helps visualize where transforms are applied from
	float propX = partProp->x;
	float propY = partProp->y;
	float lineSize = 50.0f;  // Length of the cross arms in pixels

	// Color for the origin marker (pink/magenta by default)
	float color[3] = {1.0f, 0.5f, 1.0f};

	// Lines buffer format: 4 grid lines + 4 origin marker lines
	// Each line: x1, y1, z, r, g, b, x2, y2, z, r, g, b (but stored as individual vertices)
	float lines[] = {
		// Grid lines (horizontal and vertical through 0,0)
		-10000, 0, -1,   1,1,1,
		10000, 0, -1,    1,1,1,
		0, 10000, -1,    1,1,1,
		0, -10000, -1,   1,1,1,

		// Origin marker (plus sign at part origin)
		propX - lineSize, propY, 1,    color[0], color[1], color[2],  // Left arm
		propX + lineSize, propY, 1,    color[0], color[1], color[2],  // Right arm
		propX, propY + lineSize, 1,    color[0], color[1], color[2],  // Top arm
		propX, propY - lineSize, 1,    color[0], color[1], color[2],  // Bottom arm
	};

	vGeometry.UpdateBuffer(geoParts[LINES], lines, sizeof(lines));
	return true;
}

bool Render::GenerateUVRectangleVertices()
{
	// This function draws a colored rectangle showing the UV bounds of the selected cutout
	// when in TEXTURE_VIEW or UV_SETTING_VIEW mode. This helps visualize which portion
	// of the texture the cutout uses.

	// Safety: m_parts must be loaded to access cutout properties
	if (!m_parts || !m_parts->loaded || !m_parts->currState) {
		return false;
	}

	// Only draw in TEXTURE_VIEW or UV_SETTING_VIEW mode when not animating
	if (m_parts->currState->animating || m_parts->currState->renderMode == RenderMode::DEFAULT) {
		return false;
	}

	// Get the selected cutout to draw its UV bounds
	int cutOutIndex = m_parts->currState->partCutOut;
	auto* cutOut = m_parts->GetCutOut(cutOutIndex);
	if (!cutOut) {
		return false;
	}

	// Get the texture to calculate pixel coordinates from UV coordinates
	auto* gfx = m_parts->GetPartGfx(cutOut->texture);
	if (!gfx) {
		return false;
	}

	// Calculate pixel coordinates from UV coordinates
	// UV coordinates are in texture space (0-255), we need to convert to pixel space
	float uvBppX = gfx->uvBpp[0];  // UV units per pixel X
	float uvBppY = gfx->uvBpp[1];  // UV units per pixel Y

	float x1 = cutOut->uv[0] / uvBppX;  // Left edge
	float y1 = cutOut->uv[1] / uvBppY;  // Top edge
	float x2 = (cutOut->uv[0] + cutOut->uv[2]) / uvBppX;  // Right edge
	float y2 = (cutOut->uv[1] + cutOut->uv[3]) / uvBppY;  // Bottom edge

	// Offset to center the rectangle on the texture
	float offsetX = -gfx->w / 2.0f;
	float offsetY = -gfx->h / 2.0f;

	x1 += offsetX;
	x2 += offsetX;
	y1 += offsetY;
	y2 += offsetY;

	// Color for the UV rectangle outline (cyan/blue)
	constexpr float uvRectColor[] = {0.0f, 1.0f, 1.0f};  // Cyan
	constexpr float zOrder = 100.0f;  // Draw on top

	// Triangle vertex indices for a quad
	constexpr int tX[] = {0,1,1,0};
	constexpr int tY[] = {0,0,1,1};

	// Generate quad vertices (4 vertices with 6 attributes each: X, Y, Z, R, G, B)
	float rectQuad[4*6];
	for(int j = 0; j < 4*6; j+=6)
	{
		rectQuad[j+0] = x1 + (x2-x1)*tX[j/6];  // X
		rectQuad[j+1] = y1 + (y2-y1)*tY[j/6];  // Y
		rectQuad[j+2] = zOrder;                 // Z (draw on top)
		rectQuad[j+3] = uvRectColor[0];        // R
		rectQuad[j+4] = uvRectColor[1];        // G
		rectQuad[j+5] = uvRectColor[2];        // B
	}

	quadsToDraw = 1;
	vGeometry.UpdateBuffer(geoParts[BOXES], rectQuad, sizeof(rectQuad));
	return true;
}

void Render::DontDraw()
{
	quadsToDraw = 0;
	// Reset lines buffer to show only grid (no part origin markers)
	float lines[]
	{
		-10000, 0, -1,	1,1,1,
		10000, 0, -1,	1,1,1,
		0, 10000, -1,	1,1,1,
		0, -10000, -1,	1,1,1,
		0, 0, -1,	1,1,1,
		0, 0, -1,	1,1,1,
		0, 0, -1,	1,1,1,
		0, 0, -1,	1,1,1,
	};
	vGeometry.UpdateBuffer(geoParts[LINES], lines, sizeof(lines));
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

	// Save original Parts pointer to restore later
	Parts* origParts = m_parts;

	// For multi-layer PAT rendering, we need to draw each layer separately
	// (can't return early like we did before)
	bool hasPatLayers = false;
	for (const auto& layer : renderLayers) {
		if (layer.usePat) {
			hasPatLayers = true;
			break;
		}
	}

	// If we have PAT layers, render them per-layer
	if (hasPatLayers)
	{
		constexpr float tau = glm::pi<float>()*2.f;

		// Ensure no shader is active before starting (prevents GL_INVALID_OPERATION when switching Parts)
		glUseProgram(0);

		// Save original render state
		int origX = x;
		int origY = y;
		int origOffsetX = offsetX;
		int origOffsetY = offsetY;

		for (const auto& layer : renderLayers)
		{
			if (!layer.usePat || layer.spriteId < 0 || layer.alpha == 0.0f) continue;

			// Determine which Parts to use for this layer
			Parts* layerParts = layer.sourceParts ? layer.sourceParts : m_parts;

			// Skip if Parts not loaded
			if (!layerParts || !layerParts->loaded) {
				continue;
			}

			// Switch Parts if needed (BEFORE starting shader)
			bool switchedParts = false;
			if (layerParts != m_parts) {
				SetParts(layerParts);
				switchedParts = true;

				// Copy state pointers from original Parts to layer Parts for TEXTURE_VIEW support
				// Effect Parts don't have their own views, so they inherit state from main character
				if (origParts && origParts->renderMode && origParts->currState) {
					m_parts->updatePatEditorReferences(origParts->currState, origParts->renderMode);
				}
			}

			// Start shader and setup for this layer
			sPartShader.Use();
			glDisableVertexAttribArray(2);

			// Apply layer-specific position offsets
			x = origX + layer.spawnOffsetX;
			y = origY + layer.spawnOffsetY;
			offsetX = layer.frameOffsetX;
			offsetY = layer.frameOffsetY;

			// Callback to set matrix transform with layer-specific transforms
			auto setMatrix = [this](glm::mat4 partMatrix) {
				glm::mat4 rview = projection;
				rview = glm::scale(rview, glm::vec3(scale, scale, 1.f));
				rview = glm::translate(rview, glm::vec3(x + offsetX, y + offsetY, 0));
				rview = glm::translate(rview, glm::vec3(0, 0, 1024.f));
				rview *= invOrtho;
				SetMatrixPersp(lProjectionParts, partMatrix, rview);
			};

			auto setAddColor = [this](float r, float g, float b) {
				glUniform3f(lAddColorParts, r, g, b);
			};

			auto setFlip = [this](char flip) {
				glUniform1i(lFlipParts, (int)flip);
			};

			// Render layer color
			float layerColor[4] = {
				layer.tintColor.r,
				layer.tintColor.g,
				layer.tintColor.b,
				layer.alpha
			};

			// Draw this layer's PAT
			m_parts->Draw(layer.spriteId, layer.spriteId, 0.0f, setMatrix, setAddColor, setFlip, layerColor);

			// Reset GL state after Parts::Draw() to prevent GL_INVALID_OPERATION
			glBindBuffer(GL_ARRAY_BUFFER, 0);  // Unbind VBO
			glBindTexture(GL_TEXTURE_2D, 0);    // Unbind texture
			glUseProgram(0);                    // Disable shader

			// Restore Parts if we switched
			if (switchedParts) {
				SetParts(origParts);
			}
		}

		// Restore render state
		x = origX;
		y = origY;
		offsetX = origOffsetX;
		offsetY = origOffsetY;
		// Don't return early - continue to CG layers section
	}

	// Force CG texture rebind after PAT rendering
	// PAT section unbinds GL textures, so we need to invalidate curImageId
	// to ensure SwitchImage() rebinds the CG texture even if sprite ID hasn't changed
	curImageId = -1;

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

	// Save original CG and Parts
	CG* origCG = cg;
	Parts* origCGParts = m_parts;

	// Draw each layer (CG sprites only - PAT layers already rendered above)
	for (const auto& layer : renderLayers)
	{
		if (layer.spriteId < 0 || layer.alpha == 0.0f) continue;

		// Skip PAT layers (already rendered in PAT section above)
		if (layer.usePat) continue;

		// Switch CG if this layer uses a different one (e.g., effect.ha6)
		if (layer.sourceCG && layer.sourceCG != cg) {
			SetCg(layer.sourceCG);
		}

		// Switch Parts if this layer uses different Parts (e.g., effect.pat)
		// This is needed even for CG layers in case they reference Parts data
		if (layer.sourceParts && layer.sourceParts != m_parts) {
			SetParts(layer.sourceParts);
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

	// Restore original CG and Parts
	if (cg != origCG) {
		SetCg(origCG);
	}
	if (m_parts != origCGParts) {
		SetParts(origCGParts);
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