#include "FluidInterface.h"

FluidInterface::FluidInterface(float d, int nx, int ny, float hh)
    : density(d),
      numX(nx + 2), numY(ny + 2),
      numCells((nx + 2) * (ny + 2)),
      h(hh)
{
    u.assign(numCells, 0.0f);
    v.assign(numCells, 0.0f);
    newU.assign(numCells, 0.0f);
    newV.assign(numCells, 0.0f);
    p.assign(numCells, 0.0f);
    s.assign(numCells, 0.0f);
    m.assign(numCells, 1.0f);
    newM.assign(numCells, 0.0f);
}
