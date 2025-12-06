#pragma once

#include <vector>
#include <glm/glm.hpp>
#include "ShapeBase.h"

class Sphere : public ShapeBase
{
private:
    void makeTile(glm::vec3 topLeft, glm::vec3 topRight, glm::vec3 bottomLeft, glm::vec3 bottomRight,
                  glm::vec2 uvTL, glm::vec2 uvTR, glm::vec2 uvBL, glm::vec2 uvBR);
    void makeWedge(float currTheta, float nextTheta);
    void makeSphere();
    
    void setVertexData() override;

    float m_radius = 0.5;
};
