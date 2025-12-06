#pragma once

#include <vector>
#include <glm/glm.hpp>
#include "ShapeBase.h"

class Cone : public ShapeBase
{
private:
    

private:
    void makeCapSlice(float currentTheta, float nextTheta);
    void makeCapTile(float innerRadius, float outerRadius, float currentTheta, float nextTheta);
    void makeSlopeSlice(float currentTheta, float nextTheta);
    void makeSlopeTile(float yTop, float rTop, float yBottom, float rBottom, float currentTheta, float nextTheta);
    glm::vec3 calcNorm(glm::vec3 &pt);
    void makeWedge(float currentTheta, float nextTheta);
    void setVertexData() override;

    float m_radius = 0.5;
};
