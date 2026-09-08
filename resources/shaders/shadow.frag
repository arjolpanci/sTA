#version 330 core
in vec2 vUV;

// depth-only pass: gl_FragDepth is written automatically from gl_Position,
// there is no color attachment to write to
uniform bool useTexture;
uniform sampler2D tex;
uniform float alphaCutoff; // 0 disables the cutout test

void main()
{
    // A cut-out surface has to be cut out here too, or a canopy of leaf cards
    // casts the shadow of the solid quads its leaves are painted on
    if (alphaCutoff > 0.0 && useTexture && texture(tex, vUV).a < alphaCutoff)
        discard;
}
