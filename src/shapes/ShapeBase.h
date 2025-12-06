#pragma once

#include <vector>
#include <glm/glm.hpp>
#include "ShapeInterface.h"

class ShapeBase : public ShapeInterface
{
public:
    ~ShapeBase() override = default;

    void updateParams(int param1, int param2) final;
    std::vector<float> generateShape() final { return m_vertexData; }

private:
    // private setVertexData() to be implemented by concrete shapes
    virtual void setVertexData() = 0;

protected:
    void insertVec3(std::vector<float> &data, const glm::vec3 &v);
    void insertVec2(std::vector<float> &data, const glm::vec2 &v);

    std::vector<float> m_vertexData;
    int m_param1 = 1;
    int m_param2 = 1;
};


