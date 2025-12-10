#include "terraingenerator.h"

#include <cmath>
#include <cstdlib>
#include <functional>
#include "glm/glm.hpp"

// Helper for generateTerrain()
static void addPointToVector(glm::vec3 point, std::vector<float>& vector) {
    vector.push_back(point.x);
    vector.push_back(point.y);
    vector.push_back(point.z);
}

// Helper for computePerlin() and, possibly, getColor()
static float interpolate(float A, float B, float alpha) {
    // Task 4: implement your easing/interpolation function below
    if (std::fabs(alpha - 0.0f) < 1e-6f) return A;
    if (std::fabs(alpha - 1.0f) < 1e-6f) return B;
    return A + (3.0f * alpha * alpha - 2.0f * alpha * alpha * alpha) * (B - A);
}

// Constructor
TerrainGenerator::TerrainGenerator()
{
  // Task 8: turn off wireframe shading
  // m_wireshade = true; // STENCIL CODE
  m_wireshade = false; // TA SOLUTION

  // Define resolution of terrain generation
  m_resolution = 100;

  // Generate random vector lookup table
  m_lookupSize = 1024;
  m_randVecLookup.reserve(m_lookupSize);

  // Initialize random number generator
  std::srand(1230);

  // Populate random vector lookup table
  for (int i = 0; i < m_lookupSize; i++)
  {
    m_randVecLookup.push_back(glm::vec2(std::rand() * 2.0 / RAND_MAX - 1.0,
                                        std::rand() * 2.0 / RAND_MAX - 1.0));
    }
}

// Destructor
TerrainGenerator::~TerrainGenerator()
{
    m_randVecLookup.clear();
}

// Generates the geometry of the output triangle mesh
std::vector<float> TerrainGenerator::generateTerrain() {
    std::vector<float> verts;
    verts.reserve(m_resolution * m_resolution * 6);

    for(int x = 0; x < m_resolution; x++) {
        for(int y = 0; y < m_resolution; y++) {
            int x1 = x;
            int y1 = y;

            int x2 = x + 1;
            int y2 = y + 1;

            glm::vec3 p1 = getPosition(x1,y1);
            glm::vec3 p2 = getPosition(x2,y1);
            glm::vec3 p3 = getPosition(x2,y2);
            glm::vec3 p4 = getPosition(x1,y2);

            glm::vec3 n1 = getNormal(x1,y1);
            glm::vec3 n2 = getNormal(x2,y1);
            glm::vec3 n3 = getNormal(x2,y2);
            glm::vec3 n4 = getNormal(x1,y2);

            // tris 1
            addPointToVector(p1, verts);
            addPointToVector(n1, verts);
            addPointToVector(getColor(n1, p1), verts);

            addPointToVector(p2, verts);
            addPointToVector(n2, verts);
            addPointToVector(getColor(n2, p2), verts);

            addPointToVector(p3, verts);
            addPointToVector(n3, verts);
            addPointToVector(getColor(n3, p3), verts);

            // tris 2
            addPointToVector(p1, verts);
            addPointToVector(n1, verts);
            addPointToVector(getColor(n1, p1), verts);

            addPointToVector(p3, verts);
            addPointToVector(n3, verts);
            addPointToVector(getColor(n3, p3), verts);

            addPointToVector(p4, verts);
            addPointToVector(n4, verts);
            addPointToVector(getColor(n4, p4), verts);
        }
    }
    return verts;
}

// Samples the (infinite) random vector grid at (row, col)
glm::vec2 TerrainGenerator::sampleRandomVector(int row, int col)
{
    std::hash<int> intHash;
    int index = static_cast<int>(intHash(row * 41 + col * 43) % static_cast<size_t>(m_lookupSize));
    return m_randVecLookup.at(index);
}

// Takes a grid coordinate (row, col), [0, m_resolution), which describes a vertex in a plane mesh
// Returns a normalized position (x, y, z); x and y in range from [0, 1), and z is obtained from getHeight()
glm::vec3 TerrainGenerator::getPosition(int row, int col) {
    // Normalizing the planar coordinates to a unit square 
    // makes scaling independent of sampling resolution.
    float x = 1.0f * static_cast<float>(row) / static_cast<float>(m_resolution);
    float y = 1.0f * static_cast<float>(col) / static_cast<float>(m_resolution);
    float z = getHeight(x, y);
    return glm::vec3(x,y,z);
}

// Takes a normalized (x, y) position, in range [0,1)
// Returns a height value, z, by sampling a noise function
float TerrainGenerator::getHeight(float x, float y) {
    // Map [0,1] -> [0, 2π] so the terrain repeats every 1 unit in x,y
    float ax = 2.f * M_PI * x;
    float ay = 2.f * M_PI * y;

    float hSine = 0.f;
    float amp  = 0.5f;
    float freq = 1.f;

    for (int octave = 0; octave < 5; ++octave) {
        float phaseX = 0.7f * octave + 0.3f;
        float phaseY = 1.1f * octave + 0.9f;
        float term = std::sin(freq * ax + phaseX) * std::sin(freq * ay + phaseY);
        hSine += amp * term;
        amp  *= 0.5f;
        freq *= 2.f;
    }

    // --- add Perlin detail ---
    float perlin = computePerlin(x * 6.0f, y * 6.0f); // more local bumps
    // Perlin from this implementation is ~[-0.7, 0.7]; scale down a bit
    float h = 0.75f * hSine + 0.35f * perlin;

    return 1.2f*h;
}

// Computes the normal of a vertex by averaging neighbors
glm::vec3 TerrainGenerator::getNormal(int row, int col) {
    // Task 9: Compute the average normal for the given input indices
    std::vector<glm::vec3> n;
    n.clear();
    n.push_back(getPosition(row - 1, col - 1));
    n.push_back(getPosition(row - 1, col));
    n.push_back(getPosition(row - 1, col + 1));
    n.push_back(getPosition(row, col + 1));
    n.push_back(getPosition(row + 1, col + 1));
    n.push_back(getPosition(row + 1, col));
    n.push_back(getPosition(row + 1, col - 1));
    n.push_back(getPosition(row, col - 1));

    glm::vec3 v = getPosition(row, col);

    glm::vec3 accNormal = glm::vec3(0.0f);
    for (int i = 0; i < 8; i++)
    {
        glm::vec3 normal = (-glm::cross(n[i] - v, n[(i + 1) % 8] - v));
        accNormal += normal;
    }
    return glm::normalize(accNormal);
}

// Computes color of vertex using normal and, optionally, position
glm::vec3 TerrainGenerator::getColor(glm::vec3 normal, glm::vec3 position) {
    // Task 10: compute color as a function of the normal and position
    // if (glm::dot(normal, glm::vec3(0, 0, 1)) >= 0.6f && position.z > 0.06f) return glm::vec3(1, 1, 1);
    // return glm::vec3(0.5f, 0.5f, 0.5f);
    float h = position.z;              // your height in [0, ~0.9]
    // Base sand color
    glm::vec3 sandBase(0.98f, 0.89f, 0.8f); // tweak if you want

    // Very small height-based variation (no sharp bands)
    float hNorm = glm::clamp(h * 1.2f + 0.5f, 0.0f, 1.0f);
    float brightFactor = glm::mix(0.9f, 1.1f, hNorm); // 0.9–1.1 range
    glm::vec3 base = sandBase * brightFactor;

    // Slight darkening on steep slopes so silhouettes read better
    float slope = 1.0f - glm::abs(glm::dot(normal, glm::vec3(0, 0, 1)));
    float shade = glm::mix(1.0f, 0.7f, slope); // 1 on flat, 0.7 on vertical

    return base * shade;
}
// Computes the intensity of Perlin noise at some point
float TerrainGenerator::computePerlin(float x, float y) {
    // Task 1: get grid indices (as ints)
    int p1x = static_cast<int>(std::floor(x)), p1y = static_cast<int>(std::floor(y));
    int p2x = p1x + 1, p2y = p1y;
    int p3x = p1x,     p3y = p1y + 1;
    int p4x = p1x + 1, p4y = p1y + 1;

    // Task 2: compute offset vectors
    glm::vec2 ov1 = glm::vec2(x, y) - glm::vec2(static_cast<float>(p1x), static_cast<float>(p1y));
    glm::vec2 ov2 = glm::vec2(x, y) - glm::vec2(static_cast<float>(p2x), static_cast<float>(p2y));
    glm::vec2 ov3 = glm::vec2(x, y) - glm::vec2(static_cast<float>(p3x), static_cast<float>(p3y));
    glm::vec2 ov4 = glm::vec2(x, y) - glm::vec2(static_cast<float>(p4x), static_cast<float>(p4y));

    // Task 3: compute the dot product between the grid point direction vectors and its offset vectors
    float A = glm::dot(ov1, glm::normalize(sampleRandomVector(p1x, p1y))); // top-left
    float B = glm::dot(ov2, glm::normalize(sampleRandomVector(p2x, p2y))); // top-right
    float C = glm::dot(ov4, glm::normalize(sampleRandomVector(p4x, p4y))); // bottom-right
    float D = glm::dot(ov3, glm::normalize(sampleRandomVector(p3x, p3y))); // bottom-left

    // Task 5: proper bilinear interpolation with easing
    float sx = x - static_cast<float>(p1x);
    float sy = y - static_cast<float>(p1y);
    float i1 = interpolate(A, B, sx);
    float i2 = interpolate(D, C, sx);
    return interpolate(i1, i2, sy);
}


