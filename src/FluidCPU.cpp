#include "FluidCPU.h"
#include <algorithm>
#include <cmath>
#include <vector>

// Constructor / destructor
FluidCPU::FluidCPU(float density, int numX, int numY, float h)
    : FluidInterface(density, numX, numY, h)
{
    auto mk = [&]() {
        GLuint t = 0;
        glGenTextures(1, &t);
        glBindTexture(GL_TEXTURE_2D, t);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32F, this->numX, this->numY);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        return t;
    };
    m_smokeTex    = mk();
    m_pressureTex = mk();
    m_solidTex    = mk();
}

FluidCPU::~FluidCPU() {
    glDeleteTextures(1, &m_smokeTex);
    glDeleteTextures(1, &m_pressureTex);
    glDeleteTextures(1, &m_solidTex);
}

// Scene setup — identical logic to FluidGPU, ends with uploadFields()
void FluidCPU::setupScene(int sceneNr) {
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

        if (sceneNr == 3) { dt = 1.0f / 120.0f; numIters = 40; }
    }
    else if (sceneNr == 2) {                                // paint
        gravity        = 0.0f;
        overRelaxation = 1.0f;
        obstacleRadius = 0.1f;
    }
    uploadFields();
}

void FluidCPU::setObstacle(float x, float y, bool reset) {
    float vx = 0.0f, vy = 0.0f;
    if (!reset) {
        vx = (x - obstacleX) / dt;
        vy = (y - obstacleY) / dt;
    }
    obstacleX = x;
    obstacleY = y;

    int   n = numX;
    float r = obstacleRadius;

    for (int i = 1; i < numX - 2; i++)
        for (int j = 1; j < numY - 2; j++) {
            s[j*n + i] = 1.0f;
            float dx = (i + 0.5f) * h - x;
            float dy = (j + 0.5f) * h - y;
            if (dx*dx + dy*dy < r*r) {
                s[j*n + i] = 0.0f;
                m[j*n + i] = (sceneNr == 2)
                    ? 0.5f + 0.5f * sinf(0.1f * frameNr)
                    : 1.0f;
                u[j*n + i]       = vx;
                u[j*n + (i + 1)] = vx;
                v[j*n + i]       = vy;
                v[(j + 1)*n + i] = vy;
            }
        }

    uploadFields();
}

void FluidCPU::simulate() {
    integrate();
    std::fill(p.begin(), p.end(), 0.0f);
    solveIncompressibility();
    extrapolate();
    advectVel();
    advectSmoke();
    frameNr++;
    uploadFields();
}

void FluidCPU::uploadFields() {
    glBindTexture(GL_TEXTURE_2D, m_smokeTex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, numX, numY,
                    GL_RED, GL_FLOAT, m.data());

    glBindTexture(GL_TEXTURE_2D, m_pressureTex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, numX, numY,
                    GL_RED, GL_FLOAT, p.data());

    glBindTexture(GL_TEXTURE_2D, m_solidTex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, numX, numY,
                    GL_RED, GL_FLOAT, s.data());
}

// Adds gravity to the v-component of cells whose current cell and the cell
// below are both fluid (CPU parity).
void FluidCPU::integrate() {
    int n = numX;
    for (int i = 1; i < numX; i++) {
        for (int j = 1; j < numY - 1; j++) {
            if (s[j*n + i] != 0.0f && s[(j-1)*n + i] != 0.0f) {
                v[j*n + i] += gravity * dt;
            }
        }
    }
}

// Gauss-Seidel SOR on the Poisson equation for pressure.
// Accumulates pressure and corrects velocity in place.
void FluidCPU::solveIncompressibility() {
    int   n  = numX;
    float cp = density * h / dt;

    for (int iter = 0; iter < numIters; iter++) {
        for (int i = 1; i < numX - 1; i++) {
            for (int j = 1; j < numY - 1; j++) {
                if (s[j*n + i] == 0.0f) continue;

                float sx0 = s[j*n + (i-1)];
                float sx1 = s[j*n + (i+1)];
                float sy0 = s[(j-1)*n + i];
                float sy1 = s[(j+1)*n + i];
                float sSum = sx0 + sx1 + sy0 + sy1;
                if (sSum == 0.0f) continue;

                float div = u[j*n + (i+1)] - u[j*n + i]
                          + v[(j+1)*n + i] - v[j*n + i];

                float pCorr = -div / sSum * overRelaxation;
                p[j*n + i] += cp * pCorr;

                u[j*n + i]       -= sx0 * pCorr;
                u[j*n + (i+1)]   += sx1 * pCorr;
                v[j*n + i]       -= sy0 * pCorr;
                v[(j+1)*n + i]   += sy1 * pCorr;
            }
        }
    }
}

// Copies velocity from the interior into the ghost ring.
void FluidCPU::extrapolate() {
    int n = numX;

    // u at top/bottom ghost rows
    for (int i = 0; i < numX; i++) {
        u[0*n + i]           = u[1*n + i];
        u[(numY-1)*n + i]    = u[(numY-2)*n + i];
    }
    // v at left/right ghost columns
    for (int j = 0; j < numY; j++) {
        v[j*n + 0]           = v[j*n + 1];
        v[j*n + (numX-1)]    = v[j*n + (numX-2)];
    }
}

// Bilinear interpolation with staggered offset (dx, dy).
// x,y are physical coordinates; dx,dy depend on which field is sampled.
float FluidCPU::sampleField(float x, float y, Field field) const {
    int   n  = numX;
    float h1 = 1.0f / h;
    float h2 = 0.5f * h;

    // Clamp to physical domain — same as CPU reference
    x = std::max(std::min(x, numX * h), h);
    y = std::max(std::min(y, numY * h), h);

    float dx = 0.0f, dy = 0.0f;
    const std::vector<float>* f = nullptr;
    switch (field) {
        case U_FIELD: f = &u; dy = h2; break;
        case V_FIELD: f = &v; dx = h2; break;
        case S_FIELD: f = &m; dx = h2; dy = h2; break;
    }

    float xc = (x - dx) * h1;
    float yc = (y - dy) * h1;

    int x0 = std::min((int)floor(xc), numX - 1);
    float tx = xc - (float)x0;
    int x1 = std::min(x0 + 1, numX - 1);

    int y0 = std::min((int)floor(yc), numY - 1);
    float ty = yc - (float)y0;
    int y1 = std::min(y0 + 1, numY - 1);

    float sx = 1.0f - tx;
    float sy = 1.0f - ty;

    return sx*sy * (*f)[y0*n + x0]
         + tx*sy * (*f)[y0*n + x1]
         + tx*ty * (*f)[y1*n + x1]
         + sx*ty * (*f)[y1*n + x0];
}

// Average horizontal velocity around cell (i,j) for use in v-advection.
float FluidCPU::avgU(int i, int j) const {
    int n = numX;
    return (u[(j-1)*n + i]         + u[j*n + i]
          + u[(j-1)*n + (i+1)]     + u[j*n + (i+1)]) * 0.25f;
}

// Average vertical velocity around cell (i,j) for use in u-advection.
float FluidCPU::avgV(int i, int j) const {
    int n = numX;
    return (v[j*n + (i-1)]         + v[j*n + i]
          + v[(j+1)*n + (i-1)]     + v[(j+1)*n + i]) * 0.25f;
}

// Semi-Lagrangian advection of u and v using the cell-centered velocity
// computed from the staggered neighbours.
void FluidCPU::advectVel() {
    newU = u;
    newV = v;

    int   n  = numX;
    float h2 = 0.5f * h;

    for (int i = 1; i < numX; i++) {
        for (int j = 1; j < numY; j++) {

            // u-component
            if (s[j*n + i] != 0.0f && s[j*n + (i-1)] != 0.0f && j < numY - 1) {
                float x = i * h;
                float y = j * h + h2;
                float uC = u[j*n + i];
                float vC = avgV(i, j);
                x -= dt * uC;
                y -= dt * vC;
                newU[j*n + i] = sampleField(x, y, U_FIELD);
            }

            // v-component
            if (s[j*n + i] != 0.0f && s[(j-1)*n + i] != 0.0f && i < numX - 1) {
                float x = i * h + h2;
                float y = j * h;
                float uC = avgU(i, j);
                float vC = v[j*n + i];
                x -= dt * uC;
                y -= dt * vC;
                newV[j*n + i] = sampleField(x, y, V_FIELD);
            }
        }
    }

    u = newU;
    v = newV;
}

// Semi-Lagrangian advection of the smoke density m.
void FluidCPU::advectSmoke() {
    newM = m;

    int   n  = numX;
    float h2 = 0.5f * h;

    for (int i = 1; i < numX - 1; i++) {
        for (int j = 1; j < numY - 1; j++) {
            if (s[j*n + i] != 0.0f) {
                float uC = (u[j*n + i]      + u[j*n + (i+1)])  * 0.5f;
                float vC = (v[j*n + i]      + v[(j+1)*n + i])  * 0.5f;
                float x  = i * h + h2 - dt * uC;
                float y  = j * h + h2 - dt * vC;
                newM[j*n + i] = sampleField(x, y, S_FIELD);
            }
        }
    }

    m = newM;
}
