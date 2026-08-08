#include <cmath>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include "Fluid.h"
#include <fstream>
#include <sstream>
#include <string>
// Setting up a Scene struct
struct Scene {
    // Fluid param
    float density = 1000.0f;
    int numX = 100;
    int numY = 60;
    float h = 0.02f;

    float gravity = -9.81f;
    float dt = 1.0f / 60.0f;
    int numIters = 40;
    int frameNr = 0;
    float overRelaxation = 1.9f;
    float obstacleX = 0.0f;
    float obstacleY = 0.0f;
    float obstacleRadius = 0.15f;
    bool paused = false;
    int sceneNr = 1;
    bool showObstacle = false;
    bool showStreamlines = false;
    bool showVelocities = false;
    bool showPressure = false;
    bool showSmoke = true;
    Fluid* fluid = nullptr;

    void setupScene(int sceneNr = 1){
        fluid = new Fluid(density, numX, numY, h);
        int n = numY;
        if (sceneNr == 1 || sceneNr == 3) { // vortex shedding
            float inVel = 2.0f;
            for (int i = 0; i<fluid->numX; i++) {
                for (int j = 0; j < fluid->numY; j++) {
                    float s = 1.0f; //fluid
                    if (i == 0 || j == 0 || j == fluid->numY-1) {
                        s = 0.0f;
                    }
                    fluid->s[i*n + j] = s;
                    if (i == 1) {
                        fluid->u[i*n + j] = inVel;
                    }
                }
            }

            float pipeH = 0.1f * fluid->numY;
            int minJ = floor(0.5f * fluid->numY - 0.5f * pipeH);
            int maxJ = floor(0.5f * fluid->numY + 0.5f * pipeH);
            
            for (int j = minJ; j < maxJ; j++) {
                fluid->m[j] = 0.0f;
            }
            
            setObstacle(0.4, 0.5, true);

            gravity = 0.0f;
            showPressure = false;
            showSmoke = true;
            showStreamlines = false;
            showVelocities = false;

            if (sceneNr == 3) {
                dt = 1.0f / 120.0f;
                numIters = 100;
                showPressure = true;
            }
        }

    }

    ~Scene(){ delete fluid; }

    void initFluid(float density, int numX, int numY, float h) {
        fluid = new Fluid(density, numX, numY, h);
    }
    void simulateFluid(){
        fluid->simulate(dt, gravity, numIters);
    }
    void setObstacle(float x, float y, bool reset) {
        float vx = 0.0f;
        float vy = 0.0f;

        if (!reset) {
            vx = (x - this->obstacleX) / this->dt;
            vy = (y - this->obstacleY) / this->dt;
        }
        this->obstacleX = x;
        this->obstacleY = y;
        float r = this->obstacleRadius;

        int n = this->fluid->numY;
        for (int i = 1; i < this->fluid->numX - 2; i++) {
            for (int j = 1; j < this->fluid->numY - 2; j++) {
            
                this->fluid->s[i*n + j] = 1.0f;

                float dx = (i + 0.5f) * this->fluid->h - x;
                float dy = (j + 0.5f) * this->fluid->h - y;

                if (dx * dx + dy * dy < r * r) {
                    this->fluid->s[i*n + j] = 0.0f;
                    if (this->sceneNr == 2) {
                        this->fluid->m[i*n + j] = 0.5f + 0.5f * sin(0.1f * this->frameNr);
                    } else {
                        this->fluid->m[i*n + j] = 1.0f;
                    }
                    this->fluid->u[i*n + j] = vx;
                    this->fluid->u[(i+1)*n + j] = vx;
                    this->fluid->v[i*n + j] = vy;
                    this->fluid->v[i*n + j+1] = vy;
                }
            }
        }
        this->showObstacle = true;
    }
};

// bool for checking if mouse is pressed
bool mousePressed = false;
int fbWidth, fbHeight;
int winWidth, winHeight;

Scene scene;


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
    // Compile Vertex Shader
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

// Mouse button callback:
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        Scene* scene = (Scene*)glfwGetWindowUserPointer(window);
        if (action == GLFW_PRESS) {
            mousePressed = true;
            double mx, my;
            glfwGetCursorPos(window, &mx, &my);
            // convert and call setObstacle with reset=true
            float physX = (float)mx / winWidth * (scene->numX * scene->h);
            float physY = (float)(winHeight - my) / winHeight * (scene->numY * scene->h);
            scene->setObstacle(physX, physY, true);
        } else if (action == GLFW_RELEASE) {
            mousePressed = false;
        }
    }
}

// Cursor position callback (called every frame when mouse moves):
void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos) {
    if (mousePressed) {
        Scene* scene = (Scene*)glfwGetWindowUserPointer(window);
        float physX = (float)xpos / winWidth * (scene->numX * scene->h);
        float physY = (float)(winHeight - ypos) / winHeight * (scene->numY * scene->h);
        scene->setObstacle(physX, physY, false); // reset=false to compute velocity
    }
}
int main() {
    
    scene.setupScene();
    
    // Init Fluid Object
    // STEP 1 Init OpenGL
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(1200, 800, "Fluid Simulation", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    glfwSetWindowUserPointer(window, &scene);
    
    // Load GLAD and check if it was loaded correctly
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }
    std::cout << "OpenGL version: " << glGetString(GL_VERSION) << std::endl;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    // probably needs to be fbWidth - 1 etc.
    glViewport(0, 0, fbWidth, fbHeight);   
    std::cout << "Framebuffer size: " << fbWidth << " x " << fbHeight << std::endl;
    std::cout << "Window size: ";
    glfwGetWindowSize(window, &winWidth, &winHeight);
    std::cout << winWidth << " x " << winHeight << std::endl;
    
    // Color for debugging incase something happens with the view
    glClearColor(0.0f, 1.0f, 0.0f, 1.0f); // Bright green
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
    // numX and numY are also swapped to make it comform to the way it reads the fluid vectors
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, scene.fluid->numY, scene.fluid->numX, 0, GL_RED, GL_FLOAT, nullptr);
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
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // Texture coordinate attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    // Step 5 Main loop
    glUseProgram(program);
    glUniform1i(glGetUniformLocation(program, "fluidTexture"), 0);

    // Creating cursor for interaction and setting the callback function
    GLFWcursor* cursor = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_pos_callback);

    while (!glfwWindowShouldClose(window)) {
        // Viewport fix every frame
        glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
        glViewport(0, 0, fbWidth, fbHeight);

        // 1. Simulate one step
        scene.simulateFluid();
        // 2. Upload the scalar field you want to visualize (e.g., smoke density 'm')
        glBindTexture(GL_TEXTURE_2D, texture);
        // numX and numY are also swapped here to make it comform to the way it reads the fluid vectors
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, scene.fluid->numY, scene.fluid->numX, GL_RED, GL_FLOAT, scene.fluid->m.data());
        
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
