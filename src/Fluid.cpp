#include "Fluid.h"
#include <algorithm>
#include <vector>
#include <cmath>
using namespace std;

Fluid::Fluid(float density, int numX, int numY, float h) {
    // Density : The physical density of the fluid in kg/m^3.
    this->density = density;
    // NumX : Number of grid columns, including the 2 ghost cells (one at each horizontal boundary),
    // so the actual fluid domain is numX - 2 cells wide
    this->numX = numX + 2;
    // NumY : Number of grid rows, including the 2 ghost cells (one at each vertical boundary),
    // the actual fluid domain is numY - 2 cells high.
    this->numY = numY + 2;
    // numCells : Total number of entries in each vector: numX * numY.
    this->numCells = this->numX * this->numY;
    // H : The grid spacing (cell width/height) in meters.
    this->h = h;
    // V : The horizontal (x‑direction) velocity component.
    this->v = vector<float>(this->numCells);
    // U : The vertical (y‑direction) velocity component.
    this->u = vector<float>(this->numCells);
    // newU, newV : Temporary backup buffers for the velocity fields.
    this->newU = vector<float>(this->numCells);
    this->newV = vector<float>(this->numCells);
    // P : The pressure field, defined at cell centers.
    this->p = vector<float>(this->numCells);
    // S : The solid‑fluid marker array.
    // s == 1.0 → Fluid cell (simulation is active).
    // s == 0.0 → Solid cell (wall or obstacle).
    this->s = vector<float>(this->numCells, 1.0f);
    // M : The scalar field that is visualized, this is the "smoke" (or dye) concentration.
    this->m = vector<float>(this->numCells, 0.0f);
    // newM : Temporary backup buffer for the smoke field.
    this->newM = vector<float>(this->numCells);
    // overRelaxation : SOR, speeds up pressure iteration in solveIncompressibility
    this->overRelaxation = 1.9f;
}


void Fluid::integrate(float dt, float gravity){
    int n = this->numY;
    float fspeed = gravity * dt;
    for (int i = 1; i < this->numX; i++) {
        for (int j = 1; j < this->numY - 1; j++) {
            if (this->s[i*n + j] != 0 && this->s[i*n + j-1] != 0) {
                this->v[i*n +j] += fspeed;
            }
        }
    }
}
void Fluid::solveIncompressibility(int numIters, float dt) {
    int n = this->numY;
    float cp = this->density * this->h / dt;
    
    for (int iter = 0; iter < numIters; iter++) {
        for (int i = 1; i < this->numX - 1; i++) {
            for (int j = 1; j < this->numY-1; j++) {
                if (this->s[i*n+j] == 0.0f) {
                    continue;
                }
                float s = this->s[i*n +j];
                float sx0 = this->s[(i-1)*n + j];
                float sx1 = this->s[(i+1)*n + j];

                float sy0 = this->s[i*n + j - 1];
                float sy1 = this->s[i*n + j + 1];
                s = sx0 + sx1 + sy0 + sy1;
                if (s == 0.0f){
                    continue;
                }
                float div = this->u[(i+1)*n + j] - this->u[(i*n + j)] +
                    this->v[i*n + j + 1] - this->v[i*n + j];
                float p = -div / s;
                p *= this->overRelaxation;
                this->p[i*n + j] += cp * p;
                this->u[i*n + j] -= sx0 * p;
                this->u[(i+1)*n + j] += sx1 * p;
                this->v[i*n + j] -= sy0 * p;
                this->v[i*n + j + 1] += sy1 * p;
            }
        }
    }
}
void Fluid::extrapolate() {
    int n = numY;
    for (int i = 0; i < this->numX; i++) {
        this->u[i*n + 0] = this->u[i*n + 1]; // why +0 ??
        this->u[i*n + this->numY - 1] = this->u[i*n + this->numY - 2];
    }
    for (int i = 0; i < this->numY; i++) {
        this->v[0*n + i] = this->v[1*n + i];
        this->v[(this->numX - 1)*n + i] = this->v[(this->numX - 2)*n + i];
    }
}
double Fluid::sampleField(float x, float y, Field field) { 
    int n = this->numY;
    float h = this->h;
    float h1 = 1.0f/h;
    float h2 = 0.5f*h;
    
    x = max(min(x, this->numX * h), h);
    y = max(min(y, this->numY * h), h);

    float dx = 0.0f;
    float dy = 0.0f;

    vector<float> f;
    
    switch (field) {
        case U_FIELD: f = this->u; dy = h2; break;
        case V_FIELD: f = this->v; dx = h2; break;
        case S_FIELD: f = this->m; dx = h2; dy = h2; break;
    }
    float x0 = min(floor((x-dx)*h1), (float)this->numX-1.0f);
    float tx = ((x-dx) - x0 * h) * h1;
    float x1 = min(x0 + 1.0f, (float)this->numX-1.0f);
    
    float y0 = min(floor((y-dy)*h1), (float)this->numY-1.0f);
    float ty = ((y-dy) - y0 * h) * h1;
    float y1 = min(y0 + 1.0f, (float)this->numY-1.0f);
    float sx = 1.0f - tx;
    float sy = 1.0f - ty;
    
    float val = sx*sy * f[x0*n +y0] +
        tx*sy * f[(int)x1*n + (int)y0] +
        tx*ty * f[(int)x1*n + (int)y1] +
        sx*ty * f[(int)x0*n + (int)y1];

    return val;
}

double Fluid::avgU(int i, int j) {
    int n = this->numY;
    float u = (this->u[i*n + j-1] + this->u[i*n + j] +
                this->u[(i+1)*n + j-1] + this->u[(i+1)*n + j]) * 0.25f;
    return u;
}

double Fluid::avgV(int i, int j) {
    int n = this->numY;
    float v = (this->v[(i-1)*n + j] + this->v[i*n + j] +
                this->v[(i-1)*n + j+1] + this->v[i*n + j+1]) * 0.25f;
    return v;
}

void Fluid::advectVel(float dt) {
    this->newU = this->u;
    this->newV = this->v;

    int n = this->numY;
    float h = this->h;
    float h2 = 0.5f * h;

    for (int i = 1; i < this->numX; i++) {
        for (int j = 1; j < this->numY; j++) {
            
        //cnt++; seems to count iterations for debugging
            // u component
            if (this->s[i*n + j] != 0.0f && this->s[(i-1)*n + j] != 0.0f && j < this->numY - 1) {
                // original
                float x = i*h;
                float y = j*h + h2;
                float u = this->u[i*n + j];
                float v = this->avgV(i, j);
                x = x - dt*u;
                y = y - dt*v;
                u = this->sampleField(x, y, U_FIELD);
                this->newU[i*n + j] = u;
            }
            // v component
            if (this->s[i*n + j] != 0.0f && this->s[i*n + j-1] != 0.0f && i < this->numX - 1) {
                // original
                float x = i*h + h2;
                float y = j*h;
                float u = this->avgU(i, j);
                float v = this->v[i*n + j];
                x = x - dt*u;
                y = y - dt*v;
                v = this->sampleField(x, y, V_FIELD);
                this->newV[i*n + j] = v;
            }
        }
    }
    // not 100% sure it works like that maybe wrong
    this->u = this->newU;
    this->v = this->newV;
}
void Fluid::advectSmoke(float dt) {
    //maybe use a setter
    this->newM = this->m;

    int n = this->numY;
    float h = this->h;
    float h2 = 0.5f * h;

    for (int i = 1; i < this->numX - 1; i++) {
        for (int j = 1; j < this->numY - 1; j++) {
            if (this->s[i*n + j] != 0.0f) {
                float u = (this->u[i*n + j] + this->u[(i+1)*n + j]) * 0.5f;
                float v = (this->v[i*n + j] + this->v[i*n + j + 1]) * 0.5f;
                // original
                float x = i*h + h2 - dt*u;
                float y = j*h + h2 - dt*v;
                // opengl transposed
                //double x = j*h + h2 - dt*u;   // horizontal uses j
                //double y = i*h + h2 - dt*v;   // vertical uses i
                this->newM[i*n + j] = this->sampleField(x, y, S_FIELD);
            }
        }
    }
    this->m = this->newM;
}
void Fluid::simulate(float dt, float gravity, int numIters) {
    integrate(dt, gravity);
    
    fill(p.begin(), p.end(), 0.0f);
    solveIncompressibility(numIters, dt);

    extrapolate();
    advectVel(dt);
    advectSmoke(dt);
}
