// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#pragma once

#include <string>
#include <vector>

#include "core/converters/IConverter.h"

class StudioModel;

// This is a converter for GoldSrc weapon models. It can convert a model into a view model (p_) and a world model (w_).
// It can also generate the QC files for compiling the models. The converter can be used in a command-line tool or as a library.

enum class GameDir
{
    // These are the most popular and enduring GoldSrc games. I included these three because I conducted my tests using them.
    // I didn't want to take the risk of including all of them.
    // If there is sufficient demand, other games can be added as well.

    cstrike,
    valve,
    svencoop,
};

enum class WeaponType
{
	Auto,// it detect automatically if the model is a gun or a knife, and use the correct hold pose.
    Gun,
    Knife,
    Grenade,
	Reference, // give a reference model to copy the hold pose from. 
	Dual, // ex: dual pistols, like dual berettas
    Pack,// ex: c4, tripmine 
};

struct WeaponModelOptions
{
    bool makeP = true;
    bool makeW = true;
    bool writeQC = true;

    float scale = 0.0f;
    int mirror = -1;

    GameDir gameDir = GameDir::cstrike;
    WeaponType hold = WeaponType::Auto;
    std::string referenceModel;

    bool upsideDown = false;
    bool keepEffects = false;
};

struct WeaponModelResult
{
    bool ok = false;
    std::string error;

    std::string outputFolder;
    std::string pQcPath; // p_ model path
    std::string wQcPath; // w_ model path

    int triangles = 0;
    int bodies = 0;
    float scale = 1.0f;
    bool scaleFromHands = false;
    bool mirrored = false;
    float length = 0.0f;
    std::string hold;
    bool dual = false;
    bool turnedByShape = false;

    std::vector<std::string> removedHands;
    std::vector<std::string> removedEffects;
    std::vector<std::string> removedParts;
    std::vector<std::string> textures;
    std::vector<std::string> warnings;
};

class WeaponModelConverter : public IConverter
{
public:
    static bool IsViewModelFile(const std::string& path);
    // for each body part, model and mesh of a view model, whether it is the skin of the hands
    static std::vector<std::vector<std::vector<bool>>> FindHandMeshes(const StudioModel& model);

    WeaponModelResult Convert(const std::string& inputPath, const std::string& outputDir, const WeaponModelOptions& options = WeaponModelOptions());

    void SetOptions(const WeaponModelOptions& options) { m_options = options; }

    const char* Name() const override { return "v_ to p_/w_"; }
    bool CanConvert(const std::string& path) const override { return IsViewModelFile(path); }
    ConvertResult ConvertFile(const std::string& inputPath, const std::string& outputDir) override;

private:
    WeaponModelOptions m_options;
};
