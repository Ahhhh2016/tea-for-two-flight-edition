#pragma once

#include <vector>
#include <glm/glm.hpp>
#include "ShapeBase.h"

class Cylinder : public ShapeBase
{
private:
    void makeCapTile(float y, bool isTop, float innerRadius, float outerRadius, float currentTheta, float nextTheta);
    void makeCapSlice(float currentTheta, float nextTheta, bool isTop);
    void makeSideTile(float yTop, float yBottom, float currentTheta, float nextTheta);
    void makeSideSlice(float currentTheta, float nextTheta);
    void makeWedge(float currentTheta, float nextTheta);
    float m_radius = 0.5;

    void setVertexData() override;
};
