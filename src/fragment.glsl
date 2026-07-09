#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D fluidTexture;

void main() {
    float density = texture(fluidTexture, TexCoord).r;
    vec3 color = vec3(density);
    FragColor = vec4(color, 1.0);
}
