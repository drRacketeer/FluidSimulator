#include <cmath>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include "Fluid.h"
#include <fstream>
#include <sstream>
#include <string>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"



// Setting up a Scene struct
struct Scene {
    // Fluid param
    float density = 1000.0f;
    // initial numX and numY without +2 ghostcells
    int numX = 100;
    int numY = 100;
    float h = 0.02f;

    float gravity = -9.81f;
    float dt = 1.0f / 60.0f;
    int numIters = 40;
    int frameNr = 0;
    float overRelaxation = 1.0f;
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
    GLuint uTexA, uTexB;
    GLuint vTexA, vTexB;
    GLuint pTexA, pTexB;
    GLuint solidTex;
    GLuint smokeTexA, smokeTexB;

    void setupScene(int sceneNr = 1){
        fluid = new Fluid(density, numX, numY, h);
        int n = fluid->numX;
        int &numX = fluid->numX;
        int &numY = fluid->numY;
        if (sceneNr == 0) {                       // tank
            for (int i = 0; i < numX; i++) {
                for (int j = 0; j < numY; j++) {
                    float s = 1.0f; // fluid
                    if (i == 0 || i == numX-1 || j == 0) {
                        fluid->s[j*n + i] = s;
                    }
                }
            }
        }
        else if (sceneNr == 1 || sceneNr == 3) {  // vortex shedding
            float inVel = 2.0f;
            for (int i = 0; i < numX; i++) {
                for (int j = 0; j < numY; j++) {
                    float s = 1.0f; //fluid
                    if (i == 0 || j == 0 || j == numY-1) {
                        s = 0.0f;
                    }
                    fluid->s[j*n + i] = s;
                    if (i == 1) {
                        fluid->u[j*n + i] = inVel;
                    }
                }
            }

            float pipeH = 0.1f * numY;
            int minJ = floor(0.5f * numY - 0.5f * pipeH);
            int maxJ = floor(0.5f * numY + 0.5f * pipeH);
            
            for (int j = minJ; j < maxJ; j++) {
                fluid->m[j * n + 0] = 0.0f;
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
            } else if (sceneNr == 2) { //paint

                gravity = 0.0f;
                overRelaxation = 1.0f;
                showPressure = false;
                showSmoke = true;
                showStreamlines = false;
                showVelocities = false;
                obstacleRadius = 0.1f;
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
            vx = (x - obstacleX) / dt;
            vy = (y - obstacleY) / dt;
        }
        obstacleX = x;
        obstacleY = y;
        float r = obstacleRadius;

        int n = fluid->numX;
        for (int i = 1; i < fluid->numX - 2; i++) {
            for (int j = 1; j < fluid->numY - 2; j++) {
            
                fluid->s[j*n + i] = 1.0f;

                float dx = (i + 0.5f) * fluid->h - x;
                float dy = (j + 0.5f) * fluid->h - y;

                if (dx * dx + dy * dy < r * r) {
                    fluid->s[j*n + i] = 0.0f;
                    if (sceneNr == 2) {
                        fluid->m[j*n + i] = 0.5f + 0.5f * sin(0.1f * frameNr);
                    } else {
                        fluid->m[j*n + i] = 1.0f;
                    }
                    fluid->u[j*n + i] = vx;
                    fluid->u[j*n + i + 1] = vx;
                    fluid->v[j*n + i] = vy;
                    fluid->v[(j + 1)*n + i] = vy;
                }
            }
        }
        showObstacle = true;
    }
};
Scene scene;
// NumX and NumY with the +2 ghost cells
int gridW = 0;
int gridH = 0;


void renderUi(ImGuiIO imGuiIo){
    // Set up the top bar
    ImGui::SetNextWindowPos(ImVec2(0, 0));                              // position at top-left corner
    ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, 40)); // full width, fixed height

    ImGui::Begin("TopBar", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar
    );

    // Make the background semi‑transparent (optional)
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.1f, 0.1f, 0.1f, 0.1f)); 
    
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::SameLine();
    if (ImGui::Button("Reset")) scene.setupScene(scene.sceneNr);
    ImGui::SameLine();
    ImGui::Checkbox("Show Streamlines", &scene.showStreamlines);
    ImGui::SameLine();
    ImGui::Checkbox("Show Velocities", &scene.showVelocities);
    ImGui::SameLine();
    ImGui::Checkbox("Show Pressure", &scene.showPressure);
    ImGui::SameLine();
    ImGui::Checkbox("Show Smoke", &scene.showSmoke);

    ImGui::PopStyleColor(); // restore default window bg color
    ImGui::End();
}
// bool for checking if mouse is pressed
bool mousePressed = false;
int fbWidth, fbHeight;
int winWidth, winHeight;


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

    // Cleanup: Shader objects are no longer needed
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return program;
}
GLuint createComputeProgram(const char* computeSource) {
    GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(shader, 1, &computeSource, nullptr);
    glCompileShader(shader);

    // Check compile errors
    int success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "ERROR::COMPUTE_SHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
        glDeleteShader(shader);
        return 0;
    }

    // Link program
    GLuint program = glCreateProgram();
    glAttachShader(program, shader);
    glLinkProgram(program);
    
    // Check link errors
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "ERROR::COMPUTE_PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
        glDeleteShader(shader);
        glDeleteProgram(program);
        return 0;
    }

    glDeleteShader(shader);
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
            float physX = (float)mx / winWidth * (scene->fluid->numX * scene->h);
            float physY = (float)(winHeight - my) / winHeight * (scene->fluid->numY * scene->h);
            scene->setObstacle(physX, physY, true);
            glBindTexture(GL_TEXTURE_2D, scene->solidTex);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, gridW, gridH, GL_RED, GL_FLOAT, scene->fluid->s.data());

            glBindTexture(GL_TEXTURE_2D, scene->uTexA);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, gridW, gridH, GL_RED, GL_FLOAT, scene->fluid->u.data());

            glBindTexture(GL_TEXTURE_2D, scene->vTexA);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, gridW, gridH, GL_RED, GL_FLOAT, scene->fluid->v.data());
        } 
        else if (action == GLFW_RELEASE) {
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
        glBindTexture(GL_TEXTURE_2D, scene->solidTex);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, gridW, gridH, GL_RED, GL_FLOAT, scene->fluid->s.data());
        glBindTexture(GL_TEXTURE_2D, scene->uTexA);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, gridW, gridH, GL_RED, GL_FLOAT, scene->fluid->u.data());
        glBindTexture(GL_TEXTURE_2D, scene->vTexA);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, gridW, gridH, GL_RED, GL_FLOAT, scene->fluid->v.data());
    }
}
int main() {
    
    scene.setupScene();
    gridW = scene.fluid->numX;
    gridH = scene.fluid->numY;

    // STEP 1 Init OpenGL
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(900, 900, "Fluid Simulation", nullptr, nullptr);
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
    // Creating RGBA Textures for the compute shader
    // This one is for the smoke
    glGenTextures(1, &scene.smokeTexA);
    glBindTexture(GL_TEXTURE_2D, scene.smokeTexA);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32F, gridW, gridH);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenTextures(1, &scene.smokeTexB);
    glBindTexture(GL_TEXTURE_2D, scene.smokeTexB);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32F, gridW, gridH);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // u textures
    glGenTextures(1, &scene.uTexA);
    glBindTexture(GL_TEXTURE_2D, scene.uTexA);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32F, gridW, gridH);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenTextures(1, &scene.uTexB);
    glBindTexture(GL_TEXTURE_2D, scene.uTexB);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32F, gridW, gridH);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // v textures
    glGenTextures(1, &scene.vTexA);
    glBindTexture(GL_TEXTURE_2D, scene.vTexA);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32F, gridW, gridH);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenTextures(1, &scene.vTexB);
    glBindTexture(GL_TEXTURE_2D, scene.vTexB);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32F, gridW, gridH);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // pressure textures
    glGenTextures(1, &scene.pTexA);
    glBindTexture(GL_TEXTURE_2D, scene.pTexA);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32F, gridW, gridH);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenTextures(1, &scene.pTexB);
    glBindTexture(GL_TEXTURE_2D, scene.pTexB);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32F, gridW, gridH);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // solid texture
    glGenTextures(1, &scene.solidTex);
    glBindTexture(GL_TEXTURE_2D, scene.solidTex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32F, gridW, gridH);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Clear garbage values just in case
    std::vector<float> zeros(scene.fluid->numCells, 0.0f);

    glBindTexture(GL_TEXTURE_2D, scene.pTexA);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, gridW, gridH, GL_RED, GL_FLOAT, zeros.data());
    glBindTexture(GL_TEXTURE_2D, scene.pTexB);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, gridW, gridH, GL_RED, GL_FLOAT, zeros.data());
    glBindTexture(GL_TEXTURE_2D, scene.smokeTexB);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, gridW, gridH, GL_RED, GL_FLOAT, zeros.data());
    glBindTexture(GL_TEXTURE_2D, scene.uTexB);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, gridW, gridH, GL_RED, GL_FLOAT, zeros.data());
    glBindTexture(GL_TEXTURE_2D, scene.vTexB);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, gridW, gridH, GL_RED, GL_FLOAT, zeros.data());

    // Upload textures
    glBindTexture(GL_TEXTURE_2D, scene.uTexA);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, gridW, gridH, GL_RED, GL_FLOAT, scene.fluid->u.data());

    glBindTexture(GL_TEXTURE_2D, scene.vTexA);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, gridW, gridH, GL_RED, GL_FLOAT, scene.fluid->v.data());

    glBindTexture(GL_TEXTURE_2D, scene.solidTex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, gridW, gridH, GL_RED, GL_FLOAT, scene.fluid->s.data());

    glBindTexture(GL_TEXTURE_2D, scene.smokeTexA);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, gridW, gridH, GL_RED, GL_FLOAT, scene.fluid->m.data());
    std::vector<float> check(gridW * gridH);
    glBindTexture(GL_TEXTURE_2D, scene.uTexA);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_FLOAT, check.data());

    int mismatches = 0;
    for (int i = 0; i < gridW * gridH; ++i)
        if (std::abs(check[i] - scene.fluid->u[i]) > 1e-6f) mismatches++;

    std::cout << "uTexA mismatches: " << mismatches << " / " << gridW*gridH << "\n";
    std::cout << "uTexA[1*numX + 50] = " << check[1 * gridW + 50] << "\n";
    std::cout << "uTexA[50*numX + 1] = " << check[50 * gridW + 1] << "\n";

    std::cout << "CPU u[1*numX + 50] = " << scene.fluid->u[1 * gridW + 50] << "\n";
    std::cout << "CPU u[50*numX + 1] = " << scene.fluid->u[50 * gridW + 1] << "\n";

    // Also count how many CPU u values are nonzero
    int nonzero = 0;
    for (int i = 0; i < gridW * gridH; ++i)
        if (std::abs(scene.fluid->u[i]) > 1e-6f) nonzero++;
    std::cout << "CPU u nonzero count: " << nonzero << "\n";
    /*
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
    */
    // Step 3 Build
    string vertexSrc = readShaderFile("src/vertex.glsl");
    string fragmentSrc = readShaderFile("src/fragment.glsl");
    string forcesComputeSrc = readShaderFile("src/forces.comp");
    string pressureComputeSrc = readShaderFile("src/pressure.comp");
    string projectComputeSrc = readShaderFile("src/project.comp");
    string advectVelComputeSrc = readShaderFile("src/advect_vel.comp");
    string advectSmokeComputeSrc= readShaderFile("src/advect_smoke.comp");

    GLuint program = createShaderProgram(vertexSrc.c_str(), fragmentSrc.c_str());
    GLuint forcesComputeProgram = createComputeProgram(forcesComputeSrc.c_str());
    GLuint pressureComputeProgram = createComputeProgram(pressureComputeSrc.c_str());
    GLuint projectComputeProgram = createComputeProgram(projectComputeSrc.c_str());
    GLuint advectVelComputeProgram = createComputeProgram(advectVelComputeSrc.c_str());
    GLuint advectSmokeComputeProgram = createComputeProgram(advectSmokeComputeSrc.c_str());

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


// ImGui Setup
IMGUI_CHECKVERSION();
ImGui::CreateContext();
ImGuiIO& imGuiIo = ImGui::GetIO(); (void)imGuiIo;
ImGui::StyleColorsDark();

// Init backends
ImGui_ImplGlfw_InitForOpenGL(window, true);
ImGui_ImplOpenGL3_Init("#version 430");

while (!glfwWindowShouldClose(window)) {
    // Viewport fix every frame
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    glViewport(0, 0, fbWidth, fbHeight);

    glfwPollEvents();

    // ImGui New Frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    // Step 1: Forces (gravity + vorticity confinement)
    glUseProgram(forcesComputeProgram);
    glUniform1f(glGetUniformLocation(forcesComputeProgram, "u_dt"), scene.dt);
    glUniform1f(glGetUniformLocation(forcesComputeProgram, "u_gravity"), scene.gravity);
    glUniform1f(glGetUniformLocation(forcesComputeProgram, "u_vorticityConfinement"), 0.1f);
    glUniform2f(glGetUniformLocation(forcesComputeProgram, "u_res"), (float)gridW, (float)gridH);
    glUniform1f(glGetUniformLocation(forcesComputeProgram, "u_h"), scene.fluid->h);

    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, scene.uTexA);
    glUniform1i(glGetUniformLocation(forcesComputeProgram, "u_sampler"), 0);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, scene.vTexA);
    glUniform1i(glGetUniformLocation(forcesComputeProgram, "v_sampler"), 1);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, scene.solidTex);
    glUniform1i(glGetUniformLocation(forcesComputeProgram, "solid_sampler"), 2);

    glBindImageTexture(0, scene.uTexB, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
    glBindImageTexture(1, scene.vTexB, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);

    glDispatchCompute((gridW-2 + 15)/16, (gridH-2 + 15)/16, 1);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    std::swap(scene.uTexA, scene.uTexB);
    std::swap(scene.vTexA, scene.vTexB);
    // Step 2: Pressure Solves (SOR iterations)
    for (int iter = 0; iter < scene.numIters; ++iter) {
        glUseProgram(pressureComputeProgram);
        glUniform1f(glGetUniformLocation(pressureComputeProgram, "u_h"), scene.fluid->h);
        glUniform1f(glGetUniformLocation(pressureComputeProgram, "u_density"), scene.density);
        glUniform1f(glGetUniformLocation(pressureComputeProgram, "u_dt"), scene.dt);
        glUniform1f(glGetUniformLocation(pressureComputeProgram, "u_overRelaxation"), scene.overRelaxation);
        glUniform2f(glGetUniformLocation(pressureComputeProgram, "u_res"), (float)gridW, (float)gridH);

        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, scene.uTexA);
        glUniform1i(glGetUniformLocation(pressureComputeProgram, "u_sampler"), 0);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, scene.vTexA);
        glUniform1i(glGetUniformLocation(pressureComputeProgram, "v_sampler"), 1);
        glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, scene.pTexA);
        glUniform1i(glGetUniformLocation(pressureComputeProgram, "p_sampler"), 2);
        glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, scene.solidTex);
        glUniform1i(glGetUniformLocation(pressureComputeProgram, "solid_sampler"), 3);

        glBindImageTexture(0, scene.pTexB, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);

        glDispatchCompute((gridW-2 + 15)/16, (gridH-2 + 15)/16, 1);
        glMemoryBarrier(GL_ALL_BARRIER_BITS);
        std::swap(scene.pTexA, scene.pTexB);
    }
    // Step 3: Subtract Pressure Gradient (project)
    glUseProgram(projectComputeProgram);
    glUniform1f(glGetUniformLocation(projectComputeProgram, "u_h"), scene.fluid->h);
    glUniform1f(glGetUniformLocation(projectComputeProgram, "u_density"), scene.density);
    glUniform1f(glGetUniformLocation(projectComputeProgram, "u_dt"), scene.dt);
    glUniform2f(glGetUniformLocation(projectComputeProgram, "u_res"), (float)gridW, (float)gridH);

    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, scene.uTexA);
    glUniform1i(glGetUniformLocation(projectComputeProgram, "u_sampler"), 0);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, scene.vTexA);
    glUniform1i(glGetUniformLocation(projectComputeProgram, "v_sampler"), 1);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, scene.pTexA);
    glUniform1i(glGetUniformLocation(projectComputeProgram, "p_sampler"), 2);
    glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, scene.solidTex);
    glUniform1i(glGetUniformLocation(projectComputeProgram, "solid_sampler"), 3);

    glBindImageTexture(0, scene.uTexB, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
    glBindImageTexture(1, scene.vTexB, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);

    glDispatchCompute((gridW-2 + 15)/16, (gridH-2 + 15)/16, 1);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    std::swap(scene.uTexA, scene.uTexB);
    std::swap(scene.vTexA, scene.vTexB);
    // Step 4: Advect Velocity
    glUseProgram(advectVelComputeProgram);
    glUniform1f(glGetUniformLocation(advectVelComputeProgram, "u_dt"), scene.dt);
    glUniform1f(glGetUniformLocation(advectVelComputeProgram, "u_h"), scene.fluid->h);
    glUniform2f(glGetUniformLocation(advectVelComputeProgram, "u_res"), (float)gridW, (float)gridH);

    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, scene.uTexA);
    glUniform1i(glGetUniformLocation(advectVelComputeProgram, "u_sampler"), 0);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, scene.vTexA);
    glUniform1i(glGetUniformLocation(advectVelComputeProgram, "v_sampler"), 1);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, scene.solidTex);
    glUniform1i(glGetUniformLocation(advectVelComputeProgram, "solid_sampler"), 2);

    glBindImageTexture(0, scene.uTexB, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
    glBindImageTexture(1, scene.vTexB, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);

    glDispatchCompute((gridW-2 + 15)/16, (gridH-2 + 15)/16, 1);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    std::swap(scene.uTexA, scene.uTexB);
    std::swap(scene.vTexA, scene.vTexB);

    // STEP 5: Advect Smoke
    glUseProgram(advectSmokeComputeProgram);
    glUniform1f(glGetUniformLocation(advectSmokeComputeProgram, "u_dt"), scene.dt);
    glUniform1f(glGetUniformLocation(advectSmokeComputeProgram, "u_h"), scene.fluid->h);
    glUniform2f(glGetUniformLocation(advectSmokeComputeProgram, "u_res"), (float)gridW, (float)gridH);

    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, scene.uTexA);
    glUniform1i(glGetUniformLocation(advectSmokeComputeProgram, "u_sampler"), 0);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, scene.vTexA);
    glUniform1i(glGetUniformLocation(advectSmokeComputeProgram, "v_sampler"), 1);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, scene.smokeTexA);
    glUniform1i(glGetUniformLocation(advectSmokeComputeProgram, "smoke_sampler"), 2);
    glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, scene.solidTex);
    glUniform1i(glGetUniformLocation(advectSmokeComputeProgram, "solid_sampler"), 3);

    glBindImageTexture(0, scene.smokeTexB, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);

    glDispatchCompute((gridW-2 + 15)/16, (gridH-2 + 15)/16, 1);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    std::swap(scene.smokeTexA, scene.smokeTexB);


    // Rendering
    glClear(GL_COLOR_BUFFER_BIT);
    // Use the rendering shader program
    glUseProgram(program);
    // Bind the GPU smoke texture instead of the old CPU texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, scene.smokeTexA);
    // (If you have uniforms for the texture, set them)
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    static int dbgFrame = 0;
    if (++dbgFrame == 60) {
        std::vector<float> buf(gridW * gridH);
        glBindTexture(GL_TEXTURE_2D, scene.smokeTexA);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_FLOAT, buf.data());

        float mn = 1e9f, mx = -1e9f, sum = 0;
        for (float v : buf) { mn = std::min(mn, v); mx = std::max(mx, v); sum += v; }
        std::cout << "m: min=" << mn << " max=" << mx
                << " mean=" << sum/(gridW*gridH) << "\n";

        // sample a few interior cells
        std::cout << "m[50][50]=" << buf[50*gridW + 50]
                << " m[10][80]=" << buf[80*gridW + 10]
                << " m[80][20]=" << buf[20*gridW + 80] << "\n";
    }
    // Old CPU bound simulation
    /*
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
    */
    // ImGui UI render
    renderUi(imGuiIo);

    // Render ImGui overlay
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    
    // 4. Swap and poll
    glfwSwapBuffers(window);
}

// ImGui cleanup
ImGui_ImplOpenGL3_Shutdown();
ImGui_ImplGlfw_Shutdown();
ImGui::DestroyContext();

return 0;
}
