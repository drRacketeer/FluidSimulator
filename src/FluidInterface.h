#pragma once
#include <glad/glad.h>
#include <vector>

class FluidInterface {
public:
    // Grid data (shared, initialized in constructor)
    float density;
    int   numX, numY, numCells;
    float h;

    std::vector<float> u, v, newU, newV;
    std::vector<float> p;
    std::vector<float> s;
    std::vector<float> m, newM;

    // Config — ImGui writes here directly
    float gravity              = -9.81f;
    float dt                   = 1.0f / 60.0f;
    int   numIters             = 20;
    float overRelaxation       = 1.9f;
    float vorticityConfinement = 0.1f;
    float obstacleRadius       = 0.15f;

    // Runtime state
    int   sceneNr   = 1;
    int   frameNr   = 0;
    float obstacleX = 0.0f;
    float obstacleY = 0.0f;

    // Constructor / destructor
    FluidInterface(float density, int numX, int numY, float h);
    virtual ~FluidInterface() = default;

    // Pure virtual — implemented per backend
    virtual void setupScene(int sceneNr) = 0;
    virtual void setObstacle(float x, float y, bool reset) = 0;
    virtual void simulate() = 0;

    virtual GLuint getSmokeTexture()    const = 0;
    virtual GLuint getPressureTexture() const = 0;
    virtual GLuint getSolidTexture()    const = 0;

    // Concrete accessors (data is shared → no reason to duplicate)
    int   getNumX() const { return numX; }
    int   getNumY() const { return numY; }
    float getH()    const { return h;    }
};
