
#version 330 core
layout (location = 0) in vec3 Position;
layout (location = 1) in vec4 UV;
layout (location = 2) in vec4 Color;

out vec2 Frag_UV;
out vec4 Frag_Color;

uniform mat4 ProjMtx;

void main()
{
    Frag_UV = UV.st;
    Frag_Color = Color;
    gl_Position = ProjMtx * vec4(Position, 1);
}
