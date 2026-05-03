#include "SmdParser.h"
#include <iostream>
#include <algorithm>
#include <cctype>
#include "utils/FileIO.h"

// Trim whitespace
std::string SmdParser::Trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (std::string::npos == first) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

bool SmdParser::Parse(const std::string& filePath) {
    m_bones.clear();
    m_triangles.clear();
    m_valid = false;
    m_error.clear();

    std::ifstream file(FileIO::toPath(filePath));
    if (!file.is_open()) {
        m_error = "Failed to open file: " + filePath;
        return false;
    }

    std::string line;

    if (!std::getline(file, line)) {
        m_error = "Failed to read version line";
        return false;
    }

    line = Trim(line);
    if (line != "version 1") {
        m_error = "Unsupported SMD version: " + line;
        return false;
    }

    // Parse sections
    while (std::getline(file, line)) {
        line = Trim(line);

        if (line == "nodes") {
            if (!ParseNodes(file)) {
                return false;
            }
        }
        else if (line == "skeleton") {
            if (!ParseSkeleton(file)) {
                return false;
            }
        }
        else if (line == "triangles") {
            if (!ParseTriangles(file)) {
                return false;
            }
        }
    }

    if (m_bones.empty()) {
        m_error = "No bones found in SMD file";
        return false;
    }

    m_valid = true;
    return true;
}

bool SmdParser::ParseNodes(std::ifstream& file) {
    std::string line;

    while (std::getline(file, line)) {
        line = Trim(line);

        if (line == "end") {
            return true;
        }

        // Parse: index "name" parentIndex
        SmdBone bone;

        // Find first space (after index)
        size_t firstSpace = line.find(' ');
        if (firstSpace == std::string::npos) {
            m_error = "Invalid node line: " + line;
            return false;
        }

        bone.index = std::stoi(line.substr(0, firstSpace));

        // Find quoted name
        size_t quoteStart = line.find('"', firstSpace);
        size_t quoteEnd = line.find('"', quoteStart + 1);
        if (quoteStart == std::string::npos || quoteEnd == std::string::npos) {
            m_error = "Invalid node name in line: " + line;
            return false;
        }

        bone.name = line.substr(quoteStart + 1, quoteEnd - quoteStart - 1);

        // Parse parent index after closing quote
        std::string remainder = Trim(line.substr(quoteEnd + 1));
        bone.parentIndex = std::stoi(remainder);

        // Initialize transform to zero (will be set in skeleton section)
        bone.posX = bone.posY = bone.posZ = 0.0f;
        bone.rotX = bone.rotY = bone.rotZ = 0.0f;

        m_bones.push_back(bone);
    }

    m_error = "Unexpected end of file in nodes section";
    return false;
}

bool SmdParser::ParseSkeleton(std::ifstream& file) {
    std::string line;
    bool inTimeBlock = false;

    while (std::getline(file, line)) {
        line = Trim(line);

        if (line == "end") {
            return true;
        }

        // Check for time marker
        if (line.substr(0, 4) == "time") {
            inTimeBlock = true;
            continue;
        }

        if (!inTimeBlock) {
            continue;
        }

        // Parse: boneIndex posX posY posZ rotX rotY rotZ
        std::istringstream iss(line);
        int boneIndex;
        float posX, posY, posZ, rotX, rotY, rotZ;

        if (!(iss >> boneIndex >> posX >> posY >> posZ >> rotX >> rotY >> rotZ)) {
            m_error = "Invalid skeleton line: " + line;
            return false;
        }

        if (boneIndex >= 0 && boneIndex < static_cast<int>(m_bones.size())) {
            m_bones[boneIndex].posX = posX;
            m_bones[boneIndex].posY = posY;
            m_bones[boneIndex].posZ = posZ;
            m_bones[boneIndex].rotX = rotX;
            m_bones[boneIndex].rotY = rotY;
            m_bones[boneIndex].rotZ = rotZ;
        }

    }

    m_error = "Unexpected end of file in skeleton section";
    return false;
}

bool SmdParser::ParseTriangles(std::ifstream& file) {
    std::string line;

    while (std::getline(file, line)) {
        line = Trim(line);

        if (line == "end") {
            return true;
        }

        // First line is material name
        SmdTriangle tri;
        tri.material = line;

        // Read 3 vertices
        for (int i = 0; i < 3; i++) {
            if (!std::getline(file, line)) {
                m_error = "Unexpected end of file reading triangle vertex";
                return false;
            }

            line = Trim(line);
            std::istringstream iss(line);

            // Parse: boneIndex x y z nx ny nz u v
            if (!(iss >> tri.vertices[i].boneIndex
                >> tri.vertices[i].x >> tri.vertices[i].y >> tri.vertices[i].z
                >> tri.vertices[i].nx >> tri.vertices[i].ny >> tri.vertices[i].nz
                >> tri.vertices[i].u >> tri.vertices[i].v)) {
                m_error = "Invalid vertex line: " + line;
                return false;
            }
        }

        m_triangles.push_back(tri);
    }

    m_error = "Unexpected end of file in triangles section";
    return false;
}

int SmdParser::FindBoneIndex(const std::string& name) const {
    for (const auto& bone : m_bones) {
        if (bone.name == name) {
            return bone.index;
        }
    }
    return -1;
}



const SmdBone* SmdParser::GetBone(int index) const {
    if (index >= 0 && index < static_cast<int>(m_bones.size())) {
        return &m_bones[index];
    }
    return nullptr;
}
