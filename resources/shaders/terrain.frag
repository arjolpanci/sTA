#version 330 core
in vec3 vNormal;
in vec2 vUV;
in vec3 vFragPos;
in vec4 vFragPosLightSpace;
out vec4 FragColor;
uniform sampler2D terrainData;
uniform sampler2D roadMask;
uniform sampler2DShadow shadowMap;
uniform float terrainExtent, terrainResolution, seaLevel;
uniform vec3 viewPos, lightDir, sunColor, ambientColor, fogColor;
uniform bool shadowsEnabled;
uniform mat4 lightSpaceMatrix;
uniform float shadowTexelWorld;
float hash(vec2 p) {return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
float noise(vec2 p) {
    vec2 i=floor(p), f=fract(p); f=f*f*(3.0-2.0*f);
    return mix(mix(hash(i),hash(i+vec2(1,0)),f.x),mix(hash(i+vec2(0,1)),hash(i+vec2(1)),f.x),f.y);
}
void main() {
    vec3 n=normalize(vNormal);
    vec2 uv=((vFragPos.xz/terrainExtent+.5)*(terrainResolution-1.0)+.5)/terrainResolution;
    float road=texture(roadMask,vFragPos.xz/terrainExtent+.5).r;
    float variation=noise(vFragPos.xz*.12)*.16+noise(vFragPos.xz*1.8)*.07;
    vec3 grass=vec3(.22,.34,.17)+variation*vec3(.5,.6,.3);
    vec3 sand=vec3(.67,.60,.41)+variation*.4;
    vec3 rock=vec3(.39,.41,.38)+variation*.7;
    float stone=max(smoothstep(.18,.55,1.0-n.y),smoothstep(270,420,vFragPos.y)*.4);
    vec3 base=mix(grass,rock,stone);
    base=mix(sand,base,smoothstep(seaLevel+1.2,seaLevel+5.0,vFragPos.y));
    vec3 asphalt=vec3(.085,.095,.10)+noise(vFragPos.xz*3.0)*.025;
    vec3 trail=vec3(.36,.28,.17)+variation*.3;
    base=mix(base,trail,smoothstep(.2,.48,road));
    base=mix(base,asphalt,smoothstep(.63,.85,road));
    float shadow=0;
    // Same normal-offset lookup as the lit pass, so terrain and the objects
    // standing on it agree about where a shadow starts.
    vec4 ls=lightSpaceMatrix*vec4(vFragPos+n*shadowTexelWorld*(1.2+2.6*clamp(1-dot(n,lightDir),0,1)),1);
    vec3 p=ls.xyz/ls.w*.5+.5;
    if(shadowsEnabled && p.z<1 && p.z>0 && all(greaterThan(p.xy,vec2(0))) && all(lessThan(p.xy,vec2(1)))) {
        vec2 texel=1.0/vec2(textureSize(shadowMap,0));
        float angle=fract(sin(dot(gl_FragCoord.xy,vec2(12.9898,78.233)))*43758.5453)*6.2831853;
        mat2 rot=mat2(cos(angle),-sin(angle),sin(angle),cos(angle));
        float lit=0;
        for(int x=-1;x<=2;++x) for(int y=-1;y<=2;++y)
            lit+=texture(shadowMap,vec3(p.xy+rot*(vec2(x,y)-.5)*texel,p.z-.00045));
        shadow=1-lit/16.0;
    }
    vec3 lit=base*(ambientColor+(1-shadow)*max(dot(n,lightDir),0)*sunColor);
    float fog=smoothstep(600,2700,length(viewPos-vFragPos));
    FragColor=vec4(mix(lit,fogColor,fog),1);
}
