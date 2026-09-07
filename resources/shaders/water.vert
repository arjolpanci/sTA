#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 view, projection;
uniform vec3 waterOrigin;
uniform sampler2D terrainData;
uniform float terrainExtent, terrainResolution, seaLevel, time, waveStrength;
out vec3 worldPosition;
void main() {
    vec3 position=aPos+waterOrigin;
    vec2 uv=((position.xz/terrainExtent+.5)*(terrainResolution-1.0)+.5)/terrainResolution;
    float depth=seaLevel-texture(terrainData,uv).r;
    float shore=smoothstep(0,4,depth);
    float wave=(sin(dot(position.xz,vec2(.028,.019))+time*1.1)*.18
               +sin(dot(position.xz,vec2(-.041,.023))-time*.8)*.10)*waveStrength*shore;
    worldPosition=vec3(position.x,seaLevel+wave,position.z);
    gl_Position=projection*view*vec4(worldPosition,1);
}
