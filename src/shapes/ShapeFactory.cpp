#include "ShapeFactory.h"

#include <utility>
#include "Cube.h"
#include "Sphere.h"
#include "Cylinder.h"
#include "Cone.h"

std::unique_ptr<ShapeInterface> ShapeFactory::create(PrimitiveType type)
{
    switch (type) {
    case PrimitiveType::PRIMITIVE_CUBE:
        return std::make_unique<Cube>();
    case PrimitiveType::PRIMITIVE_SPHERE:
        return std::make_unique<Sphere>();
    case PrimitiveType::PRIMITIVE_CYLINDER:
        return std::make_unique<Cylinder>();
    case PrimitiveType::PRIMITIVE_CONE:
        return std::make_unique<Cone>();
    case PrimitiveType::PRIMITIVE_MESH:
    default:
        return nullptr;
    }
}


