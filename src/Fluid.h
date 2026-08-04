#pragma once
#include <vector>
using namespace std;

class Fluid {
    public:
    float density;
    int numX;
    int numY;
    int numCells;
    float h;
    vector<float> u, v, newU, newV, p, s, m, newM;
    float overRelaxation;

    Fluid(float density, int numX, int numY, float h);
    private:
    enum Field {
        U_FIELD,
        V_FIELD,
        S_FIELD
    };
    void integrate(float dt, float gravity);
    void solveIncompressibility(int numIters, float dt);
    void extrapolate();
    float sampleField(float x, float y, Field field);
    float avgU(int i, int j);
    float avgV(int i, int j);
    void advectVel(float dt);
    void advectSmoke(float dt);
    public:
    void simulate(float dt, float gravity, int numIters);
    
};
