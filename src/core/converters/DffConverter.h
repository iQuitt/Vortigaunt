// Copyright (C) 2026 iQuit (https://github.com/iQuitt)
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#pragma once

#include "IConverter.h"

#include <string>
#include <vector>

// GTA III / Vice City / San Andreas models (RenderWare DFF) with their TXD textures
struct DffConvertOptions
{
    float scale = 39.37f;    // GTA metres to GoldSrc units (inches), the size of a CS player
    bool writeQC = true;
    bool damagedParts = true;  // the _dam parts as a second choice of each _ok bodygroup
    bool extras = true;        // extra1..n as bodygroups, empty by default
    std::string gameFolder;  // GTA folder for vehicles.ide, carcols.dat and generic/vehicle.txd; found from the file when empty
};

struct DffConvertReport
{
    bool ok = false;
    std::string error;
    std::string smdPath;
    std::string qcPath;
    int bones = 0;
    int triangles = 0;
    int textures = 0;
    std::vector<std::string> missingTextures;
    std::vector<std::string> bodyGroups;
    int controllers = 0;  // bikes: 0 forks, 1 handlebars, 2 front wheel, 3 rear wheel
    std::vector<std::string> attachments;  // GTA's frames by attachment number: exhaust, headlights, taillights, ped_frontseat
};

// One texture of a TXD as 32 bit RGBA
struct TxdImage
{
    std::string name;
    int width = 0;
    int height = 0;
    bool alpha = false;
    std::vector<uint8_t> rgba;
};

class DffConverter : public IConverter
{
public:
    static bool IsSupportedFile(const std::string& path);
    static std::vector<TxdImage> ReadTxd(const std::string& path, std::string* error = nullptr);
    static std::vector<TxdImage> ReadTxd(const std::vector<uint8_t>& data, std::string* error = nullptr);
    // the GTA folder above a file: the one with models/gta3.img and data/
    static std::string FindGameFolder(const std::string& path);

    DffConvertReport Convert(const std::string& inputPath, const std::string& outputDir);

    void SetOptions(const DffConvertOptions& options) { m_options = options; }
    const DffConvertOptions& Options() const { return m_options; }

    const char* Name() const override { return "DFF"; }
    bool CanConvert(const std::string& path) const override { return IsSupportedFile(path); }
    ConvertResult ConvertFile(const std::string& inputPath, const std::string& outputDir) override;

private:
    DffConvertOptions m_options;
};
