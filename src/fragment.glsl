#version 430 core

uniform sampler2D fluidTexture;
uniform sampler2D solidTexture;
uniform int   u_mode;       // 0 = smoke, 1 = pressure
uniform float u_minVal;
uniform float u_maxVal;

in  vec2 TexCoord;
out vec4 FragColor;

vec3 sciColor(float val, float minVal, float maxVal) {
    val = clamp(val, minVal, maxVal - 0.0001);
    float d = maxVal - minVal;
    val = (d == 0.0) ? 0.5 : (val - minVal) / d;
    float m = 0.25;
    float num = floor(val / m);
    float s = (val - num * m) / m;
    vec3 color;
    if      (num == 0.0) color = vec3(0.0, s, 1.0);
    else if (num == 1.0) color = vec3(0.0, 1.0, 1.0 - s);
    else if (num == 2.0) color = vec3(s, 1.0, 0.0);
    else if (num == 3.0) color = vec3(1.0, 1.0 - s, 0.0);
    else                 color = vec3(1.0, 0.0, 0.0);
    return color;
}

void main() {
    float fluidVal = texture(fluidTexture, TexCoord).r;
    float solidVal = texture(solidTexture, TexCoord).r;

    vec3 color;
    if (solidVal < 0.5) {
        // Solid cell (obstacle or wall)
        color = vec3(0.8, 0.2, 0.2);
    } else if (u_mode == 1) {
        color = sciColor(fluidVal, u_minVal, u_maxVal);
    } else {
        color = vec3(fluidVal);             // grayscale smoke
    }
    FragColor = vec4(color, 1.0);
}
