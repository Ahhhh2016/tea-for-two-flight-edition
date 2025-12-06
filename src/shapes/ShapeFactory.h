#pragma once

#include <memory>
#include "utils/scenedata.h"
#include "ShapeInterface.h"

class ShapeFactory
{
public:
    static std::unique_ptr<ShapeInterface> create(PrimitiveType type);
};


