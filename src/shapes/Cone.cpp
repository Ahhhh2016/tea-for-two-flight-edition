#include "Cone.h"
#include <algorithm>
#include <cmath>

// updateParams now implemented in ShapeBase


void Cone::setVertexData() {
    // TODO for Project 5: Lights, Camera
    int slices = std::max(3, m_param2);
    float dTheta = 2 * M_PI / static_cast<float>(slices);
    for (int i = 0; i < slices; i++) {
        float currentTheta = static_cast<float>(i) * dTheta;
        float nextTheta = static_cast<float>(i + 1) * dTheta;
        makeWedge(currentTheta, nextTheta);
    }
}


// slope normals for the actual implicit cone (not trimesh cone)
glm::vec3 Cone::calcNorm(glm::vec3 &pt) {
    float xNorm = (2.f * pt.x);
    float yNorm = -(1.f / 4.f) * (2.f * pt.y - 1.f);
    float zNorm = (2.f * pt.z);
    return glm::normalize(glm::vec3{xNorm, yNorm, zNorm});
}

void Cone::makeCapTile(float innerRadius, float outerRadius, float currentTheta, float nextTheta) {
    const float y = -0.5f;
    const glm::vec3 normal(0.0f, -1.0f, 0.0f);

    // Inner ring points (degenerate at the center if innerRadius == 0)
    glm::vec3 inner0(innerRadius * glm::cos(currentTheta), y, innerRadius * glm::sin(currentTheta));
    glm::vec3 inner1(innerRadius * glm::cos(nextTheta),    y, innerRadius * glm::sin(nextTheta));

    // Outer ring points
    glm::vec3 outer0(outerRadius * glm::cos(currentTheta), y, outerRadius * glm::sin(currentTheta));
    glm::vec3 outer1(outerRadius * glm::cos(nextTheta),    y, outerRadius * glm::sin(nextTheta));

    if (innerRadius == 0.0f) {
        // Triangle fan at the center
        glm::vec3 center(0.0f, y, 0.0f);
        glm::vec2 uvCenter(0.5f, 0.5f);
        auto uvFromXZ = [this](const glm::vec3 &p) -> glm::vec2 {
            float u = 0.5f + (p.x / (2.f * m_radius));
            float v = 0.5f + (p.z / (2.f * m_radius));
            return glm::vec2(u, 1.0f - v);
        };

        insertVec3(m_vertexData, center); insertVec3(m_vertexData, normal); insertVec2(m_vertexData, uvCenter);
        insertVec3(m_vertexData, outer0); insertVec3(m_vertexData, normal); insertVec2(m_vertexData, uvFromXZ(outer0));
        insertVec3(m_vertexData, outer1); insertVec3(m_vertexData, normal); insertVec2(m_vertexData, uvFromXZ(outer1));
        return;
    }

    auto uvFromXZ = [this](const glm::vec3 &p) -> glm::vec2 {
        float u = 0.5f + (p.x / (2.f * m_radius));
        float v = 0.5f + (p.z / (2.f * m_radius));
        return glm::vec2(u, 1.0f - v);
    };
    insertVec3(m_vertexData, inner0); insertVec3(m_vertexData, normal); insertVec2(m_vertexData, uvFromXZ(inner0));
    insertVec3(m_vertexData, outer0); insertVec3(m_vertexData, normal); insertVec2(m_vertexData, uvFromXZ(outer0));
    insertVec3(m_vertexData, outer1); insertVec3(m_vertexData, normal); insertVec2(m_vertexData, uvFromXZ(outer1));

    insertVec3(m_vertexData, inner0); insertVec3(m_vertexData, normal); insertVec2(m_vertexData, uvFromXZ(inner0));
    insertVec3(m_vertexData, outer1); insertVec3(m_vertexData, normal); insertVec2(m_vertexData, uvFromXZ(outer1));
    insertVec3(m_vertexData, inner1); insertVec3(m_vertexData, normal); insertVec2(m_vertexData, uvFromXZ(inner1));
}

void Cone::makeCapSlice(float currentTheta, float nextTheta) {
    int radialDivisions = std::max(1, m_param1);
    float radiusStep = m_radius / static_cast<float>(radialDivisions);

    for (int i = 0; i < radialDivisions; i++) {
        float innerRadius = static_cast<float>(i) * radiusStep;
        float outerRadius = static_cast<float>(i + 1) * radiusStep;
        makeCapTile(innerRadius, outerRadius, currentTheta, nextTheta);
    }
}

void Cone::makeSlopeTile(float yTop, float rTop, float yBottom, float rBottom, float currentTheta, float nextTheta) {
    glm::vec3 topLeft(rTop * glm::cos(currentTheta),  yTop,    rTop * glm::sin(currentTheta));
    glm::vec3 topRight(rTop * glm::cos(nextTheta),    yTop,    rTop * glm::sin(nextTheta));
    glm::vec3 bottomLeft(rBottom * glm::cos(currentTheta), yBottom, rBottom * glm::sin(currentTheta));
    glm::vec3 bottomRight(rBottom * glm::cos(nextTheta),   yBottom, rBottom * glm::sin(nextTheta));

    glm::vec3 nTL, nTR, nBL, nBR;

    // when rTop == 0, use the face-direction normal taken from the next ring at mid-theta
    if (rTop == 0.0f) {
        float midTheta = 0.5f * (currentTheta + nextTheta);
        glm::vec3 refPt(rBottom * glm::cos(midTheta), yBottom, rBottom * glm::sin(midTheta));
        glm::vec3 tipNormal = calcNorm(refPt);
        nTL = tipNormal;
        nTR = tipNormal;
    } else {
        nTL = calcNorm(topLeft);
        nTR = calcNorm(topRight);
    }
    nBL = calcNorm(bottomLeft);
    nBR = calcNorm(bottomRight);

    auto uvFrom = [](float theta, float y) -> glm::vec2 {
        float u = theta / (2.f * static_cast<float>(M_PI));
        float v = (y + 0.5f);
        return glm::vec2(1.0f - u, 1.0f - v);
    };
    insertVec3(m_vertexData, topLeft);     insertVec3(m_vertexData, nTL); insertVec2(m_vertexData, uvFrom(currentTheta, yTop));
    insertVec3(m_vertexData, bottomRight); insertVec3(m_vertexData, nBR); insertVec2(m_vertexData, uvFrom(nextTheta, yBottom));
    insertVec3(m_vertexData, bottomLeft);  insertVec3(m_vertexData, nBL); insertVec2(m_vertexData, uvFrom(currentTheta, yBottom));

    insertVec3(m_vertexData, bottomRight); insertVec3(m_vertexData, nBR); insertVec2(m_vertexData, uvFrom(nextTheta, yBottom));
    insertVec3(m_vertexData, topLeft);     insertVec3(m_vertexData, nTL); insertVec2(m_vertexData, uvFrom(currentTheta, yTop));
    insertVec3(m_vertexData, topRight);    insertVec3(m_vertexData, nTR); insertVec2(m_vertexData, uvFrom(nextTheta, yTop));
}

void Cone::makeSlopeSlice(float currentTheta, float nextTheta) {
    int verticalDivisions = std::max(1, m_param1);
    float stepY = 1.0f / static_cast<float>(verticalDivisions); 

    for (int i = 0; i < verticalDivisions; i++) {
        float yTop = 0.5f - static_cast<float>(i) * stepY;
        float yBottom = 0.5f - static_cast<float>(i + 1) * stepY;
        float rTop = m_radius * (static_cast<float>(i) / static_cast<float>(verticalDivisions));
        float rBottom = m_radius * (static_cast<float>(i + 1) / static_cast<float>(verticalDivisions));
        makeSlopeTile(yTop, rTop, yBottom, rBottom, currentTheta, nextTheta);
    }
}

void Cone::makeWedge(float currentTheta, float nextTheta) {
    makeCapSlice(currentTheta, nextTheta);
    makeSlopeSlice(currentTheta, nextTheta);
}
