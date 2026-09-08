#version 330 core
// One oversized triangle covering the screen, built from gl_VertexID so no
// vertex buffer is needed. The direction each fragment looks in is recovered
// from the inverse view-projection.
uniform mat4 inverseViewProjection;

out vec3 vRay;

void main()
{
    vec2 corner = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2) * 2.0 - 1.0;
    vec4 near = inverseViewProjection * vec4(corner, -1.0, 1.0);
    vec4 far = inverseViewProjection * vec4(corner, 1.0, 1.0);
    vRay = far.xyz / far.w - near.xyz / near.w;
    gl_Position = vec4(corner, 1.0, 1.0);
}
