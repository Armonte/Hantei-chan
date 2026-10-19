
#version 330 core
uniform sampler2D Texture;
uniform vec4 addColor;

in vec2 Frag_UV;
in vec4 Frag_Color;
out vec4 FragColor;

void main()
{
    vec4 col = texture(Texture, Frag_UV);
    col *= Frag_Color;  // diffuse
    col += addColor;    // specular (PRSP), alpha included
    FragColor = col;
}
