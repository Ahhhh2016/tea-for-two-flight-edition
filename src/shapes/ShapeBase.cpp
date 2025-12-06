#include "ShapeBase.h"

void ShapeBase::updateParams(int param1, int param2)
{
    m_vertexData = std::vector<float>();
    m_param1 = param1;
    m_param2 = param2;
    setVertexData();
}

// Inserts a glm::vec3 into a vector of floats.
void ShapeBase::insertVec3(std::vector<float> &data, const glm::vec3 &v)
{
    data.push_back(v.x);
    data.push_back(v.y);
    data.push_back(v.z);
}

void ShapeBase::insertVec2(std::vector<float> &data, const glm::vec2 &v)
{
    data.push_back(v.x);
    data.push_back(v.y);
}

