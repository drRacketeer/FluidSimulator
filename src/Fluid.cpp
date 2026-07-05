#include <algorithm>
#include <vector>
#include <cmath>
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

    Fluid(double density, int numX, int numY, double h) {
        this->density = density;
        this->numX = numX + 2;
        this->numY = numY + 2;
        this->numCells = this->numX * this->numY;
        this->h = h;
        this->v = vector<double>(this->numCells);
        this->u = vector<double>(this->numCells);
        this->newU = vector<double>(this->numCells);
        this->newV = vector<double>(this->numCells);
        this->p = vector<double>(this->numCells);
        this->s = vector<double>(this->numCells);
        this->m = vector<double>(this->numCells, 1.0f);
        this->newM = vector<double>(this->numCells);
    }
    private:
    enum Field {
        U_FIELD,
        V_FIELD,
        S_FIELD
    };

    void integrate(double dt, double gravity){
        int n = this->numY;
        double fspeed = gravity * dt;
        for (int i = 1; i < this->numX; i++) {
            for (int j = 1; j < this->numY - 1; j++) {
                if (this->s[i*n + j] != 0 && this->s[i*n + j-1] != 0) {
                    this->v[i*n +j] += fspeed;
                }
            }
        }
    }
    void solveIncompressibility(int numIters, double dt) {
        int n = this->numY;
        double cp = this->density * this->h / dt;
        
        for (int iter = 0; iter < numIters; iter++) {
            for (int i = 1; i < this->numX - 1; i++) {
                for (int j = 1; j < this->numY-1; j++) {
                    if (this->s[i*n+j] == 0.0) {
                        continue;
                    }
                    double s = this->s[i*n +j];
                    double sx0 = this->s[(i-1)*n + j];
                    double sx1 = this->s[(i+1)*n + j];

                    double sy0 = this->s[i*n + j - 1];
                    double sy1 = this->s[i*n + j + 1];
                    s = sx0 + sx1 + sy0 + sy1;
                    if (s == 0.0){
                        continue;
                    }
                    //this part in general is kinda weird maybe rewrite
                    double div = this->u[(i+1)*n + j] - this->u[(i*n + j)] +
                        this->v[i*n + j + 1] - this->v[i*n + j];
                    double p = -div / s;
                    // p *= scene.overRelaxation find how that function works then insert back
                    this->p[i*n + j] += cp * p;

                    this->u[i*n + j] -= sx0 * p;
                    this->u[(i+1)*n + j] += sx1 * p;
                    this->v[i*n + j] -= sy0 * p;
                    this->v[i*n + j + 1] += sy1 * p;
                }
            }
        }
    }
    void extrapolate() {
        int n = numY;
        for (int i = 0; i < this->numX; i++) {
            this->u[i*n + 0] = this->u[i*n + 1]; // why +0 ??
            this->u[i*n + this->numY - 1] = this->u[1*n + this->numY - 2];
        }
        for (int i = 0; i < this->numY; i++) {
            this->v[0*n + i] = this->v[1*n + i];
            this->v[(this->numX - 1)*n + i] = this->v[(this->numX - 2)*n + i];
        }
    }
    // add enum for field
    double sampleField(double x, double y, Field field) { 
        int n = this->numY;
        double h = this->h;
        double h1 = 1.0/h;
        double h2 = 0.5*h;
        
        x = max(min(x, this->numX * h), h);
        y = max(min(y, this->numY * h), h);

        double dx = 0.0;
        double dy = 0.0;

        vector<double> f;
        
        switch (field) {
            //maybe create copies of the vector
            case U_FIELD: f = this->u; dy = h2; break;
            case V_FIELD: f = this->v; dx = h2; break;
            case S_FIELD: f = this->m; dx = h2; dy = h2; break;
        }
        double x0 = min(floor((x-dx)*h1), this->numX-1.0);
        double tx = ((x-dx) - x0 * h) * h1;
        double x1 = min(x0 + 1.0, this->numX-1.0);
        
        double y0 = min(floor((y-dy)*h1), this->numY-1.0);
        double ty = ((y-dy) - y0 * h) * h1;
        double y1 = min(y0 + 1.0, this->numY-1.0);
        double sx = 1.0 - tx;
        double sy = 1.0 - ty;
        
        double val = sx*sy * f[x0*n +y0] +
            tx*sy * f[x1*n + y0] +
            tx*ty * f[x1*n + y1] +
            sx*ty * f[x0*n + y1];

        return val;
    }
    
    double avgU(int i, int j) {
        int n = this->numY;
        double u = (this->u[i*n + j-1] + this->u[i*n + j] +
                    this->u[(i+1)*n + j-1] + this->u[(i+1)*n + j]) * 0.25;
        return u;
    }

    double avgV(int i, int j) {
        int n = this->numY;
        double v = (this->v[(i-1)*n + j] + this->v[i*n + j] +
                    this->v[(i-1)*n + j+1] + this->v[i*n + j+1]) * 0.25;
        return v;
    }
    
    void advectVel(double dt) {
        this->newU = this->u;
        this->newV = this->v;

        int n = this->numY;
        double h = this->h;
        double h2 = 0.5 * h;

        for (int i = 1; i < this->numX; i++) {
            for (int j = 1; j < this->numY; j++) {
                
            //cnt++; seems to count iterations for debugging
            // u component
                if (this->s[i*n + j] != 0.0 && this->s[(i-1)*n + j] != 0.0 && j < this->numY - 1) {
                    double x = i*h;
                    double y = j*h + h2;
                    double u = this->u[i*n + j];
                    double v = this->avgV(i, j);
                    x = x - dt*u;
                    y = y - dt*v;
                    u = this->sampleField(x, y, U_FIELD);
                    this->newU[i*n + j] = u;
                }
                if (this->s[i*n + j] != 0.0 && this->s[i*n + j-1] != 0.0 && i < this->numX - 1) {
                    double x = i*h + h2;
                    double y = j*h;
                    double u = this->avgU(i, j);
                    double v = this->v[i*n + j];
                    x = x - dt*u;
                    y = y - dt*v;
                    v = this->sampleField(x, y, V_FIELD);
                    this->newV[i*n + j] = u;
                }
            }
        }
        // not 100% sure it works like that maybe wrong
        this->u = this->newU;
        this->v = this->newV;
    }
    void advectSmoke(double dt) {
        //maybe use a setter
        this->newM = this->m;

        double n = this->numY;
        double h = this->h;
        double h2 = 0.5 * h;

        for (int i = 1; i < this->numX - 1; i++) {
            for (int j = 1; j < this->numY - 1; j++) {
                if (this->s[i*n + j] != 0.0) {
                    double u = (this->u[i*n + j] + this->u[(i+1)*n + j]) * 0.5;
                    double v = (this->v[i*n + j] + this->v[i*n + j + 1]) * 0.5;
                    double x = i*h + h2 - dt*u;
                    double y = j*h + h2 - dt*v;

                    this->newM[i*n + j] = this->sampleField(x, y, S_FIELD);
                }
            }
        }
        this->m = this->newM;
    }
    public:
    void simulate(double dt, double gravity, int numIters) {
        integrate(dt, gravity);
        
        fill(p.begin(), p.end(), 0.0);
        solveIncompressibility(numIters, dt);

        extrapolate();
        advectVel(dt);
        advectSmoke(dt);
    }
};
