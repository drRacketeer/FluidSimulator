#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include "Fluid.h"
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
    
    // Create the window
    GLFWwindow *window =
        glfwCreateWindow(800, 800, "OpenGL Test", nullptr, nullptr);
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

    // Set the viewport (the rendering area within the window)
    glViewport(0, 0, 800, 600);

    // Main rendering loop
    while (!glfwWindowShouldClose(window)) {
        // Set the color to use when clearing the screen
        glClearColor(0.1f, 0.2f, 0.4f,
                 1.0f); // A deep, pleasant navy blue [citation:3]

        // Clear the screen with the color we just set
        glClear(GL_COLOR_BUFFER_BIT); // GL_COLOR_BUFFER_BIT tells OpenGL to clear
                                  // the color buffer [citation:6][citation:7]

        // Swap the back buffer with the front buffer (show what we just drew)
        glfwSwapBuffers(window);

        // Poll for and process events (like closing the window)
        glfwPollEvents();
    }

    // Clean up and exit
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
