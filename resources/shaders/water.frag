#version 330 core
in vec3 worldPosition;
out vec4 FragColor;
uniform sampler2D terrainData;
uniform float terrainExtent, terrainResolution, seaLevel, time, waveStrength;
uniform vec3 viewPos, lightDir;
float hash(vec2 p) {return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
float noise(vec2 p) {
    vec2 i=floor(p), f=fract(p); f=f*f*(3-2*f);
    return mix(mix(hash(i),hash(i+vec2(1,0)),f.x),mix(hash(i+vec2(0,1)),hash(i+1),f.x),f.y);
}
void main() {
    vec2 p=worldPosition.xz;
    vec2 uv=((p/terrainExtent+.5)*(terrainResolution-1.0)+.5)/terrainResolution;
    float floorHeight=texture(terrainData,uv).r;
    float depth=max(0,seaLevel-floorHeight);
    if (floorHeight>worldPosition.y+.08) discard;
    float damp=smoothstep(0,3,depth)*waveStrength;
    vec2 slope=vec2(.028,.019)*cos(dot(p,vec2(.028,.019))+time*1.1)*.18
             +vec2(-.041,.023)*cos(dot(p,vec2(-.041,.023))-time*.8)*.10;
    float rippleFade=1-smoothstep(.3,2.0,max(length(dFdx(p)),length(dFdy(p))));
    // Short ripples perturb normals without requiring tiny geometry triangles.
    slope+=vec2(.11,.07)*cos(dot(p,vec2(.7,.4))+time*2.1)*.32*rippleFade;
    slope+=vec2(-.06,.14)*cos(dot(p,vec2(-.3,.8))-time*1.7)*.25*rippleFade;
    vec3 n=normalize(vec3(-slope.x*damp,1,-slope.y*damp));
    vec3 view=normalize(viewPos-worldPosition);
    float fresnel=.08+.62*pow(1-max(dot(n,view),0),5);
    vec3 water=mix(vec3(.13,.48,.44),vec3(.035,.16,.25),smoothstep(0,18,depth));
    water=mix(water,vec3(.60,.73,.79),fresnel);
    float spec=pow(max(dot(n,normalize(lightDir+view)),0),mix(40.0,180.0,rippleFade))*mix(.35,1.3,rippleFade);
    float breaking=sin(depth*5.0-time*1.7+noise(p*.18)*2)*.5+.5;
    float foam=(1-smoothstep(.1,2.8,depth))*smoothstep(.35,.78,breaking+noise(p*1.1)*.28);
    water=mix(water,vec3(.87,.94,.88),foam*.88)+vec3(1,.91,.72)*spec;
    float fog=smoothstep(600,2700,length(viewPos-worldPosition));
    water=mix(water,vec3(.60,.73,.79),fog);
    FragColor=vec4(water,mix(.6,1.0,smoothstep(0,6,depth)));
}
