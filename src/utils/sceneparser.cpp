#include "sceneparser.h"
#include "scenefilereader.h"
#include <glm/gtx/transform.hpp>

#include <chrono>
#include <iostream>

bool isIdentityOrNullMatrix(glm::mat4 ctm) {
    bool isIdentityMatrix = true;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (i == j && ctm[i][j] != 1.0f) {
                isIdentityMatrix = false;
                break;
            } else if (i != j && ctm[i][j] != 0.0f) {
                isIdentityMatrix = false;
                break;
            }
        }
    }

    bool isNullMatrix = true;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (ctm[i][j] != 0.0f) {
                isNullMatrix = false;
                break;
            }
        }
    }
    return isIdentityMatrix || isNullMatrix;
}

void SceneParser::dfsSceneTree(SceneNode *node, RenderData *renderData, glm::mat4 ctm, int& shapeIdCounter) {
    if (node == nullptr) return;
    if (node->transformations.size() != 0)
    {
        for (int i = 0; i < node->transformations.size(); i++)
        {
            // Apply transformations in the order they appear in the scene file
            if (node->transformations[i]->type == TransformationType::TRANSFORMATION_TRANSLATE)
            {
                ctm = glm::translate(ctm, node->transformations[i]->translate);
            }
            else if (node->transformations[i]->type == TransformationType::TRANSFORMATION_ROTATE)
            {
                ctm = glm::rotate(ctm, node->transformations[i]->angle, node->transformations[i]->rotate);
            }
            else if (node->transformations[i]->type == TransformationType::TRANSFORMATION_SCALE)
            {
                ctm = glm::scale(ctm, node->transformations[i]->scale);
            }
            else if (node->transformations[i]->type == TransformationType::TRANSFORMATION_MATRIX && !isIdentityOrNullMatrix(node->transformations[i]->matrix))
            {
                ctm = node->transformations[i]->matrix * ctm;
            }
        }
    }
    if (node->primitives.size() != 0)
    {
        for (int i = 0; i < node->primitives.size(); i++)
        {
            RenderShapeData primitiveShape;
            primitiveShape.primitive = *node->primitives[i];
            primitiveShape.ctm = ctm;

            // for (int i = 0; i < 4; i++) {
            //     for (int j = 0; j < 4; j++) {
            //         std::cout << primitiveShape.ctm[i][j] << " ";
            //     }
            //     std::cout << std::endl;
            // }

            renderData->shapes.push_back(primitiveShape);
        }
    }

    if (!node->lights.empty())
    {
        for (int i = 0; i < node->lights.size(); i++)
        {
            SceneLightData sceneLight;
            sceneLight.type = node->lights[i]->type;
            sceneLight.angle = node->lights[i]->angle;
            sceneLight.color = node->lights[i]->color;
            // sceneLight.dir = node->lights[i]->dir;
            sceneLight.id = node->lights[i]->id;
            sceneLight.penumbra = node->lights[i]->penumbra;
            sceneLight.width = node->lights[i]->width;
            sceneLight.height = node->lights[i]->height;
            sceneLight.function = node->lights[i]->function;

            if (node->lights[i]->type != LightType::LIGHT_DIRECTIONAL)
            {
                sceneLight.pos = ctm * glm::vec4({0.f, 0.f, 0.f, 1.f});
            }
            if (node->lights[i]->type != LightType::LIGHT_POINT)
            {
                sceneLight.dir = ctm * node->lights[i]->dir;
            }
            renderData->lights.push_back(sceneLight);
        }
    }

    if (!node->children.empty())
    {
        for (int i = 0; i < node->children.size(); i++)
        {
            dfsSceneTree(node->children[i], renderData, ctm, shapeIdCounter);
        }
    }
}

bool SceneParser::parse(std::string filepath, RenderData &renderData) {
    ScenefileReader fileReader = ScenefileReader(filepath);
    bool success = fileReader.readJSON();
    if (!success) {
        return false;
    }

    // TODO: Use your Lab 5 code here

    // populate renderData with global data, and camera data;
    renderData.cameraData = fileReader.getCameraData();
    renderData.cameraData.pos[3] = 1.f;
    renderData.globalData = fileReader.getGlobalData();

    // populate renderData's list of primitives and their transforms.
    //         This will involve traversing the scene graph, and we recommend you
    //         create a helper function to do so!
    SceneNode* root = fileReader.getRootNode();
    renderData.shapes.clear();
    renderData.lights.clear();

    int shapeIdCounter = 0;
    dfsSceneTree(root, &renderData, glm::mat4(1.0), shapeIdCounter);

    return true;
}

