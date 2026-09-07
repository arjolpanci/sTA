#version 330 core
in vec3 vNormal;
in vec2 vUV;
in vec3 vFragPos;
in vec4 vFragPosLightSpace;
out vec4 FragColor;
uniform sampler2D terrainData;
uniform sampler2D shadowMap;
uniform float terrainExtent, terrainResolution, seaLevel;
uniform vec3 viewPos, lightDir;
uniform bool shadowsEnabled;
float hash(vec2 p) {return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
float noise(vec2 p) {
    vec2 i=floor(p), f=fract(p); f=f*f*(3.0-2.0*f);
    return mix(mix(hash(i),hash(i+vec2(1,0)),f.x),mix(hash(i+vec2(0,1)),hash(i+vec2(1)),f.x),f.y);
}
void main() {
    vec3 n=normalize(vNormal);
    vec2 uv=((vFragPos.xz/terrainExtent+.5)*(terrainResolution-1.0)+.5)/terrainResolution;
    float road=texture(terrainData,uv).g;
    float variation=noise(vFragPos.xz*.12)*.16+noise(vFragPos.xz*1.8)*.07;
    vec3 grass=vec3(.22,.34,.17)+variation*vec3(.5,.6,.3);
    vec3 sand=vec3(.67,.60,.41)+variation*.4;
    vec3 rock=vec3(.39,.41,.38)+variation*.7;
    float stone=max(smoothstep(.18,.55,1.0-n.y),smoothstep(90,165,vFragPos.y));
    vec3 base=mix(grass,rock,stone);
    base=mix(sand,base,smoothstep(seaLevel+1.2,seaLevel+5.0,vFragPos.y));
    vec3 asphalt=vec3(.085,.095,.10)+noise(vFragPos.xz*3.0)*.025;
    base=mix(base,asphalt,smoothstep(.25,.8,road));
    float shadow=0;
    vec3 p=vFragPosLightSpace.xyz/vFragPosLightSpace.w*.5+.5;
    if(shadowsEnabled && p.z<1 && p.z>0 && all(greaterThan(p.xy,vec2(0))) && all(lessThan(p.xy,vec2(1)))) {
        vec2 texel=1.0/vec2(textureSize(shadowMap,0));
        float bias=max(.0025*(1-dot(n,lightDir)),.0006);
        for(int x=-1;x<=1;++x) for(int y=-1;y<=1;++y)
            shadow+=(p.z-bias>texture(shadowMap,p.xy+vec2(x,y)*texel).r)?1.0/9.0:0.0;
    }
    vec3 lit=base*(.42+(1-shadow)*max(dot(n,lightDir),0)*.65);
    float fog=smoothstep(600,2700,length(viewPos-vFragPos));
    FragColor=vec4(mix(lit,vec3(.60,.73,.79),fog),1);
}
