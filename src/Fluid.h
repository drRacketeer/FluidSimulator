#pragma once
#include <vector>
using namespace std;

class Fluid {
    public:
    double density;
    int numX;
    int numY;
    int numCells;
    double h;
    vector<double> u;
    vector<double> v;
    vector<double> newU;
    vector<double> newV;
    vector<double> p;
    vector<double> s;
    vector<double> m;
    vector<double> newM;

    Fluid(double density, int numX, int numY, double h);
    private:
    enum Field {
        U_FIELD,
        V_FIELD,
        S_FIELD
    };
    void integrate(double dt, double gravity);
    void solveIncompressibility(int numIters, double dt);
    void extrapolate();
    double sampleField(double x, double y, Field field);
    double avgU(int i, int j);
    double avgV(int i, int j);
    void advectVel(double dt);
    void advectSmoke(double dt);
    public:
    void simulate(double dt, double gravity, int numIters);
    
};
