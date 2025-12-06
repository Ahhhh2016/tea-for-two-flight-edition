#include "Cube.h"

void Cube::makeTile(glm::vec3 topLeft,
                    glm::vec3 topRight,
                    glm::vec3 bottomLeft,
                    glm::vec3 bottomRight,
                    glm::vec2 uvTL,
                    glm::vec2 uvTR,
                    glm::vec2 uvBL,
                    glm::vec2 uvBR) {
    // Task 2: create a tile (i.e. 2 triangles) based on 4 given points.
    glm::vec3 n0 = glm::normalize(glm::cross((topLeft - bottomLeft), (topLeft - topRight)));
    insertVec3(m_vertexData, topLeft);     insertVec3(m_vertexData, n0); insertVec2(m_vertexData, uvTL);
    insertVec3(m_vertexData, bottomLeft);  insertVec3(m_vertexData, n0); insertVec2(m_vertexData, uvBL);
    insertVec3(m_vertexData, bottomRight); insertVec3(m_vertexData, n0); insertVec2(m_vertexData, uvBR);

    glm::vec3 n1 = n0;
    insertVec3(m_vertexData, topLeft);     insertVec3(m_vertexData, n1); insertVec2(m_vertexData, uvTL);
    insertVec3(m_vertexData, bottomRight); insertVec3(m_vertexData, n1); insertVec2(m_vertexData, uvBR);
    insertVec3(m_vertexData, topRight);    insertVec3(m_vertexData, n1); insertVec2(m_vertexData, uvTR);
}

void Cube::makeFace(glm::vec3 topLeft,
                    glm::vec3 topRight,
                    glm::vec3 bottomLeft,
                    glm::vec3 bottomRight) {
    // Task 3: create a single side of the cube out of the 4
    //         given points and makeTile()
    // Note: think about how param 1 affects the number of triangles on
    //       the face of the cube
    int divisions = std::max(1, m_param1);
    glm::vec3 stepX = (topRight - topLeft) / static_cast<float>(divisions);
    glm::vec3 stepY = (bottomLeft - topLeft) / static_cast<float>(divisions);

    for (int i = 0; i < divisions; i++) {
        for (int j = 0; j < divisions; j++) {
            glm::vec3 tl = topLeft + static_cast<float>(j) * stepX + static_cast<float>(i) * stepY;
            glm::vec3 tr = topLeft + static_cast<float>(j + 1) * stepX + static_cast<float>(i) * stepY;
            glm::vec3 bl = topLeft + static_cast<float>(j) * stepX + static_cast<float>(i + 1) * stepY;
            glm::vec3 br = topLeft + static_cast<float>(j + 1) * stepX + static_cast<float>(i + 1) * stepY;

            float du = 1.f / static_cast<float>(divisions);
            float dv = 1.f / static_cast<float>(divisions);
            glm::vec2 uvTL(static_cast<float>(j) * du,       static_cast<float>(i) * dv);
            glm::vec2 uvTR(static_cast<float>(j + 1) * du,   static_cast<float>(i) * dv);
            glm::vec2 uvBL(static_cast<float>(j) * du,       static_cast<float>(i + 1) * dv);
            glm::vec2 uvBR(static_cast<float>(j + 1) * du,   static_cast<float>(i + 1) * dv);

            makeTile(tl, tr, bl, br, uvTL, uvTR, uvBL, uvBR);
        }
    }
}

void Cube::setVertexData() {
    // Uncomment these lines for Task 2, then comment them out for Task 3:


    // Uncomment these lines for Task 3:

    // +Z (front)
    makeFace(glm::vec3(-0.5f,  0.5f, 0.5f),
             glm::vec3( 0.5f,  0.5f, 0.5f),
             glm::vec3(-0.5f, -0.5f, 0.5f),
             glm::vec3( 0.5f, -0.5f, 0.5f));

    // Task 4: Use the makeFace() function to make all 6 sides of the cube

    // -Z (back)
    makeFace(glm::vec3( 0.5f,  0.5f, -0.5f),
    glm::vec3(-0.5f,  0.5f, -0.5f),
    glm::vec3( 0.5f, -0.5f, -0.5f),
    glm::vec3(-0.5f, -0.5f, -0.5f));
    // -X (left)
    makeFace(glm::vec3(-0.5f,  0.5f, -0.5f),
            glm::vec3(-0.5f,  0.5f,  0.5f),
            glm::vec3(-0.5f, -0.5f, -0.5f),
            glm::vec3(-0.5f, -0.5f,  0.5f));
    // +X (right)
    makeFace(glm::vec3( 0.5f,  0.5f,  0.5f),
            glm::vec3( 0.5f,  0.5f, -0.5f),
            glm::vec3( 0.5f, -0.5f,  0.5f),
            glm::vec3( 0.5f, -0.5f, -0.5f));
    // +Y (top)
    makeFace(glm::vec3(-0.5f,  0.5f, -0.5f),
            glm::vec3( 0.5f,  0.5f, -0.5f),
            glm::vec3(-0.5f,  0.5f,  0.5f),
            glm::vec3( 0.5f,  0.5f,  0.5f));
    // -Y (bottom)
    makeFace(glm::vec3(-0.5f, -0.5f,  0.5f),
            glm::vec3( 0.5f, -0.5f,  0.5f),
            glm::vec3(-0.5f, -0.5f, -0.5f),
            glm::vec3( 0.5f, -0.5f, -0.5f));

}
