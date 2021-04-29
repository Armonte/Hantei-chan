#include "render.h"
#include "main.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

//Error
#include <windows.h>
#include <glad/glad.h>
#include <sstream>

#include "hitbox.h"


Render::Render():
cg(nullptr),
vSprite(Vao::F2F2, GL_DYNAMIC_DRAW),
vGeometry(Vao::F3F3, GL_STREAM_DRAW),
curImageId(-1),
imageVertex{
	256, 256, 	0, 0,
	512, 256,  	1, 0, 
	512, 512,  	1, 1, 

	512, 512, 	1, 1,
	256, 512,  	0, 1,
	256, 256,  	0, 0,
},
quadsToDraw(0),
x(0), offsetX(0),
y(0), offsetY(0)
{
	sSimple.BindAttrib("Position", 0);
	sSimple.BindAttrib("Color", 1);
	sSimple.LoadShader("src/simple.vert", "src/simple.frag");
	sSimple.Use();

	sTextured.BindAttrib("Position", 0);
	sTextured.BindAttrib("UV", 1);
	sTextured.LoadShader("src/textured.vert", "src/textured.frag");
	
	lAlphaS = sSimple.GetLoc("Alpha");
	lProjectionS = sSimple.GetLoc("ProjMtx");
	lProjectionT = sTextured.GetLoc("ProjMtx");

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
	geoParts[BOXES] = vGeometry.Prepare(sizeof(float)*6*4*128, nullptr);
	vGeometry.Load();
	vGeometry.InitQuads(geoParts[BOXES]);

	UpdateProj(clientRect.x, clientRect.y);

	glViewport(0, 0, clientRect.x, clientRect.y);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_DEPTH_TEST);
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
	SetModelView(
		glm::scale(
			glm::translate(glm::mat4(1), glm::vec3(x,y,0)),
			glm::vec3(scale, scale, 1.f)
		)
	);
	glUniform1f(lAlphaS, 0.55f);
	SetMatrix(lProjectionS);
	vGeometry.Bind();
	vGeometry.Draw(geoParts[LINES], 0, GL_LINES);

	SetModelView(
		glm::scale(
			glm::translate(glm::mat4(1), glm::vec3(x+offsetX,y+offsetY,0)),
			glm::vec3(scale, scale, 1.f)
		)
	);
 	sTextured.Use();
	SetMatrix(lProjectionT);
	if(texture.isApplied)
	{
		vSprite.Bind();
		vSprite.Draw(0);
	}


	sSimple.Use();
	vGeometry.Bind();
	//glUniform1f(lAlphaS, 0.6f);
	vGeometry.DrawQuads(GL_LINE_LOOP, quadsToDraw);
	glUniform1f(lAlphaS, 0.2f);
	vGeometry.DrawQuads(GL_TRIANGLE_FAN, quadsToDraw);
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
	projection = glm::ortho<float>(0, w, h, 0, -10.f, 10.f);
}

void Render::SetCg(CG *cg_)
{
	cg = cg_;
}

void Render::SwitchImage(int id)
{
	if(id != curImageId && cg->m_loaded)
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
			texture.Apply();
			
			AdjustImageQuad(texture.image->offsetX, texture.image->offsetY, texture.image->width, texture.image->height);
			vSprite.UpdateBuffer(0, imageVertex);
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

void Render::GenerateHitboxVertices(Hitbox **hitboxes, int size)
{
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
	constexpr float greenColor[] 		{0, 1, 0, 2};
	constexpr float redColor[] 			{1, 0, 0, 3};
	constexpr float clashColor[]		{1, 1, 0, 4};
	constexpr float shieldColor[] 		{0, 0, 1, 5};
	constexpr float purple[] 			{1, 0, 1, 6}; //特別
	constexpr float projectileColor[] 	{0, 1, 1, 7}; //飛び道具？

	constexpr int tX[] = {0,1,1,0};
	constexpr int tY[] = {0,0,1,1};

	int floats = size*4*6; //4 Vertices with 6 attributes.
	if(clientQuads.size() < floats)
		clientQuads.resize(floats);
	
	int dataI = 0;
	for(int i = 0; i < 33; i++)
	{
		if(!hitboxes[i])
			continue;

		if(i==0)
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
			clientQuads[dataI+j+0] = hitboxes[i]->x1 + (hitboxes[i]->x2-hitboxes[i]->x1)*tX[j/5];
			clientQuads[dataI+j+1] = hitboxes[i]->y1 + (hitboxes[i]->y2-hitboxes[i]->y1)*tY[j/5];
			clientQuads[dataI+j+2] = color[3];
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