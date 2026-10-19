
#version 330 core
uniform sampler2D Texture;
uniform sampler2D Palette;   // 256x1 RGBA, current CG palette
uniform int indexed;         // 0 = direct RGBA, 1 = indexed nearest, 2 = indexed + bilinear, 3 = MBTL zoom > 1 CG filter

in vec2 Frag_UV;
in vec4 Frag_Color;
out vec4 FragColor;

// Palette lookup for an 8bpp index texture (index stored as R/255).
vec4 palLookup(vec2 uv)
{
    float idx = texture(Texture, uv).r;
    return texture(Palette, vec2(idx * (255.0/256.0) + (0.5/256.0), 0.5));
}

void main()
{
    vec4 col;
    if (indexed == 0) {
        col = texture(Texture, Frag_UV.st);
    } else if (indexed == 1) {
        col = palLookup(Frag_UV.st);
    } else if (indexed == 3) {
        // MBTL TecCharaDrawBgColorFilter_V / ...SpecularFilter_V (gamedata/mbtl/Shader/sh_chara.txt
        // CharaDrawBgColorBaseFilterBokashi), picked by Han6Draw_SubmitCgSprite_ParentMtx 0x4A0C40 when the camera
        // zoom is > 1: a 3x3 point-sampled neighbourhood one texel apart (g_fStepTexture1DotSizeW/H = 1 / (rect w+1),
        // 1 / (rect h+1), and the CG texture is exactly (w+1) x (h+1)), palette colour weighted .04/.06/.04,
        // .06/.60/.06, .04/.06/.04, summed over the texels whose index is not 0, with no renormalisation; none -> 0.
        // Texels outside the image read as index 0 (the editor image's own border; the game's sampler wraps).
        ivec2 ts = textureSize(Texture, 0);
        ivec2 c = ivec2(floor(Frag_UV.st * vec2(ts)));
        col = vec4(0.0);
        for (int y = -1; y <= 1; y++) for (int x = -1; x <= 1; x++) {
            ivec2 t = c + ivec2(x, y);
            if (t.x < 0 || t.y < 0 || t.x >= ts.x || t.y >= ts.y) continue;
            float idx = texelFetch(Texture, t, 0).r;
            if (idx <= 0.0) continue;
            float w = (x == 0 && y == 0) ? 0.60 : ((x == 0 || y == 0) ? 0.06 : 0.04);
            col += texture(Palette, vec2(idx * (255.0/256.0) + (0.5/256.0), 0.5)) * w;
        }
    } else {
        // Manual bilinear AFTER palette lookup (filtering raw indices is garbage).
        vec2 ts = vec2(textureSize(Texture, 0));
        vec2 p = Frag_UV.st * ts - 0.5;
        vec2 f = fract(p);
        vec2 base = (floor(p) + 0.5) / ts;
        vec4 c00 = palLookup(base);
        vec4 c10 = palLookup(base + vec2(1.0, 0.0) / ts);
        vec4 c01 = palLookup(base + vec2(0.0, 1.0) / ts);
        vec4 c11 = palLookup(base + vec2(1.0, 1.0) / ts);
        col = mix(mix(c00, c10, f.x), mix(c01, c11, f.x), f.y);
    }

    FragColor = col * Frag_Color;
};
