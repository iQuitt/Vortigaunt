// Copyright (C) 2026 iQuit (https://github.com/iQuitt)
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#pragma once

#include "core/mdl/StudioModel.h"

#include <string>
#include <vector>

// A submodel with more vertices or normals than GoldSrc draws (kStudioMaxVertices): the engine draws it broken
// or not at all. Normals are counted as the model stores them, once in each mesh that uses them.
struct StudioOversize
{
    int part = 0, model = 0;
    int vertices = 0, normals = 0;
    bool splittable = false;  // the only submodel of its part: what is cut off can go to parts of its own
};

std::vector<StudioOversize> FindOversizedSubModels(const StudioModel& model);

struct SplitResult
{
    int split = 0;      // submodels cut up
    int partsAdded = 0;
    int skipped = 0;    // over the limit but one of several submodels of its part
    std::string error;  // nothing was changed
};

// Cuts each splittable oversized submodel into connected pieces within the limit. The first piece stays where the
// submodel was, the rest become parts of one submodel each after the last part, so the body values the game sets
// pick the same models as before.
SplitResult SplitOversizedSubModels(StudioModel& model);
