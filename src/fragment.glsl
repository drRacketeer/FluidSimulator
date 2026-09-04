#version 430 core
out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D fluidTexture;

void main() {
    float density = texture(fluidTexture, vec2(TexCoord.y, TexCoord.x)).r;
    vec3 color = vec3(density);
    FragColor = vec4(color, 1.0);
}
