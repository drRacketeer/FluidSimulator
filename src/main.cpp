#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include "Fluid.h"
#include <fstream>
#include <sstream>
#include <string>

string readShaderFile(const std::string& filepath) {
    ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "ERROR: Could not open shader file: " << filepath << std::endl;
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
    }

GLuint createShaderProgram(const char* vertexSource, const char* fragmentSource) {
    // ---------- Compile Vertex Shader ----------
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSource, nullptr);
    glCompileShader(vertexShader);

    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        std::cerr << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
        return 0; // Early exit
    }

    // ---------- Compile Fragment Shader ----------
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSource, nullptr);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
        std::cerr << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
        glDeleteShader(vertexShader); // Clean up before returning
        return 0;
    }

    // ---------- Link Shaders into a Program ----------
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "ERROR::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        glDeleteProgram(program);
        return 0;
    }

    // ---------- Cleanup: Shader objects are no longer needed ----------
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return program;
}
int main() {
    
    // Fluid variables
    double density = 1000.0;
    int numX = 20;
    int numY = 20;
    double h = 0.02;
    // Setting up a Scene struct for further experimentation
    struct Scene {
        double gravity = -9.81;
        double dt = 1.0 / 120.0;
        int numIters = 10;
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
        Fluid* fluid = nullptr;

        ~Scene(){ delete fluid; }

        void initFluid(double density, int numX, int numY, double h) {
            fluid = new Fluid(density, numX, numY, h);
        }

    };
    Scene scene;
    
    // Init Fluid Object
    scene.initFluid(density, numX, numY, h);
    // Fill s with 1.0 (fluid) and set boundaries to 0.0 (solid walls)
    for (int i = 0; i < scene.fluid->numX; i++) {
        for (int j = 0; j < scene.fluid->numY; j++) {
            scene.fluid->s[i * scene.fluid->numY + j] = 1.0;
        }
    }
    // Walls
    for (int i = 0; i < scene.fluid->numX; i++) {
        scene.fluid->s[i * scene.fluid->numY + 0] = 0.0;                // bottom
        scene.fluid->s[i * scene.fluid->numY + scene.fluid->numY - 1] = 0.0; // top
    }
    for (int j = 0; j < scene.fluid->numY; j++) {
        scene.fluid->s[0 * scene.fluid->numY + j] = 0.0;                // left
        scene.fluid->s[(scene.fluid->numX - 1) * scene.fluid->numY + j] = 0.0; // right
    }

    // STEP 1 Init OpenGL
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 800, "Fluid Simulation", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    
    
    // Load OpenGL functions via GLAD
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    // Check if GLAD loaded correctly
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }
    std::cout << "OpenGL version: " << glGetString(GL_VERSION) << std::endl;
    
    // Color for debugging
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    
    // Step 2 Create 2D Texture to hold the simulation data
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    // Set filtering and wrap modes
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Allocate storage for the texture on GPU (initially zero)
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, scene.fluid->numX, scene.fluid->numY, 0, GL_RED, GL_FLOAT, nullptr);
    // Step 3 Build
    string vertexSrc = readShaderFile("src/vertex.glsl");
    string fragmentSrc = readShaderFile("src/fragment.glsl");
    GLuint program = createShaderProgram(vertexSrc.c_str(), fragmentSrc.c_str());
    
    // Step 4 Setup full quad VAO
    float vertices[] = {
    // positions      // texCoords
    -1.0f,  1.0f,     0.0f, 1.0f,
    -1.0f, -1.0f,     0.0f, 0.0f,
     1.0f, -1.0f,     1.0f, 0.0f,

    -1.0f,  1.0f,     0.0f, 1.0f,
     1.0f, -1.0f,     1.0f, 0.0f,
     1.0f,  1.0f,     1.0f, 1.0f
    };

    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(double), (void*)0);
    glEnableVertexAttribArray(0);
    // Texture coordinate attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(double), (void*)(2 * sizeof(double)));
    glEnableVertexAttribArray(1);
    
    // Set a few cells near the bottom center to a high value (e.g., 2.0)
    int centerX = scene.fluid->numX / 2;
    int centerY = 5; // a few rows above bottom
    scene.fluid->m[centerX * scene.fluid->numY + centerY] = 2.0f;
    // Step 5 Main loop
    glUseProgram(program);
    glUniform1i(glGetUniformLocation(program, "fluidTexture"), 0);
    while (!glfwWindowShouldClose(window)) {
        // 1. Simulate one step
        scene.fluid->simulate(scene.dt, scene.gravity, scene.numIters);

        // Check if simulation is running
        float sum = 0.0f;
        for (int i = 0; i < scene.fluid->numCells; i++) sum += scene.fluid->m[i];
        std::cout << "Average m: " << sum / scene.fluid->numCells << std::endl;

        // 2. Upload the scalar field you want to visualize (e.g., smoke density 'm')
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, scene.fluid->numX, scene.fluid->numY, GL_RED, GL_FLOAT, scene.fluid->m.data());
        
        // 3. Render
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(program);
        glBindTexture(GL_TEXTURE_2D, texture);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // 4. Swap and poll
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    return 0;
}
