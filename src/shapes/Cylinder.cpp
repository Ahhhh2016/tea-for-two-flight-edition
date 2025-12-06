#include "Cylinder.h"
#include <algorithm>
#include <cmath>

void Cylinder::makeCapTile(float y, bool isTop, float innerRadius, float outerRadius, float currentTheta, float nextTheta)
{
    const glm::vec3 n = isTop ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(0.0f, -1.0f, 0.0f);

    glm::vec3 inner0(innerRadius * glm::cos(currentTheta), y, innerRadius * glm::sin(currentTheta));
    glm::vec3 inner1(innerRadius * glm::cos(nextTheta),    y, innerRadius * glm::sin(nextTheta));
    glm::vec3 outer0(outerRadius * glm::cos(currentTheta), y, outerRadius * glm::sin(currentTheta));
    glm::vec3 outer1(outerRadius * glm::cos(nextTheta),    y, outerRadius * glm::sin(nextTheta));

    if (innerRadius == 0.0f) {
        glm::vec3 center(0.0f, y, 0.0f);
        glm::vec2 uvCenter(0.5f, 0.5f);
        auto uvFromXZ = [this](const glm::vec3 &p) -> glm::vec2 {
            float u = 0.5f + (p.x / (2.f * m_radius));
            float v = 0.5f + (p.z / (2.f * m_radius));
            return glm::vec2(u, v);
        };
        auto uvCap = [&](const glm::vec3 &p) -> glm::vec2 {
            glm::vec2 uv = uvFromXZ(p);
            if (!isTop) {
                uv.y = 1.0f - uv.y;
            }
            return uv;
        };
        if (isTop) {
            // CCW when viewed from above
            insertVec3(m_vertexData, center);  insertVec3(m_vertexData, n); insertVec2(m_vertexData, uvCenter);
            insertVec3(m_vertexData, outer1);  insertVec3(m_vertexData, n); insertVec2(m_vertexData, uvCap(outer1));
            insertVec3(m_vertexData, outer0);  insertVec3(m_vertexData, n); insertVec2(m_vertexData, uvCap(outer0));
        } else {
            // CCW when viewed from below
            insertVec3(m_vertexData, center);  insertVec3(m_vertexData, n); insertVec2(m_vertexData, uvCenter);
            insertVec3(m_vertexData, outer0);  insertVec3(m_vertexData, n); insertVec2(m_vertexData, uvCap(outer0));
            insertVec3(m_vertexData, outer1);  insertVec3(m_vertexData, n); insertVec2(m_vertexData, uvCap(outer1));
        }
        return;
    }

    auto uvFromXZ = [this](const glm::vec3 &p) -> glm::vec2 {
        float u = 0.5f + (p.x / (2.f * m_radius));
        float v = 0.5f + (p.z / (2.f * m_radius));
        return glm::vec2(u, v);
    };
    auto uvCap = [&](const glm::vec3 &p) -> glm::vec2 {
        glm::vec2 uv = uvFromXZ(p);
        if (!isTop) {
            uv.y = 1.0f - uv.y;
        }
        return uv;
    };

    if (isTop) {
        // Reverse order vs bottom to keep CCW from above
        insertVec3(m_vertexData, inner0); insertVec3(m_vertexData, n); insertVec2(m_vertexData, uvCap(inner0));
        insertVec3(m_vertexData, outer1); insertVec3(m_vertexData, n); insertVec2(m_vertexData, uvCap(outer1));
        insertVec3(m_vertexData, outer0); insertVec3(m_vertexData, n); insertVec2(m_vertexData, uvCap(outer0));

        insertVec3(m_vertexData, inner0); insertVec3(m_vertexData, n); insertVec2(m_vertexData, uvCap(inner0));
        insertVec3(m_vertexData, inner1); insertVec3(m_vertexData, n); insertVec2(m_vertexData, uvCap(inner1));
        insertVec3(m_vertexData, outer1); insertVec3(m_vertexData, n); insertVec2(m_vertexData, uvCap(outer1));
    } else {
        // Bottom: standard fan outward, CCW from below
        insertVec3(m_vertexData, inner0); insertVec3(m_vertexData, n); insertVec2(m_vertexData, uvCap(inner0));
        insertVec3(m_vertexData, outer0); insertVec3(m_vertexData, n); insertVec2(m_vertexData, uvCap(outer0));
        insertVec3(m_vertexData, outer1); insertVec3(m_vertexData, n); insertVec2(m_vertexData, uvCap(outer1));

        insertVec3(m_vertexData, inner0); insertVec3(m_vertexData, n); insertVec2(m_vertexData, uvCap(inner0));
        insertVec3(m_vertexData, outer1); insertVec3(m_vertexData, n); insertVec2(m_vertexData, uvCap(outer1));
        insertVec3(m_vertexData, inner1); insertVec3(m_vertexData, n); insertVec2(m_vertexData, uvCap(inner1));
    }
}

void Cylinder::makeCapSlice(float currentTheta, float nextTheta, bool isTop)
{
    int radialDivisions = std::max(1, m_param1);
    float radiusStep = m_radius / static_cast<float>(radialDivisions);
    float y = isTop ? 0.5f : -0.5f;

    for (int r = 0; r < radialDivisions; r++) {
        float innerRadius = static_cast<float>(r) * radiusStep;
        float outerRadius = static_cast<float>(r + 1) * radiusStep;
        makeCapTile(y, isTop, innerRadius, outerRadius, currentTheta, nextTheta);
    }
}

void Cylinder::makeSideTile(float yTop, float yBottom, float currentTheta, float nextTheta)
{
    glm::vec3 topLeft(m_radius * glm::cos(currentTheta),  yTop,    m_radius * glm::sin(currentTheta));
    glm::vec3 topRight(m_radius * glm::cos(nextTheta),    yTop,    m_radius * glm::sin(nextTheta));
    glm::vec3 bottomLeft(m_radius * glm::cos(currentTheta), yBottom, m_radius * glm::sin(currentTheta));
    glm::vec3 bottomRight(m_radius * glm::cos(nextTheta),   yBottom, m_radius * glm::sin(nextTheta));

    glm::vec3 nTL(glm::cos(currentTheta), 0.0f, glm::sin(currentTheta));
    glm::vec3 nTR(glm::cos(nextTheta),    0.0f, glm::sin(nextTheta));
    glm::vec3 nBL = nTL;
    glm::vec3 nBR = nTR;

    auto uvFrom = [](float theta, float y) -> glm::vec2 {
        float u = theta / (2.f * static_cast<float>(M_PI));
        // Map y in [-0.5, 0.5] to v in [0,1] with v=1 at top
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

void Cylinder::makeSideSlice(float currentTheta, float nextTheta)
{
    int verticalDivisions = std::max(1, m_param1);
    float stepY = 1.0f / static_cast<float>(verticalDivisions);
    for (int v = 0; v < verticalDivisions; v++) {
        float yTop = 0.5f - static_cast<float>(v) * stepY;
        float yBottom = 0.5f - static_cast<float>(v + 1) * stepY;
        makeSideTile(yTop, yBottom, currentTheta, nextTheta);
    }
}

void Cylinder::makeWedge(float currentTheta, float nextTheta)
{
    makeCapSlice(currentTheta, nextTheta, false);
    makeCapSlice(currentTheta, nextTheta, true);
    makeSideSlice(currentTheta, nextTheta);
}

void Cylinder::setVertexData() {
    int slices = std::max(3, m_param2);
    float dTheta = 2.f * static_cast<float>(M_PI) / static_cast<float>(slices);
    for (int s = 0; s < slices; s++) {
        float currentTheta = static_cast<float>(s) * dTheta;
        float nextTheta = static_cast<float>(s + 1) * dTheta;
        makeWedge(currentTheta, nextTheta);
    }
}
