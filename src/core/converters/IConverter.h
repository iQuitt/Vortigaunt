// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>

struct ConvertResult
{
    bool ok = false;
    std::string error;
    std::string outputFolder;
    std::vector<std::string> qcFiles;
};

struct ConvertAssets
{
    int total = 0;
    int converted = 0;
    bool cancelled = false;
    std::vector<std::pair<std::string, std::string>> failed;
};

using ConvertProgress = std::function<bool(int index, int total, const std::string& inputPath, const ConvertResult& result)>;

class IConverter
{
public:
    virtual ~IConverter() = default;

    virtual const char* Name() const = 0;

    virtual bool CanConvert(const std::string& path) const = 0;

    virtual ConvertResult ConvertFile(const std::string& inputPath, const std::string& outputDir) = 0;
};

ConvertAssets ConvertAll(IConverter& converter, const std::string& inputPath, const std::string& outputDir, const ConvertProgress& progress = {});
