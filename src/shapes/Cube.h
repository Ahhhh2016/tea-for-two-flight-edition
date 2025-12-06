#pragma once

#include <vector>
#include <glm/glm.hpp>
#include "ShapeBase.h"

class Cube : public ShapeBase
{
private:
    void makeTile(glm::vec3 topLeft, glm::vec3 topRight, glm::vec3 bottomLeft, glm::vec3 bottomRight,
                  glm::vec2 uvTL, glm::vec2 uvTR, glm::vec2 uvBL, glm::vec2 uvBR);
    void makeFace(glm::vec3 topLeft, glm::vec3 topRight, glm::vec3 bottomLeft, glm::vec3 bottomRight);

    void setVertexData() override;
};
