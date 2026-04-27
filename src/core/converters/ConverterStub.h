#pragma once

// Stub header for Linux 
// Lithtech SDK Does not Support Linux.
// TODO: Just check out if there is a another  way (probably no).

#ifndef ENABLE_LITHTECH

#include <string>
#include <vector>
#include <functional>

class Converter {
public:
    Converter() = default;
    ~Converter() = default;
    
    struct ConverterSetting {
        bool SingleAnimFile = false;
        bool IgnoreMeshes = false;
        bool IgnoreAnimations = false;
    };
    
    void SetExportFormat(const std::string&) {}
    void SetConvertSetting(const ConverterSetting&) {}
    int ConvertSingleLTBFile(const std::string&, const std::string&) { return -1; }
};

#define CONVERT_RET_OK 0
#define CONVERT_RET_INVALID_INPUT_FILE -1
#define CONVERT_RET_LOADING_MODEL_FAILED -2
#define CONVERT_RET_DECODING_FAILED -3

#endif // !ENABLE_LITHTECH
