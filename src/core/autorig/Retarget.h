// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#pragma once

// Animation retargeting between Biped skeletons: the source's world rotations on the target's bone lengths,
// root movement scaled by leg length.

#include <string>
#include <vector>

namespace retarget
{

struct RetargetResult
{
    bool ok = false;
    std::string error;
    int frames = 0;
    int fps = 30;                      // lower for long motions
    float scale = 0.0f;                // target units for a source unit
    std::vector<std::string> unmatched;  // bones of the target the source has none of
};

// retargets an SMD motion from one rest skeleton to another, written to outPath
RetargetResult Retarget(const std::string& sourceRest, const std::string& sourceMotion, const std::string& targetRest, const std::string& outPath);

// the same for a Metin2 .gr2 motion and character, converted to SMD in workFolder
RetargetResult RetargetGr2Motion(const std::string& characterGr2, const std::string& motionGr2, const std::string& targetRest, const std::string& outPath,
                     const std::string& workFolder);

// adds or replaces "$sequence <name>" in the QC, with an optional activity and weight
bool AddSequence(const std::string& qcPath, const std::string& name, const std::string& animPath, int fps, bool loop, std::string& error,
                 const std::string& activity = {}, int weight = 1);

// a $sequence of a QC
struct QcSequence
{
    std::string name, path, activity;
    int fps = 30;
    bool loop = false;
};

// the SMD of the QC's first body; empty when none
std::string QcBody(const std::string& qcPath);

// the QC's sequences
std::vector<QcSequence> QcSequences(const std::string& qcPath);

}  // namespace retarget
