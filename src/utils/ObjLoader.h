#pragma once

#include <string>
#include <vector>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>

namespace ObjLoader {

// Returns true on success; otherwise returns false and sets 'errMsg'.
bool load(const std::string &filepath,
          std::vector<float> &outInterleavedPN,
          std::string &errMsg);

} 


