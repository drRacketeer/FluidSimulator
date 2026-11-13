#pragma once
#include "FluidInterface.h"

class FluidGPU : public FluidInterface {
public:
    FluidGPU(float density, int numX, int numY, float h);
    ~FluidGPU() override;

    void setupScene(int sceneNr) override;
    void setObstacle(float x, float y, bool reset) override;
    void simulate() override;

    GLuint getSmokeTexture()    const override { return m_smokeTexA; }
    GLuint getPressureTexture() const override { return m_pTexA;    }
    GLuint getSolidTexture() const override { return m_solidTex; }

private:
    GLuint m_uTexA = 0, m_uTexB = 0;
    GLuint m_vTexA = 0, m_vTexB = 0;
    GLuint m_pTexA = 0, m_pTexB = 0;
    GLuint m_solidTex = 0;
    GLuint m_smokeTexA = 0, m_smokeTexB = 0;

    GLuint m_forcesProgram      = 0;
    GLuint m_pressureRBProgram  = 0;
    GLuint m_projectProgram     = 0;
    GLuint m_advectVelProgram   = 0;
    GLuint m_advectSmokeProgram = 0;
    GLuint m_obstacleVelProgram = 0;

    void initTextures();
    void initPrograms();
    void uploadAllTextures();
    void applyObstacleVelocity(float vx, float vy);
    void syncSolid();
    void syncAll();
};
