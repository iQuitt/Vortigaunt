#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <sstream>

// GoldSrc SMD bone data
struct SmdBone {
    int index;
    std::string name;
    int parentIndex;
    // Position (translation)
    float posX, posY, posZ;
    // Rotation (euler angles in radians)
    float rotX, rotY, rotZ;
};

// GoldSrc SMD vertex data (single bone weight)
struct SmdVertex {
    int boneIndex;
    float x, y, z;      // position
    float nx, ny, nz;   // normal
    float u, v;         // UV coordinates
};

// GoldSrc SMD triangle
struct SmdTriangle {
    std::string material;
    SmdVertex vertices[3];
};

// Parser for GoldSrc SMD files
// Extracts skeleton hierarchy and mesh data
class SmdParser {
public:
    SmdParser() = default;
    ~SmdParser() = default;

    // Parse an SMD file
    bool Parse(const std::string& filePath);
    
    // Get parsed data
    const std::vector<SmdBone>& GetBones() const { return m_bones; }
    const std::vector<SmdTriangle>& GetTriangles() const { return m_triangles; }
    
    // Check if parsing was successful
    bool IsValid() const { return m_valid; }
    
    // Get error message if parsing failed
    const std::string& GetError() const { return m_error; }
    
    // Find bone by name
    int FindBoneIndex(const std::string& name) const;
    
    // Get bone by index
    const SmdBone* GetBone(int index) const;

private:
    std::vector<SmdBone> m_bones;
    std::vector<SmdTriangle> m_triangles;
    bool m_valid = false;
    std::string m_error;
    
    // Parsing helpers
    bool ParseNodes(std::ifstream& file);
    bool ParseSkeleton(std::ifstream& file);
    bool ParseTriangles(std::ifstream& file);
    
    // Utility
    static std::string Trim(const std::string& str);
};
