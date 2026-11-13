#include "FluidGPU.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <cmath>
// File-local shader helpers
static std::string readShaderFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "Could not open shader: " << path << "\n";
        return "";
    }
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static GLuint createComputeProgram(const char* src) {
    GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    int ok; char log[1024];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        glGetShaderInfoLog(shader, 1024, nullptr, log);
        std::cerr << "COMPUTE COMPILE FAILED:\n" << log << "\n";
        glDeleteShader(shader);
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, shader);
    glLinkProgram(program);

    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        glGetProgramInfoLog(program, 1024, nullptr, log);
        std::cerr << "COMPUTE LINK FAILED:\n" << log << "\n";
        glDeleteShader(shader);
        glDeleteProgram(program);
        return 0;
    }
    glDeleteShader(shader);
    return program;
}

// Constructor / destructor
FluidGPU::FluidGPU(float density, int numX, int numY, float h)
    : FluidInterface(density, numX, numY, h)
{
    initTextures();
    initPrograms();
    uploadAllTextures();
}

FluidGPU::~FluidGPU() {
    GLuint textures[] = {
        m_uTexA, m_uTexB,
        m_vTexA, m_vTexB,
        m_pTexA, m_pTexB,
        m_solidTex,
        m_smokeTexA, m_smokeTexB
    };
    glDeleteTextures(9, textures);

    if (m_forcesProgram)      glDeleteProgram(m_forcesProgram);
    if (m_pressureRBProgram)  glDeleteProgram(m_pressureRBProgram);
    if (m_projectProgram)     glDeleteProgram(m_projectProgram);
    if (m_advectVelProgram)   glDeleteProgram(m_advectVelProgram);
    if (m_advectSmokeProgram) glDeleteProgram(m_advectSmokeProgram);
    if (m_obstacleVelProgram) glDeleteProgram(m_obstacleVelProgram);
}

// GPU resource setup
void FluidGPU::initTextures() {
    int gw = numX, gh = numY;

    auto mk = [&]() {
        GLuint t = 0;
        glGenTextures(1, &t);
        glBindTexture(GL_TEXTURE_2D, t);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32F, gw, gh);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        return t;
    };

    m_uTexA     = mk();
    m_uTexB     = mk();
    m_vTexA     = mk();
    m_vTexB     = mk();
    m_pTexA     = mk();
    m_pTexB     = mk();
    m_solidTex  = mk();
    m_smokeTexA = mk();
    m_smokeTexB = mk();
}

void FluidGPU::initPrograms() {
    auto load = [](const char* path) {
        return createComputeProgram(readShaderFile(path).c_str());
    };
    m_forcesProgram      = load("src/forces.comp");
    m_pressureRBProgram  = load("src/pressure_rb.comp");
    m_projectProgram     = load("src/project.comp");
    m_advectVelProgram   = load("src/advect_vel.comp");
    m_advectSmokeProgram = load("src/advect_smoke.comp");
    m_obstacleVelProgram = load("src/obstacle_vel.comp");
}

// Texture uploads
void FluidGPU::uploadAllTextures() {
    int gw = numX, gh = numY;

    // Seed BOTH A and B so the ghost ring stays consistent across swaps.
    glBindTexture(GL_TEXTURE_2D, m_uTexA);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, gw, gh, GL_RED, GL_FLOAT, u.data());
    glBindTexture(GL_TEXTURE_2D, m_uTexB);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, gw, gh, GL_RED, GL_FLOAT, u.data());

    glBindTexture(GL_TEXTURE_2D, m_vTexA);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, gw, gh, GL_RED, GL_FLOAT, v.data());
    glBindTexture(GL_TEXTURE_2D, m_vTexB);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, gw, gh, GL_RED, GL_FLOAT, v.data());

    glBindTexture(GL_TEXTURE_2D, m_smokeTexA);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, gw, gh, GL_RED, GL_FLOAT, m.data());
    glBindTexture(GL_TEXTURE_2D, m_smokeTexB);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, gw, gh, GL_RED, GL_FLOAT, m.data());

    glBindTexture(GL_TEXTURE_2D, m_solidTex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, gw, gh, GL_RED, GL_FLOAT, s.data());

    std::vector<float> zeros(numCells, 0.0f);
    glBindTexture(GL_TEXTURE_2D, m_pTexA);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, gw, gh, GL_RED, GL_FLOAT, zeros.data());
    glBindTexture(GL_TEXTURE_2D, m_pTexB);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, gw, gh, GL_RED, GL_FLOAT, zeros.data());
}

void FluidGPU::syncSolid() {
    glBindTexture(GL_TEXTURE_2D, m_solidTex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, numX, numY,
                    GL_RED, GL_FLOAT, s.data());
}

void FluidGPU::syncAll() {
    syncSolid();

    glBindTexture(GL_TEXTURE_2D, m_uTexA);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, numX, numY,
                    GL_RED, GL_FLOAT, u.data());
    glBindTexture(GL_TEXTURE_2D, m_vTexA);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, numX, numY,
                    GL_RED, GL_FLOAT, v.data());
    glBindTexture(GL_TEXTURE_2D, m_smokeTexA);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, numX, numY,
                    GL_RED, GL_FLOAT, m.data());
}
void FluidGPU::applyObstacleVelocity(float vx, float vy) {
    int gw = numX, gh = numY;
    GLuint groupsX = (gw - 2 + 15) / 16;
    GLuint groupsY = (gh - 2 + 15) / 16;

    glUseProgram(m_obstacleVelProgram);
    glUniform1f(glGetUniformLocation(m_obstacleVelProgram, "u_obX"), obstacleX);
    glUniform1f(glGetUniformLocation(m_obstacleVelProgram, "u_obY"), obstacleY);
    glUniform1f(glGetUniformLocation(m_obstacleVelProgram, "u_obR"), obstacleRadius);
    glUniform1f(glGetUniformLocation(m_obstacleVelProgram, "u_obVx"), vx);
    glUniform1f(glGetUniformLocation(m_obstacleVelProgram, "u_obVy"), vy);
    glUniform1f(glGetUniformLocation(m_obstacleVelProgram, "u_h"), h);
    glUniform2f(glGetUniformLocation(m_obstacleVelProgram, "u_res"), (float)gw, (float)gh);

    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, m_uTexA);
    glUniform1i(glGetUniformLocation(m_obstacleVelProgram, "u_sampler"), 0);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, m_vTexA);
    glUniform1i(glGetUniformLocation(m_obstacleVelProgram, "v_sampler"), 1);

    glBindImageTexture(0, m_uTexB, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
    glBindImageTexture(1, m_vTexB, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);

    glDispatchCompute(groupsX, groupsY, 1);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    std::swap(m_uTexA, m_uTexB);
    std::swap(m_vTexA, m_vTexB);
}
// Scene setup — same logic as FluidCPU, but ends with syncStateToGPU()
void FluidGPU::setupScene(int sceneNr) {
    this->sceneNr = sceneNr;
    int n = numX;

    if (sceneNr == 0) {                                     // tank
        for (int i = 0; i < numX; i++)
            for (int j = 0; j < numY; j++) {
                float sv = 1.0f;
                if (i == 0 || i == numX - 1 || j == 0) sv = 0.0f;
                s[j*n + i] = sv;
            }
        gravity        = -9.81f;
        overRelaxation = 1.9f;
    }
    else if (sceneNr == 1 || sceneNr == 3) {                // vortex shedding
        float inVel = 2.0f;
        for (int i = 0; i < numX; i++)
            for (int j = 0; j < numY; j++) {
                float sv = 1.0f;
                if (i == 0 || j == 0 || j == numY - 1) sv = 0.0f;
                s[j*n + i] = sv;
                if (i == 1) u[j*n + i] = inVel;
            }

        float pipeH = 0.1f * numY;
        int minJ = (int)floor(0.5f * numY - 0.5f * pipeH);
        int maxJ = (int)floor(0.5f * numY + 0.5f * pipeH);
        for (int j = minJ; j < maxJ; j++) m[j*n + 0] = 0.0f;

        obstacleRadius = 0.15f;
        setObstacle(0.4f, 0.5f, true);

        gravity        = 0.0f;
        dt             = 1.0f / 60.0f;
        numIters       = 20;
        overRelaxation = 1.9f;

        if (sceneNr == 3) {
            dt       = 1.0f / 120.0f;
            numIters = 40;
        }
    }
    else if (sceneNr == 2) {                                // paint
        gravity        = 0.0f;
        overRelaxation = 1.0f;
        obstacleRadius = 0.1f;
    }

    syncAll();
}

void FluidGPU::setObstacle(float x, float y, bool reset) {
    float vx = 0.0f, vy = 0.0f;
    if (!reset) {
        vx = (x - obstacleX) / dt;
        vy = (y - obstacleY) / dt;
    }
    obstacleX = x;
    obstacleY = y;

    int   n = numX;
    float r = obstacleRadius;

    for (int i = 1; i < numX - 2; i++) {
        for (int j = 1; j < numY - 2; j++) {
            s[j*n + i] = 1.0f;

            float dx = (i + 0.5f) * h - x;
            float dy = (j + 0.5f) * h - y;

            if (dx*dx + dy*dy < r*r) {
                s[j*n + i] = 0.0f;
            }
        }
    }
    syncSolid();
    applyObstacleVelocity(vx, vy);

    syncSolid();
}

// Simulation — the 5-step compute pipeline
void FluidGPU::simulate() {
    int gw = numX, gh = numY;
    GLuint groupsX = (gw - 2 + 15) / 16;
    GLuint groupsY = (gh - 2 + 15) / 16;

    // STEP 1: Forces (gravity + vorticity confinement)
    glUseProgram(m_forcesProgram);
    glUniform1f(glGetUniformLocation(m_forcesProgram, "u_dt"), dt);
    glUniform1f(glGetUniformLocation(m_forcesProgram, "u_gravity"), gravity);
    glUniform1f(glGetUniformLocation(m_forcesProgram, "u_vorticityConfinement"), vorticityConfinement);
    glUniform2f(glGetUniformLocation(m_forcesProgram, "u_res"), (float)gw, (float)gh);
    glUniform1f(glGetUniformLocation(m_forcesProgram, "u_h"), h);

    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, m_uTexA);
    glUniform1i(glGetUniformLocation(m_forcesProgram, "u_sampler"), 0);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, m_vTexA);
    glUniform1i(glGetUniformLocation(m_forcesProgram, "v_sampler"), 1);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, m_solidTex);
    glUniform1i(glGetUniformLocation(m_forcesProgram, "solid_sampler"), 2);

    glBindImageTexture(0, m_uTexB, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
    glBindImageTexture(1, m_vTexB, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);

    glDispatchCompute(groupsX, groupsY, 1);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    std::swap(m_uTexA, m_uTexB);
    std::swap(m_vTexA, m_vTexB);

    // STEP 2: Pressure solve (Red-Black Gauss-Seidel)
    glUseProgram(m_pressureRBProgram);
    glUniform1f(glGetUniformLocation(m_pressureRBProgram, "u_h"), h);
    glUniform1f(glGetUniformLocation(m_pressureRBProgram, "u_density"), density);
    glUniform1f(glGetUniformLocation(m_pressureRBProgram, "u_dt"), dt);
    glUniform1f(glGetUniformLocation(m_pressureRBProgram, "u_overRelaxation"), overRelaxation);
    glUniform2f(glGetUniformLocation(m_pressureRBProgram, "u_res"), (float)gw, (float)gh);

    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, m_uTexA);
    glUniform1i(glGetUniformLocation(m_pressureRBProgram, "u_sampler"), 0);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, m_vTexA);
    glUniform1i(glGetUniformLocation(m_pressureRBProgram, "v_sampler"), 1);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, m_solidTex);
    glUniform1i(glGetUniformLocation(m_pressureRBProgram, "solid_sampler"), 2);

    // pTexA is read-write: RBGS updates it in place
    glBindImageTexture(0, m_pTexA, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F);

    for (int iter = 0; iter < numIters; ++iter) {
        // Red pass
        glUniform1i(glGetUniformLocation(m_pressureRBProgram, "u_pass"), 0);
        glDispatchCompute(groupsX, groupsY, 1);
        glMemoryBarrier(GL_ALL_BARRIER_BITS);

        // Black pass
        glUniform1i(glGetUniformLocation(m_pressureRBProgram, "u_pass"), 1);
        glDispatchCompute(groupsX, groupsY, 1);
        glMemoryBarrier(GL_ALL_BARRIER_BITS);
    }

    // STEP 3: Subtract pressure gradient
    glUseProgram(m_projectProgram);
    glUniform1f(glGetUniformLocation(m_projectProgram, "u_h"), h);
    glUniform1f(glGetUniformLocation(m_projectProgram, "u_density"), density);
    glUniform1f(glGetUniformLocation(m_projectProgram, "u_dt"), dt);
    glUniform2f(glGetUniformLocation(m_projectProgram, "u_res"), (float)gw, (float)gh);

    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, m_uTexA);
    glUniform1i(glGetUniformLocation(m_projectProgram, "u_sampler"), 0);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, m_vTexA);
    glUniform1i(glGetUniformLocation(m_projectProgram, "v_sampler"), 1);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, m_pTexA);
    glUniform1i(glGetUniformLocation(m_projectProgram, "p_sampler"), 2);
    glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, m_solidTex);
    glUniform1i(glGetUniformLocation(m_projectProgram, "solid_sampler"), 3);

    glBindImageTexture(0, m_uTexB, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
    glBindImageTexture(1, m_vTexB, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);

    glDispatchCompute(groupsX, groupsY, 1);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    std::swap(m_uTexA, m_uTexB);
    std::swap(m_vTexA, m_vTexB);

    // STEP 4: Advect velocity
    glUseProgram(m_advectVelProgram);
    glUniform1f(glGetUniformLocation(m_advectVelProgram, "u_dt"), dt);
    glUniform1f(glGetUniformLocation(m_advectVelProgram, "u_h"), h);
    glUniform2f(glGetUniformLocation(m_advectVelProgram, "u_res"), (float)gw, (float)gh);

    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, m_uTexA);
    glUniform1i(glGetUniformLocation(m_advectVelProgram, "u_sampler"), 0);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, m_vTexA);
    glUniform1i(glGetUniformLocation(m_advectVelProgram, "v_sampler"), 1);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, m_solidTex);
    glUniform1i(glGetUniformLocation(m_advectVelProgram, "solid_sampler"), 2);

    glBindImageTexture(0, m_uTexB, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
    glBindImageTexture(1, m_vTexB, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);

    glDispatchCompute(groupsX, groupsY, 1);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    std::swap(m_uTexA, m_uTexB);
    std::swap(m_vTexA, m_vTexB);

    // STEP 5: Advect smoke
    glUseProgram(m_advectSmokeProgram);
    glUniform1f(glGetUniformLocation(m_advectSmokeProgram, "u_dt"), dt);
    glUniform1f(glGetUniformLocation(m_advectSmokeProgram, "u_h"), h);
    glUniform2f(glGetUniformLocation(m_advectSmokeProgram, "u_res"), (float)gw, (float)gh);

    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, m_uTexA);
    glUniform1i(glGetUniformLocation(m_advectSmokeProgram, "u_sampler"), 0);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, m_vTexA);
    glUniform1i(glGetUniformLocation(m_advectSmokeProgram, "v_sampler"), 1);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, m_smokeTexA);
    glUniform1i(glGetUniformLocation(m_advectSmokeProgram, "smoke_sampler"), 2);
    glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, m_solidTex);
    glUniform1i(glGetUniformLocation(m_advectSmokeProgram, "solid_sampler"), 3);

    glBindImageTexture(0, m_smokeTexB, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);

    glDispatchCompute(groupsX, groupsY, 1);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    std::swap(m_smokeTexA, m_smokeTexB);

    // Advance frame counter
    frameNr++;
}
