#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D fluidTexture;

void main() {
    float density = texture(fluidTexture, TexCoord).r;
    // Simple heatmap: black -> blue -> cyan -> yellow -> white
    vec3 color = vec3(density);          // grayscale
    // Or use a custom palette function
    FragColor = vec4(color, 1.0);
}
