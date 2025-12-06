#pragma once

#include <vector>

class ShapeInterface
{
public:
    virtual ~ShapeInterface() = default;

    // cube only uses param1
    virtual void updateParams(int param1, int param2) = 0;
    // Returns vertex data: [pos(3), normal(3), uv(2)] per-vertex
    virtual std::vector<float> generateShape() = 0;
};



