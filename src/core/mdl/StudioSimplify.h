// Copyright (C) 2026 iQuit (https://github.com/iQuitt)
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#pragma once

#include "core/mdl/StudioModel.h"

// triangles of the chosen faces before and after
struct SimplifyResult
{
    int before = 0;
    int after = 0;
};

SimplifyResult SimplifyStudioModel(StudioModel& model, int part, int subModel, int texture, float keep, bool smoothNormals, bool keepJoints = true);

// triangles of the same choice
int CountStudioTriangles(const StudioModel& model, int part, int subModel, int texture);
