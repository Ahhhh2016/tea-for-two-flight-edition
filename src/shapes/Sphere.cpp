#include "Sphere.h"
#include <glm/gtc/constants.hpp>

void Sphere::makeTile(glm::vec3 topLeft,
                      glm::vec3 topRight,
                      glm::vec3 bottomLeft,
                      glm::vec3 bottomRight,
                      glm::vec2 uvTL,
                      glm::vec2 uvTR,
                      glm::vec2 uvBL,
                      glm::vec2 uvBR) {
    // Task 5: Implement the makeTile() function for a Sphere
    // Note: this function is very similar to the makeTile() function for Cube,
    //       but the normals are calculated in a different way!
    insertVec3(m_vertexData, topLeft);     insertVec3(m_vertexData, glm::normalize(topLeft));     insertVec2(m_vertexData, uvTL);
    insertVec3(m_vertexData, bottomRight); insertVec3(m_vertexData, glm::normalize(bottomRight)); insertVec2(m_vertexData, uvBR);
    insertVec3(m_vertexData, bottomLeft);  insertVec3(m_vertexData, glm::normalize(bottomLeft));  insertVec2(m_vertexData, uvBL);

    insertVec3(m_vertexData, bottomRight); insertVec3(m_vertexData, glm::normalize(bottomRight)); insertVec2(m_vertexData, uvBR);
    insertVec3(m_vertexData, topLeft);     insertVec3(m_vertexData, glm::normalize(topLeft));     insertVec2(m_vertexData, uvTL);
    insertVec3(m_vertexData, topRight);    insertVec3(m_vertexData, glm::normalize(topRight));    insertVec2(m_vertexData, uvTR);
}

void Sphere::makeWedge(float currentTheta, float nextTheta) {
    // Task 6: create a single wedge of the sphere using the
    //         makeTile() function you implemented in Task 5
    // Note: think about how param 1 comes into play here!
    int divisions = std::max(2, m_param1);
    float phiStep = glm::radians(180.f / (float)divisions);

    for (int i = 0; i < divisions; i++) {
        float currentPhi = i * phiStep;
        float nextPhi = (i + 1 == divisions) ? glm::radians(180.f) : (i + 1) * phiStep;

        glm::vec3 topLeft;
        topLeft.x = m_radius * glm::cos(currentTheta) * glm::sin(currentPhi);
        topLeft.y = m_radius * glm::cos(currentPhi);
        topLeft.z = m_radius * glm::sin(currentTheta) * glm::sin(currentPhi);
        // printf("%f %f %f\n", topLeft.x, topLeft.y, topLeft.z);

        glm::vec3 topRight;
        topRight.x = m_radius * glm::cos(nextTheta) * glm::sin(currentPhi);
        topRight.y = m_radius * glm::cos(currentPhi);
        topRight.z = m_radius * glm::sin(nextTheta) * glm::sin(currentPhi);
        // printf("%f %f %f\n", topRight.x, topRight.y, topRight.z);

        glm::vec3 bottomLeft;
        bottomLeft.x = m_radius * glm::cos(currentTheta) * glm::sin(nextPhi);
        bottomLeft.y = m_radius * glm::cos(nextPhi);
        bottomLeft.z = m_radius * glm::sin(currentTheta) * glm::sin(nextPhi);
        //        printf("%f %f %f\n", bottomLeft.x, bottomLeft.y, bottomLeft.z);

        glm::vec3 bottomRight;
        bottomRight.x = m_radius * glm::cos(nextTheta) * glm::sin(nextPhi);
        bottomRight.y = m_radius * glm::cos(nextPhi);
        bottomRight.z = m_radius * glm::sin(nextTheta) * glm::sin(nextPhi);
        //        printf("%f %f %f\n", bottomRight.x, bottomRight.y, bottomRight.z);
        //        printf("\n");


        auto uvFrom = [](float theta, float phi) -> glm::vec2 {
            // theta in [0, 2pi], phi in [0, pi]
            float u = 1.f - (theta / (2.f * glm::pi<float>()));
            float v = (phi / glm::pi<float>()); 
            return glm::vec2(u, v);
        };
        glm::vec2 uvTL = uvFrom(currentTheta, currentPhi);
        glm::vec2 uvTR = uvFrom(nextTheta,   currentPhi);
        glm::vec2 uvBL = uvFrom(currentTheta, nextPhi);
        glm::vec2 uvBR = uvFrom(nextTheta,   nextPhi);

        // Fix seam by wrapping u=1 to u=0 to avoid interpolation across seam
        if (uvTR.x - uvTL.x > 0.5f) uvTR.x -= 1.f;
        if (uvBR.x - uvBL.x > 0.5f) uvBR.x -= 1.f;
        if (uvTL.x - uvTR.x > 0.5f) uvTL.x -= 1.f;
        if (uvBL.x - uvBR.x > 0.5f) uvBL.x -= 1.f;

        makeTile(topLeft, topRight, bottomLeft, bottomRight, uvTL, uvTR, uvBL, uvBR);
    }
}

void Sphere::makeSphere() {
    // Task 7: create a full sphere using the makeWedge() function you
    //         implemented in Task 6
    // Note: think about how param 2 comes into play here!

    int slices = std::max(3, m_param2);
    float thetaStep = glm::radians(360.f / slices);
    for (int i = 0; i < slices; i++)
    {
        float currentTheta = i * thetaStep;
        float nextTheta = (i + 1) * thetaStep;
        makeWedge(currentTheta, nextTheta);
    }
}

void Sphere::setVertexData() {
    // Uncomment these lines to make a wedge for Task 6, then comment them out for Task 7:

    // float thetaStep = glm::radians(360.f / m_param2);
    // float currentTheta = 0 * thetaStep;
    // float nextTheta = 1 * thetaStep;
    // makeWedge(currentTheta, nextTheta);

    // Uncomment these lines to make sphere for Task 7:
    makeSphere();
}
