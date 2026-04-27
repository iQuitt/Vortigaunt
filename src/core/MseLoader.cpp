#include "MseLoader.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>

namespace Mse {

// TimeEvent implementation
float TimeEvent::sample(float time) const {
    if (values.empty()) return 0.0f;
    
    auto it = values.lower_bound(time);
    
    if (it == values.begin()) {
        return it->second;
    }
    if (it == values.end()) {
        return values.rbegin()->second;
    }
    
    auto prev = std::prev(it);
    float t0 = prev->first;
    float t1 = it->first;
    float v0 = prev->second;
    float v1 = it->second;
    
    if (t1 == t0) return v0;
    
    float t = (time - t0) / (t1 - t0);
    return v0 + (v1 - v0) * t;
}

float TimeEvent::getFirst() const {
    if (values.empty()) return 0.0f;
    return values.begin()->second;
}

// TimeEventVec3 implementation
std::array<float, 3> TimeEventVec3::sample(float time) const {
    if (values.empty()) return {0.0f, 0.0f, 0.0f};
    
    auto it = values.lower_bound(time);
    
    if (it == values.begin()) {
        return it->second.position;
    }
    if (it == values.end()) {
        return values.rbegin()->second.position;
    }
    
    auto prev = std::prev(it);
    float t0 = prev->first;
    float t1 = it->first;
    const auto& v0 = prev->second;
    const auto& v1 = it->second; // Target keyframe defines the curve type arriving at it? 
    // EffectElementBase.cpp: "TEffectPosition & rPrevEffectPosition = *rPrev;"
    // "int iMovingType = rPrevEffectPosition.m_iMovingType;" -> The PREVIOUS key defines the curve type for the segment!
    
    if (t1 == t0) return v0.position;
    
    float t = (time - t0) / (t1 - t0);
    
    if (v0.movingType == 1) { // Bezier
        // Formula: P(t) = (1-t)^2 * P0 + 2(1-t)t * (P0 + Control) + t^2 * P1
        float invT = 1.0f - t;
        float t2 = t * t;
        float invT2 = invT * invT;
        
        std::array<float, 3> result;
        for(int i=0; i<3; ++i) {
            float p0 = v0.position[i];
            float p1 = v1.position[i];
            float c = v0.controlPoint[i]; // Control point is usually RELATIVE or ABSOLUTE?
            // EffectElementBase.cpp: "(rPrevEffectPosition.m_vecPosition + rPrevEffectPosition.m_vecControlPoint)"
            // This implies m_vecControlPoint is RELATIVE to the start position.
            float pControl = p0 + c; 
            
            result[i] = p0 * invT2 + pControl * 2.0f * invT * t + p1 * t2;
        }
        return result;
    } 
    else { // Linear (Direct)
        return {
            v0.position[0] + (v1.position[0] - v0.position[0]) * t,
            v0.position[1] + (v1.position[1] - v0.position[1]) * t,
            v0.position[2] + (v1.position[2] - v0.position[2]) * t
        };
    }
}

// Free function wrapper for sampleTimeEventVec3
std::array<float, 3> sampleTimeEventVec3(const TimeEventVec3& event, float time) {
    return event.sample(time);
}

// MseLoader implementation
bool MseLoader::load(const std::string& filepath) {
    m_filePath = filepath;
    m_data = MseData();
    m_error.clear();
    
    std::ifstream file(filepath);
    if (!file.is_open()) {
        m_error = "Failed to open file: " + filepath;
        return false;
    }
    
    return parse(file);
}

bool MseLoader::loadFromString(const std::string& content) {
    m_filePath.clear();
    m_data = MseData();
    m_error.clear();
    
    std::istringstream stream(content);
    return parse(stream);
}

bool MseLoader::parse(std::istream& stream) {
    std::string line;
    
    while (std::getline(stream, line)) {
        trimString(line);
        if (line.empty()) continue;
        
        auto parts = splitString(line);
        if (parts.empty()) continue;
        
        if (parts[0] == "BoundingSphereRadius" && parts.size() >= 2) {
            m_data.boundingSphereRadius = std::stof(parts[1]);
        }
        else if (parts[0] == "BoundingSpherePosition" && parts.size() >= 4) {
            m_data.boundingSpherePosition[0] = std::stof(parts[1]);
            m_data.boundingSpherePosition[1] = std::stof(parts[2]);
            m_data.boundingSpherePosition[2] = std::stof(parts[3]);
        }
        else if (line.find("Group Particle") != std::string::npos) {
            ParticleData particle;
            if (!parseParticleGroup(stream, particle)) {
                return false;
            }
            m_data.particles.push_back(std::move(particle));
        }
        else if (line.find("Group Mesh") != std::string::npos) {
            MeshData mesh;
            if (!parseMeshGroup(stream, mesh)) {
                return false;
            }
            m_data.meshes.push_back(std::move(mesh));
        }
        else if (line.find("Group Light") != std::string::npos) {
            LightData light;
            if (!parseLightGroup(stream, light)) {
                return false;
            }
            m_data.lights.push_back(std::move(light));
        }

    }
    
    return true;
}

bool MseLoader::parseParticleGroup(std::istream& stream, ParticleData& particle) {
    std::string line;
    int braceCount = 0;
    bool foundOpenBrace = false;
    
    while (std::getline(stream, line)) {
        trimString(line);
        if (line.empty()) continue;
        
        // Track braces
        for (char c : line) {
            if (c == '{') {
                braceCount++;
                foundOpenBrace = true;
            }
            else if (c == '}') {
                braceCount--;
            }
        }
        
        // If we've found the opening brace and now at 0, we're done
        if (foundOpenBrace && braceCount == 0) {
            return true;
        }
        
        auto parts = splitString(line);
        if (parts.empty()) continue;
        
        if (parts[0] == "StartTime" && parts.size() >= 2) {
            particle.startTime = std::stof(parts[1]);
        }
        else if (parts[0] == "List" && parts.size() >= 2) {
            if (parts[1] == "TimeEventPosition") {
                parseTimeEventPositionList(stream, particle.timeEventPosition);
            }
        }
        else if (line.find("Group EmitterProperty") != std::string::npos) {
            if (!parseEmitterProperty(stream, particle)) {
                return false;
            }
        }
        else if (line.find("Group ParticleProperty") != std::string::npos) {
            if (!parseParticleProperty(stream, particle)) {
                return false;
            }
        }
    }
    
    return true;
}

bool MseLoader::parseEmitterProperty(std::istream& stream, ParticleData& particle) {
    std::string line;
    int braceCount = 0;
    bool foundOpenBrace = false;
    
    while (std::getline(stream, line)) {
        trimString(line);
        if (line.empty()) continue;
        
        for (char c : line) {
            if (c == '{') {
                braceCount++;
                foundOpenBrace = true;
            }
            else if (c == '}') {
                braceCount--;
            }
        }
        
        if (foundOpenBrace && braceCount == 0) {
            return true;
        }
        
        auto parts = splitString(line);
        if (parts.empty()) continue;
        
        if (parts[0] == "MaxEmissionCount" && parts.size() >= 2) {
            particle.maxEmissionCount = std::stoi(parts[1]);
        }
        else if (parts[0] == "CycleLength" && parts.size() >= 2) {
            particle.cycleLength = std::stof(parts[1]);
        }
        else if (parts[0] == "CycleLoopEnable" && parts.size() >= 2) {
            particle.cycleLoopEnable = (std::stoi(parts[1]) != 0);
        }
        else if (parts[0] == "LoopCount" && parts.size() >= 2) {
            particle.loopCount = std::stoi(parts[1]);
        }
        else if (parts[0] == "EmitterShape" && parts.size() >= 2) {
            particle.emitterShape = static_cast<EmitterShape>(std::stoi(parts[1]));
        }
        else if (parts[0] == "EmitterAdvancedType" && parts.size() >= 2) {
            particle.emitterAdvancedType = static_cast<EmitterAdvancedType>(std::stoi(parts[1]));
        }
        else if (parts[0] == "EmitterEmitFromEdgeFlag" && parts.size() >= 2) {
            particle.emitterEmitFromEdgeFlag = (std::stoi(parts[1]) != 0);
        }
        else if (parts[0] == "EmittingRadius" && parts.size() >= 2) {
            particle.emittingRadius = std::stof(parts[1]);
        }
        else if (parts[0] == "EmittingSize" && parts.size() >= 4) {
            particle.emittingSize[0] = std::stof(parts[1]);
            particle.emittingSize[1] = std::stof(parts[2]);
            particle.emittingSize[2] = std::stof(parts[3]);
        }
        else if (parts[0] == "EmittingDirection" && parts.size() >= 4) {
            particle.emittingDirection[0] = std::stof(parts[1]);
            particle.emittingDirection[1] = std::stof(parts[2]);
            particle.emittingDirection[2] = std::stof(parts[3]);
        }
        else if (parts[0] == "List" && parts.size() >= 2) {
            if (parts[1] == "TimeEventEmittingSize") {
                parseTimeEventList(stream, particle.timeEventEmittingSize);
            }
            else if (parts[1] == "TimeEventEmittingAngularVelocity") {
                parseTimeEventList(stream, particle.timeEventEmittingAngularVelocity);
            }
            else if (parts[1] == "TimeEventEmittingDirectionX") {
                parseTimeEventList(stream, particle.timeEventEmittingDirectionX);
            }
            else if (parts[1] == "TimeEventEmittingDirectionY") {
                parseTimeEventList(stream, particle.timeEventEmittingDirectionY);
            }
            else if (parts[1] == "TimeEventEmittingDirectionZ") {
                parseTimeEventList(stream, particle.timeEventEmittingDirectionZ);
            }
            else if (parts[1] == "TimeEventEmittingVelocity") {
                parseTimeEventList(stream, particle.timeEventEmittingVelocity);
            }
            else if (parts[1] == "TimeEventEmissionCountPerSecond") {
                parseTimeEventList(stream, particle.timeEventEmissionCountPerSecond);
            }
            else if (parts[1] == "TimeEventLifeTime") {
                parseTimeEventList(stream, particle.timeEventLifeTime);
            }
            else if (parts[1] == "TimeEventSizeX") {
                parseTimeEventList(stream, particle.timeEventSizeX);
            }
            else if (parts[1] == "TimeEventSizeY") {
                parseTimeEventList(stream, particle.timeEventSizeY);
            }
        }
    }
    
    return true;
}

bool MseLoader::parseParticleProperty(std::istream& stream, ParticleData& particle) {
    std::string line;
    int braceCount = 0;
    bool foundOpenBrace = false;
    
    while (std::getline(stream, line)) {
        trimString(line);
        if (line.empty()) continue;
        
        for (char c : line) {
            if (c == '{') {
                braceCount++;
                foundOpenBrace = true;
            }
            else if (c == '}') {
                braceCount--;
            }
        }
        
        if (foundOpenBrace && braceCount == 0) {
            return true;
        }
        
        auto parts = splitString(line);
        if (parts.empty()) continue;
        
        if (parts[0] == "SrcBlendType" && parts.size() >= 2) {
            particle.srcBlendType = static_cast<BlendType>(std::stoi(parts[1]));
        }
        else if (parts[0] == "DestBlendType" && parts.size() >= 2) {
            particle.destBlendType = static_cast<BlendType>(std::stoi(parts[1]));
        }
        else if (parts[0] == "ColorOperationType" && parts.size() >= 2) {
            particle.colorOperationType = std::stoi(parts[1]);
        }
        else if (parts[0] == "BillboardType" && parts.size() >= 2) {
            particle.billboardType = static_cast<BillboardType>(std::stoi(parts[1]));
        }
        else if (parts[0] == "RotationType" && parts.size() >= 2) {
            particle.rotationType = static_cast<RotationType>(std::stoi(parts[1]));
        }
        else if (parts[0] == "RotationSpeed" && parts.size() >= 2) {
            particle.rotationSpeed = std::stof(parts[1]);
        }
        else if (parts[0] == "RotationRandomStartingBegin" && parts.size() >= 2) {
            particle.rotationRandomStartingBegin = std::stoi(parts[1]);
        }
        else if (parts[0] == "RotationRandomStartingEnd" && parts.size() >= 2) {
            particle.rotationRandomStartingEnd = std::stoi(parts[1]);
        }
        else if (parts[0] == "AttachEnable" && parts.size() >= 2) {
            particle.attachEnable = (std::stoi(parts[1]) != 0);
        }
        else if (parts[0] == "StretchEnable" && parts.size() >= 2) {
            particle.stretchEnable = (std::stoi(parts[1]) != 0);
        }
        else if (parts[0] == "TexAniType" && parts.size() >= 2) {
            particle.texAniType = static_cast<TexAniType>(std::stoi(parts[1]));
        }
        else if (parts[0] == "TexAniDelay" && parts.size() >= 2) {
            particle.texAniDelay = std::stof(parts[1]);
        }
        else if (parts[0] == "TexAniRandomStartFrameEnable" && parts.size() >= 2) {
            particle.texAniRandomStartFrameEnable = (std::stoi(parts[1]) != 0);
        }
        else if (parts[0] == "List" && parts.size() >= 2) {
            if (parts[1] == "TimeEventGravity") {
                parseTimeEventList(stream, particle.timeEventGravity);
            }
            else if (parts[1] == "TimeEventAirResistance") {
                parseTimeEventList(stream, particle.timeEventAirResistance);
            }
            else if (parts[1] == "TimeEventScaleX") {
                parseTimeEventList(stream, particle.timeEventScaleX);
            }
            else if (parts[1] == "TimeEventScaleY") {
                parseTimeEventList(stream, particle.timeEventScaleY);
            }
            else if (parts[1] == "TimeEventColorRed") {
                parseTimeEventList(stream, particle.timeEventColorRed);
            }
            else if (parts[1] == "TimeEventColorGreen") {
                parseTimeEventList(stream, particle.timeEventColorGreen);
            }
            else if (parts[1] == "TimeEventColorBlue") {
                parseTimeEventList(stream, particle.timeEventColorBlue);
            }
            else if (parts[1] == "TimeEventAlpha") {
                parseTimeEventList(stream, particle.timeEventAlpha);
            }
            else if (parts[1] == "TimeEventRotation") {
                parseTimeEventList(stream, particle.timeEventRotation);
            }
            else if (parts[1] == "TextureFiles") {
                parseTextureFilesList(stream, particle.textureFiles);
            }
        }
    }
    
    return true;
}

bool MseLoader::parseTimeEventList(std::istream& stream, TimeEvent& event) {
    std::string line;
    
    // Find opening brace
    while (std::getline(stream, line)) {
        trimString(line);
        if (line == "{") break;
        if (line.find("{") != std::string::npos) break;
    }
    
    // Parse entries until closing brace
    while (std::getline(stream, line)) {
        trimString(line);
        if (line.empty()) continue;
        if (line == "}" || line.find("}") != std::string::npos) {
            return true;
        }
        
        auto parts = splitString(line);
        if (parts.size() >= 2) {
            try {
                float time = std::stof(parts[0]);
                float value = std::stof(parts[1]);
                event.values[time] = value;
            }
            catch (...) {
                // Skip invalid entries
            }
        }
    }
    
    return true;
}

bool MseLoader::parseTimeEventPositionList(std::istream& stream, TimeEventVec3& event) {
    std::string line;
    
    // Find opening brace
    while (std::getline(stream, line)) {
        trimString(line);
        if (line == "{") break;
        if (line.find("{") != std::string::npos) break;
    }
    
    // Parse entries until closing brace
    while (std::getline(stream, line)) {
        trimString(line);
        if (line.empty()) continue;
        if (line == "}" || line.find("}") != std::string::npos) {
            return true;
        }
        
        // Format: time "MOVING_TYPE_DIRECT" x y z
        // Or: time "MOVING_TYPE_BEZIER_CURVE" x y z cx cy cz
        auto parts = splitString(line);
        if (parts.size() >= 5) {
            try {
                float time = std::stof(parts[0]);
                std::string type = parts[1];
                
                TimeEventPosition pos;
                pos.position[0] = std::stof(parts[2]);
                pos.position[1] = std::stof(parts[3]);
                pos.position[2] = std::stof(parts[4]);
                
                if (type == "MOVING_TYPE_BEZIER_CURVE" && parts.size() >= 8) {
                    pos.movingType = 1;
                    pos.controlPoint[0] = std::stof(parts[5]);
                    pos.controlPoint[1] = std::stof(parts[6]);
                    pos.controlPoint[2] = std::stof(parts[7]);
                } else {
                    pos.movingType = 0;
                }
                
                event.values[time] = pos;
            }
            catch (...) {
                // Skip invalid entries
            }
        }
    }
    
    return true;
}

bool MseLoader::parseTextureFilesList(std::istream& stream, std::vector<std::string>& textures) {
    std::string line;
    
    // Find opening brace
    while (std::getline(stream, line)) {
        trimString(line);
        if (line == "{") break;
        if (line.find("{") != std::string::npos) break;
    }
    
    // Parse entries until closing brace
    while (std::getline(stream, line)) {
        trimString(line);
        if (line.empty()) continue;
        if (line == "}" || line.find("}") != std::string::npos) {
            return true;
        }
        
        // Remove quotes
        std::string texture = line;
        if (!texture.empty() && texture.front() == '"') {
            texture = texture.substr(1);
        }
        if (!texture.empty() && texture.back() == '"') {
            texture.pop_back();
        }
        
        if (!texture.empty()) {
            textures.push_back(texture);
        }
    }
    
    return true;
}

void MseLoader::trimString(std::string& str) {
    // Trim leading whitespace
    auto start = std::find_if_not(str.begin(), str.end(), [](unsigned char c) {
        return std::isspace(c);
    });
    str.erase(str.begin(), start);
    
    // Trim trailing whitespace
    auto end = std::find_if_not(str.rbegin(), str.rend(), [](unsigned char c) {
        return std::isspace(c);
    }).base();
    str.erase(end, str.end());
}

std::vector<std::string> MseLoader::splitString(const std::string& str) {
    std::vector<std::string> result;
    std::istringstream iss(str);
    std::string token;
    
    while (iss >> token) {
        // Handle quoted strings
        if (!token.empty() && token.front() == '"') {
            std::string quoted = token;
            while (!token.empty() && token.back() != '"' && iss >> token) {
                quoted += " " + token;
            }
            result.push_back(quoted);
        }
        else {
            result.push_back(token);
        }
    }
    
    return result;
}

std::vector<std::string> MseLoader::getTextureSearchPaths(
    const std::string& basePath, 
    const std::string& textureFile) 
{
    std::vector<std::string> paths;
    
    // Extract just filename from textureFile (remove any path)
    std::string filename = textureFile;
    size_t lastSlash = textureFile.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        filename = textureFile.substr(lastSlash + 1);
    }
    
    // Direct path
    paths.push_back(basePath + "/" + textureFile);
    paths.push_back(basePath + "\\" + textureFile);
    paths.push_back(basePath + "/" + filename);
    paths.push_back(basePath + "\\" + filename);
    
    // Common subdirectories
    paths.push_back(basePath + "/Textures/" + filename);
    paths.push_back(basePath + "/Effects/" + filename);
    paths.push_back(basePath + "/Particles/" + filename);
    paths.push_back(basePath + "/effect/" + filename);
    paths.push_back(basePath + "/texture/" + filename);
    
    // Recursive search in all subdirectories
    try {
        std::filesystem::path baseDir(basePath);
        if (std::filesystem::exists(baseDir) && std::filesystem::is_directory(baseDir)) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(baseDir)) {
                if (entry.is_regular_file()) {
                    std::string entryFilename = entry.path().filename().string();
                    // Case-insensitive comparison
                    std::string lowerEntry = entryFilename;
                    std::string lowerTarget = filename;
                    std::transform(lowerEntry.begin(), lowerEntry.end(), lowerEntry.begin(), ::tolower);
                    std::transform(lowerTarget.begin(), lowerTarget.end(), lowerTarget.begin(), ::tolower);
                    
                    if (lowerEntry == lowerTarget) {
                        paths.push_back(entry.path().string());
                    }
                }
            }
        }
    }
    catch (...) {
        // Ignore filesystem errors
    }
    
    // Just the filename (for relative paths)
    paths.push_back(textureFile);
    paths.push_back(filename);
    
    return paths;
}

// ========== Mesh Group Parsing ==========
bool MseLoader::parseMeshGroup(std::istream& stream, MeshData& mesh) {
    std::string line;
    int braceCount = 0;
    bool foundOpenBrace = false;
    int meshElementIndex = -1;
    
    while (std::getline(stream, line)) {
        trimString(line);
        if (line.empty()) continue;
        
        // Track braces
        for (char c : line) {
            if (c == '{') {
                braceCount++;
                foundOpenBrace = true;
            }
            else if (c == '}') {
                braceCount--;
            }
        }
        
        if (foundOpenBrace && braceCount == 0) {
            return true;
        }
        
        auto parts = splitString(line);
        if (parts.empty()) continue;
        
        // Group-level properties
        if (parts[0] == "StartTime" && parts.size() >= 2) {
            mesh.startTime = std::stof(parts[1]);
        }
        else if (parts[0] == "List" && parts.size() >= 2) {
            if (parts[1] == "TimeEventPosition") {
                parseTimeEventPositionList(stream, mesh.timeEventPosition);
            }
        }
        // Mesh Script properties (from CEffectMeshScript::OnLoadScript)
        else if (parts[0] == "MeshFileName" && parts.size() >= 2) {
            // Remove quotes if present
            mesh.meshFileName = parts[1];
            if (mesh.meshFileName.size() >= 2 && mesh.meshFileName[0] == '"') {
                mesh.meshFileName = mesh.meshFileName.substr(1, mesh.meshFileName.size() - 2);
            }
        }
        else if (parts[0] == "MeshAnimationLoopEnable" && parts.size() >= 2) {
            mesh.meshAnimationLoopEnable = std::stoi(parts[1]) != 0;
        }
        else if (parts[0] == "MeshAnimationLoopCount" && parts.size() >= 2) {
            mesh.meshAnimationLoopCount = std::stoi(parts[1]);
        }
        else if (parts[0] == "MeshAnimationFrameDelay" && parts.size() >= 2) {
            mesh.meshAnimationFrameDelay = std::stof(parts[1]);
        }
        else if (parts[0] == "MeshElementCount" && parts.size() >= 2) {
            int count = std::stoi(parts[1]);
            mesh.meshElements.resize(count);
            meshElementIndex = -1;
        }
        // Mesh element sub-group (from OnLoadScript loop)
        else if (line.find("Group") != std::string::npos && 
                 line.find("MeshElement") != std::string::npos) {
            meshElementIndex++;
            if (meshElementIndex >= 0 && 
                meshElementIndex < static_cast<int>(mesh.meshElements.size())) {
                // Parse mesh element properties
                parseMeshElement(stream, mesh.meshElements[meshElementIndex]);
            }
        }
    }
    
    return true;
}

bool MseLoader::parseMeshElement(std::istream& stream, MeshElementData& element) {
    std::string line;
    int braceCount = 0;
    bool foundOpenBrace = false;
    
    while (std::getline(stream, line)) {
        trimString(line);
        if (line.empty()) continue;
        
        for (char c : line) {
            if (c == '{') {
                braceCount++;
                foundOpenBrace = true;
            }
            else if (c == '}') {
                braceCount--;
            }
        }
        
        if (foundOpenBrace && braceCount == 0) {
            return true;
        }
        
        auto parts = splitString(line);
        if (parts.empty()) continue;
        
        // From CEffectMeshScript::OnLoadScript
        if (parts[0] == "BillboardType" && parts.size() >= 2) {
            element.billboardType = std::stoi(parts[1]);
        }
        else if (parts[0] == "BlendingEnable" && parts.size() >= 2) {
            element.blendingEnable = std::stoi(parts[1]) != 0;
        }
        else if (parts[0] == "BlendingSrcType" && parts.size() >= 2) {
            element.blendingSrcType = std::stoi(parts[1]);
        }
        else if (parts[0] == "BlendingDestType" && parts.size() >= 2) {
            element.blendingDestType = std::stoi(parts[1]);
        }
        else if (parts[0] == "TextureAlphaEnable" && parts.size() >= 2) {
            element.textureAlphaEnable = std::stoi(parts[1]) != 0;
        }
        else if (parts[0] == "ColorOperationType" && parts.size() >= 2) {
            element.colorOperationType = std::stoi(parts[1]);
        }
        else if (parts[0] == "ColorFactor" && parts.size() >= 5) {
            element.colorFactor[0] = std::stof(parts[1]);
            element.colorFactor[1] = std::stof(parts[2]);
            element.colorFactor[2] = std::stof(parts[3]);
            element.colorFactor[3] = std::stof(parts[4]);
        }
        else if (parts[0] == "TextureAnimationLoopEnable" && parts.size() >= 2) {
            element.textureAnimationLoopEnable = std::stoi(parts[1]) != 0;
        }
        else if (parts[0] == "TextureAnimationFrameDelay" && parts.size() >= 2) {
            element.textureAnimationFrameDelay = std::stof(parts[1]);
        }
        else if (parts[0] == "TextureAnimationStartFrame" && parts.size() >= 2) {
            element.textureAnimationStartFrame = std::stoi(parts[1]);
        }
        else if (parts[0] == "List") {
            if (parts.size() >= 2 && parts[1] == "TimeEventAlpha") {
                parseTimeEventList(stream, element.timeEventAlpha);
            }
        }
    }
    
    return true;
}

// ========== Light Group Parsing ==========
bool MseLoader::parseLightGroup(std::istream& stream, LightData& light) {
    std::string line;
    int braceCount = 0;
    bool foundOpenBrace = false;
    
    while (std::getline(stream, line)) {
        trimString(line);
        if (line.empty()) continue;
        
        for (char c : line) {
            if (c == '{') {
                braceCount++;
                foundOpenBrace = true;
            }
            else if (c == '}') {
                braceCount--;
            }
        }
        
        if (foundOpenBrace && braceCount == 0) {
            return true;
        }
        
        auto parts = splitString(line);
        if (parts.empty()) continue;
        
        if (parts[0] == "StartTime" && parts.size() >= 2) {
            light.startTime = std::stof(parts[1]);
        }
        else if (parts[0] == "Duration" && parts.size() >= 2) {
            light.duration = std::stof(parts[1]);
        }
        else if (parts[0] == "LoopFlag" && parts.size() >= 2) {
            light.loopFlag = std::stoi(parts[1]) != 0;
        }
        else if (parts[0] == "LoopCount" && parts.size() >= 2) {
            light.loopCount = std::stoi(parts[1]);
        }
        else if (parts[0] == "AmbientColor" && parts.size() >= 5) {
            light.ambientColor[0] = std::stof(parts[1]);
            light.ambientColor[1] = std::stof(parts[2]);
            light.ambientColor[2] = std::stof(parts[3]);
            light.ambientColor[3] = std::stof(parts[4]);
        }
        else if (parts[0] == "DiffuseColor" && parts.size() >= 5) {
            light.diffuseColor[0] = std::stof(parts[1]);
            light.diffuseColor[1] = std::stof(parts[2]);
            light.diffuseColor[2] = std::stof(parts[3]);
            light.diffuseColor[3] = std::stof(parts[4]);
        }
        else if (parts[0] == "MaxRange" && parts.size() >= 2) {
            light.maxRange = std::stof(parts[1]);
        }
        else if (parts[0] == "Attenuation0" && parts.size() >= 2) {
            light.attenuation0 = std::stof(parts[1]);
        }
        else if (parts[0] == "Attenuation1" && parts.size() >= 2) {
            light.attenuation1 = std::stof(parts[1]);
        }
        else if (parts[0] == "Attenuation2" && parts.size() >= 2) {
            light.attenuation2 = std::stof(parts[1]);
        }
        else if (parts[0] == "List") {
            if (parts.size() >= 2 && parts[1] == "TimeEventPosition") {
                parseTimeEventPositionList(stream, light.timeEventPosition);
            }
            else if (parts.size() >= 2 && parts[1] == "TimeEventRange") {
                parseTimeEventList(stream, light.timeEventRange);
            }
        }
    }
    
    return true;
}

} // namespace Mse


