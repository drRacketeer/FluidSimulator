#include <vector>
using std::vector;

class Fluid {
public:
  double density;
  int numX;
  int numY;
  int numCells;
  double h;
  vector<float> u;
  vector<float> v;
  vector<float> newU;
  vector<float> newV;
  vector<float> p;
  vector<float> s;
  vector<float> m;
  vector<float> newM;

  Fluid(double density, int numX, int numY, double h) {
    this->density = density;
    this->numX = numX + 2;
    this->numY = numY + 2;
    this->numCells = this->numX * this->numY;
    this->h = h;
    this->v = vector<float>(this->numCells);
    this->u = vector<float>(this->numCells);
    this->newU = vector<float>(this->numCells);
    this->newV = vector<float>(this->numCells);
    this->p = vector<float>(this->numCells);
    this->s = vector<float>(this->numCells);
    this->m = vector<float>(this->numCells, 1.0f);
    this->newM = vector<float>(this->numCells);
  }
};
