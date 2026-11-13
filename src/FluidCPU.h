#pragma once
#include "FluidInterface.h"

class FluidCPU : public FluidInterface {
public:
    FluidCPU(float density, int numX, int numY, float h);
    ~FluidCPU() override;

    void setupScene(int sceneNr) override;
    void setObstacle(float x, float y, bool reset) override;
    void simulate() override;

    GLuint getSmokeTexture()    const override { return m_smokeTex;    }
    GLuint getPressureTexture() const override { return m_pressureTex; }
    GLuint getSolidTexture() const override { return m_solidTex; }

private:
    enum Field { U_FIELD, V_FIELD, S_FIELD };

    void integrate();
    void solveIncompressibility();
    void extrapolate();
    float sampleField(float x, float y, Field field) const;
    float avgU(int i, int j) const;
    float avgV(int i, int j) const;
    void advectVel();
    void advectSmoke();
    void uploadFields();

    GLuint m_smokeTex = 0;
    GLuint m_pressureTex = 0;
    GLuint m_solidTex    = 0;
};
