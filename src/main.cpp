#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include "Fluid.h"
using namespace std;

int main() {

struct Scene {
    double gravity = -9.81;
    double dt = 1.0 / 120.0;
    int numIters = 100;
    int frameNr = 0;
    double overRelaxation = 1.9;
    double obstacleX = 0.0;
    double obstacleY = 0.0;
    double obstacleRadius = 0.15;
    bool paused = false;
    int sceneNr = 0;
    bool showObstacle = false;
    bool showStreamlines = false;
    bool showVelocities = false;
    bool showPressure = false;
    bool showSmoke = true;
    Fluid fluid;
};


// Initialize GLFW (the window library)
if (!glfwInit()) {
    std::cerr << "Failed to initialize GLFW" << std::endl;
    return -1;
}

// Configure OpenGL version (3.3 is a good, widely-supported baseline)
glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // Required on macOS
#endif

// Create the window, or GLFWwindow* instead
GLFWwindow *window =glfwCreateWindow(800, 800, "Fluid Simulation", nullptr, nullptr);
if (!window) {
    std::cerr << "Failed to create GLFW window" << std::endl;
    glfwTerminate();
    return -1;
}

// Make the window's OpenGL context current
glfwMakeContextCurrent(window);

// Load OpenGL function pointers with GLAD
if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    std::cerr << "Failed to initialize GLAD" << std::endl;
    return -1;
}

// Vertices for a full-screen quad: position (x, y) and texture coordinates (u, v)
float vertices[] = {
    // positions          // texCoords
    1.0f,  1.0f,  0.0f, 1.0f,
    -1.0f, -1.0f,  0.0f, 0.0f,
    1.0f, -1.0f,  1.0f, 0.0f,

    -1.0f,  1.0f,  0.0f, 1.0f,
    1.0f, -1.0f,  1.0f, 0.0f,
    1.0f,  1.0f,  1.0f, 1.0f
};

// Generate and bind a Vertex Array Object (VAO) and Vertex Buffer Object (VBO)
unsigned int VAO, VBO;
glGenVertexArrays(1, &VAO);
glGenBuffers(1, &VBO);

glBindVertexArray(VAO);

glBindBuffer(GL_ARRAY_BUFFER, VBO);
glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

// Position attribute
glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
glEnableVertexAttribArray(0);
// Texture coordinate attribute
glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
glEnableVertexAttribArray(1);


// Create and configure the texture
unsigned int texture;
glGenTextures(1, &texture);
glBindTexture(GL_TEXTURE_2D, texture);

// Set texture parameters
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

// Allocate memory for the texture on the GPU
// For a density field, we only need one channel (GL_RED).
// If you want to store velocity, you could use GL_RG for two components.
glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, numX, numY, 0, GL_RED, GL_FLOAT, nullptr);

// Main loop
while (!glfwWindowShouldClose(window)) {
    // 1. Update the simulation
    f->simulate(dt, gravity, numIters);

    // 2. Upload the new data to the GPU texture
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, f->numX, f->numY, GL_RED, GL_FLOAT, f->m.data());

    // 3. Rendering
    glClear(GL_COLOR_BUFFER_BIT);

    // Use the shader program and bind the texture
    glUseProgram(shaderProgram);
    glBindTexture(GL_TEXTURE_2D, texture);

    // Draw the full-screen quad
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Swap buffers and poll events
    glfwSwapBuffers(window);
    glfwPollEvents();
}
return 0;
}
