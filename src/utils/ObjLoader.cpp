#include "ObjLoader.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cctype>
#include <algorithm>
#include <unordered_map>
#include <stdexcept>
#include <glm/glm.hpp>

namespace {

struct FaceIdx {
    // 0-based indices; -1 if missing
    int v = -1;  // position index
    int vt = -1; // texcoord index
    int vn = -1; // normal index
};

// Convert possibly negative OBJ index to 0-based absolute index
inline int resolveIndex(int idxToken, int listSize) {
    if (idxToken > 0) {
        return idxToken - 1; // OBJ is 1-based
    } else if (idxToken < 0) {
        return listSize + idxToken; // negative indices are relative to end
    } else {
        return -1; // 0 is invalid in OBJ
    }
}

inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
}
inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
}
inline void trim(std::string &s) { ltrim(s); rtrim(s); }

inline FaceIdx parseFaceToken(const std::string &token, int vCount, int vtCount, int vnCount) {
    FaceIdx f;
    int a = 0, b = 0, c = 0;
    size_t firstSlash = token.find('/');
    if (firstSlash == std::string::npos) {
        // "v"
        a = std::stoi(token);
        f.v = resolveIndex(a, vCount);
        return f;
    }
    size_t secondSlash = token.find('/', firstSlash + 1);
    if (secondSlash == std::string::npos) {
        // "v/vt"
        a = std::stoi(token.substr(0, firstSlash));
        b = std::stoi(token.substr(firstSlash + 1));
        f.v = resolveIndex(a, vCount);
        f.vt = resolveIndex(b, vtCount);
        return f;
    }
    // "v//vn" or "v/vt/vn"
    std::string sA = token.substr(0, firstSlash);
    std::string sB = token.substr(firstSlash + 1, secondSlash - firstSlash - 1);
    std::string sC = token.substr(secondSlash + 1);
    if (!sA.empty()) a = std::stoi(sA);
    f.v = resolveIndex(a, vCount);
    if (!sB.empty()) {
        b = std::stoi(sB);
        f.vt = resolveIndex(b, vtCount);
    }
    if (!sC.empty()) {
        c = std::stoi(sC);
        f.vn = resolveIndex(c, vnCount);
    }
    return f;
}

inline glm::vec3 computeFaceNormal(const glm::vec3 &a, const glm::vec3 &b, const glm::vec3 &c) {
    glm::vec3 e1 = b - a;
    glm::vec3 e2 = c - a;
    glm::vec3 n = glm::cross(e1, e2);
    float len = glm::length(n);
    if (len > 0.f) n /= len;
    return n;
}

} 

namespace ObjLoader {

bool load(const std::string &filepath,
          std::vector<float> &outInterleavedPN,
          std::string &errMsg) {
    outInterleavedPN.clear();
    errMsg.clear();

    std::ifstream in(filepath);
    if (!in) {
        errMsg = "Failed to open OBJ file: " + filepath;
        return false;
    }

    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> texcoords;

    std::string line;
    positions.reserve(1 << 12);
    normals.reserve(1 << 12);
    texcoords.reserve(1 << 12);
    outInterleavedPN.reserve(1 << 16);

    while (std::getline(in, line)) {
        trim(line);
        if (line.empty() || line[0] == '#') continue;

        std::istringstream ss(line);
        std::string tag;
        ss >> tag;
        if (tag == "v") {
            glm::vec3 p(0.f);
            ss >> p.x >> p.y >> p.z;
            positions.push_back(p);
        } else if (tag == "vn") {
            glm::vec3 n(0.f);
            ss >> n.x >> n.y >> n.z;
            normals.push_back(n);
        } else if (tag == "vt") {
            glm::vec2 t(0.f);
            ss >> t.x >> t.y;
            texcoords.push_back(t);
        } else if (tag == "f") {
            // Collect tokens for this face
            std::vector<std::string> tokens;
            std::string tok;
            while (ss >> tok) tokens.push_back(tok);
            if (tokens.size() < 3) continue;

            // Convert tokens to indices
            std::vector<FaceIdx> idxs;
            idxs.reserve(tokens.size());
            for (const auto &t : tokens) {
                idxs.push_back(parseFaceToken(t,
                                              static_cast<int>(positions.size()),
                                              static_cast<int>(texcoords.size()),
                                              static_cast<int>(normals.size())));
            }

            // Triangulate via fan: (0, i, i+1)
            for (size_t i = 1; i + 1 < idxs.size(); ++i) {
                const FaceIdx &a = idxs[0];
                const FaceIdx &b = idxs[i];
                const FaceIdx &c = idxs[i + 1];
                if (a.v < 0 || b.v < 0 || c.v < 0) {
                    // malformed face
                    continue;
                }
                const glm::vec3 &pa = positions[a.v];
                const glm::vec3 &pb = positions[b.v];
                const glm::vec3 &pc = positions[c.v];

                // Use per-face (flat) normal for this triangle, regardless of OBJ-provided normals
                glm::vec3 n = computeFaceNormal(pa, pb, pc);
                glm::vec3 na = n, nb = n, nc = n;

                // interleaved PN
                outInterleavedPN.push_back(pa.x);
                outInterleavedPN.push_back(pa.y);
                outInterleavedPN.push_back(pa.z);
                outInterleavedPN.push_back(na.x);
                outInterleavedPN.push_back(na.y);
                outInterleavedPN.push_back(na.z);

                outInterleavedPN.push_back(pb.x);
                outInterleavedPN.push_back(pb.y);
                outInterleavedPN.push_back(pb.z);
                outInterleavedPN.push_back(nb.x);
                outInterleavedPN.push_back(nb.y);
                outInterleavedPN.push_back(nb.z);

                outInterleavedPN.push_back(pc.x);
                outInterleavedPN.push_back(pc.y);
                outInterleavedPN.push_back(pc.z);
                outInterleavedPN.push_back(nc.x);
                outInterleavedPN.push_back(nc.y);
                outInterleavedPN.push_back(nc.z);
            }
        } 
    }

    if (outInterleavedPN.empty()) {
        errMsg = "OBJ contained no triangles or failed to parse faces.";
        return false;
    }
    return true;
}

}


