#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include "FluidInterface.h"
#include "FluidCPU.h"
#include "FluidGPU.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

// Scene
struct Scene {
    bool paused       = false;
    bool useGPU       = false;
    bool showSmoke    = true;
    bool showPressure = false;

    FluidInterface* fluid = nullptr;

    ~Scene() { delete fluid; }
};
Scene scene;

// Globals for mouse interaction
bool mousePressed = false;
int  fbWidth, fbHeight;
int  winWidth, winHeight;

// Shader helpers for the rendering pipeline (vertex + fragment)
// (compute shaders are compiled inside FluidGPU itself)
std::string readShaderFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "ERROR: Could not open shader file: " << filepath << "\n";
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

GLuint createShaderProgram(const char* vertexSource, const char* fragmentSource) {
    // Vertex shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSource, nullptr);
    glCompileShader(vertexShader);

    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        std::cerr << "ERROR::VERTEX::COMPILE_FAILED\n" << infoLog << "\n";
        return 0;
    }

    // Fragment shader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSource, nullptr);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
        std::cerr << "ERROR::FRAGMENT::COMPILE_FAILED\n" << infoLog << "\n";
        glDeleteShader(vertexShader);
        return 0;
    }

    // Link
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "ERROR::PROGRAM::LINK_FAILED\n" << infoLog << "\n";
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        glDeleteProgram(program);
        return 0;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return program;
}

// Mouse callbacks
void mouse_button_callback(GLFWwindow* window, int button, int action, [[maybe_unused]] int mods) {
    if (button != GLFW_MOUSE_BUTTON_LEFT) return;

    Scene* sc = (Scene*)glfwGetWindowUserPointer(window);
    if (action == GLFW_PRESS) {
        mousePressed = true;

        double mx, my;
        glfwGetCursorPos(window, &mx, &my);

        float physX = (float)mx / winWidth
                    * (sc->fluid->getNumX() * sc->fluid->getH());
        float physY = (float)(winHeight - my) / winHeight
                    * (sc->fluid->getNumY() * sc->fluid->getH());

        sc->fluid->setObstacle(physX, physY, true);
    } else if (action == GLFW_RELEASE) {
        mousePressed = false;
    }
}

void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos) {
    if (!mousePressed) return;

    Scene* sc = (Scene*)glfwGetWindowUserPointer(window);

    float physX = (float)xpos / winWidth
                * (sc->fluid->getNumX() * sc->fluid->getH());
    float physY = (float)(winHeight - ypos) / winHeight
                * (sc->fluid->getNumY() * sc->fluid->getH());

    sc->fluid->setObstacle(physX, physY, false);
}

// ImGui top bar
void renderUi() {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, 0));

    ImGui::Begin("TopBar", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize   |
        ImGuiWindowFlags_NoMove     |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.1f, 0.1f, 0.1f, 0.85f));

    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

    ImGui::SameLine();
    if (ImGui::Button("Reset")) {
        scene.fluid->setupScene(scene.fluid->sceneNr);
    }

    ImGui::SameLine();
    ImGui::Checkbox("Smoke",    &scene.showSmoke);

    ImGui::SameLine();
    ImGui::Checkbox("Pressure", &scene.showPressure);

    ImGui::SameLine();
    ImGui::Checkbox("Pause",    &scene.paused);

    // ---- Simulation params (edit the shared data directly) ----
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    ImGui::SliderFloat("dt", &scene.fluid->dt, 0.005f, 0.033f, "%.3f");

    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    ImGui::SliderInt("Iters", &scene.fluid->numIters, 5, 60);

    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    ImGui::SliderFloat("Gravity", &scene.fluid->gravity, -20.0f, 0.0f);

    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    ImGui::SliderFloat("OverRelax", &scene.fluid->overRelaxation, 1.0f, 1.95f);

    // Vorticity is only meaningful on the GPU backend, but harmless to expose
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    ImGui::SliderFloat("Vorticity", &scene.fluid->vorticityConfinement, 0.0f, 0.5f);

    ImGui::PopStyleColor();
    ImGui::End();
}

int main() {
    // Init GLFW + GL
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(900, 900, "Fluid Simulation", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        return -1;
    }
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        return -1;
    }

    std::cout << "OpenGL version: " << glGetString(GL_VERSION) << "\n";

    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    glViewport(0, 0, fbWidth, fbHeight);

    glfwGetWindowSize(window, &winWidth, &winHeight);
    std::cout << "Window size: " << winWidth << " x " << winHeight << "\n";

    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);  // white background

    // Create the fluid (GPU or CPU)
    if (scene.useGPU) {
        std::cout << "Using GPU backend\n";
        scene.fluid = new FluidGPU(1000.0f, 200, 200, 0.02f);
    } else {
        std::cout << "Using CPU backend\n";
        scene.fluid = new FluidCPU(1000.0f, 100, 100, 0.02f);
    }
    scene.fluid->setupScene(1);

    glfwSetWindowUserPointer(window, &scene);

    // Rendering pipeline (full-screen quad + fragment shader)
    std::string vertexSrc   = readShaderFile("src/vertex.glsl");
    std::string fragmentSrc = readShaderFile("src/fragment.glsl");
    GLuint program = createShaderProgram(vertexSrc.c_str(), fragmentSrc.c_str());
    if (!program) {
        std::cerr << "Failed to build rendering shader\n";
        return -1;
    }

    float vertices[] = {
        // positions        // texCoords
        -1.0f,  1.0f,   0.0f, 1.0f,
        -1.0f, -1.0f,   0.0f, 0.0f,
         1.0f, -1.0f,   1.0f, 0.0f,

        -1.0f,  1.0f,   0.0f, 1.0f,
         1.0f, -1.0f,   1.0f, 0.0f,
         1.0f,  1.0f,   1.0f, 1.0f
    };

    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glUseProgram(program);
    glUniform1i(glGetUniformLocation(program, "fluidTexture"), 0);
    glUniform1i(glGetUniformLocation(program, "solidTexture"), 1);

    // Mouse input
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_pos_callback);

    GLFWcursor* cursor = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
    (void)cursor;

    // ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 430");

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
        glViewport(0, 0, fbWidth, fbHeight);

        glfwPollEvents();

        // ImGui frame start
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Simulate
        if (!scene.paused) {
            scene.fluid->simulate();
        }

        // Render
        GLuint fluidTex = scene.showPressure
            ? scene.fluid->getPressureTexture()
            : scene.fluid->getSmokeTexture();

        // Pressure min/max (only needed when showPressure is on)
        static GLint minLoc = glGetUniformLocation(program, "u_minVal");
        static GLint maxLoc = glGetUniformLocation(program, "u_maxVal");
        static GLint modeLoc = glGetUniformLocation(program, "u_mode");

        if (scene.showPressure) {
            float minP =  1e9f, maxP = -1e9f;
            for (float v : scene.fluid->p) {
                minP = std::min(minP, v);
                maxP = std::max(maxP, v);
            }
            glUniform1f(minLoc, minP);
            glUniform1f(maxLoc, maxP);
        }
        glUniform1i(modeLoc, scene.showPressure ? 1 : 0);

        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(program);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, fluidTex);
        glUniform1i(glGetUniformLocation(program, "fluidTexture"), 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, scene.fluid->getSolidTexture());
        glUniform1i(glGetUniformLocation(program, "solidTexture"), 1);

        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // ImGui
        renderUi();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteProgram(program);
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);

    delete scene.fluid;
    scene.fluid = nullptr;

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
