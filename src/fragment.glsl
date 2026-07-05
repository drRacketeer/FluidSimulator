#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D fluidTexture;

void main() {
    // Sample the fluid density (stored in the red channel)
    float density = texture(fluidTexture, TexCoord).r;
    // Map density to a color (e.g., from blue to red)
    vec3 color = vec3(density, density * 0.5, 1.0 - density);
    FragColor = vec4(color, 1.0);
}
