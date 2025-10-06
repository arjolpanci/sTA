#version 330 core
out vec4 FragColor;
  
in vec3 vertexColor;
in vec2 TexCoord;

uniform sampler2D texture1;
uniform sampler2D texture2;


void main()
{
    //FragColor = texture(ourTexture, TexCoord) * vec4(vertexColor, 1.0);
    FragColor = mix(texture(texture1, TexCoord), texture(texture2, vec2(-TexCoord.x, TexCoord.y)), 0.2);
}