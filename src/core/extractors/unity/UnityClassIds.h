#pragma once

#include <cstdint>

// Unity serialized class IDs used by the extractor.
// Reference: https://docs.unity3d.com/Manual/ClassIDReference.html
namespace UnityClass {
    constexpr int32_t GameObject          = 1;
    constexpr int32_t Transform           = 4;
    constexpr int32_t Material            = 21;
    constexpr int32_t MeshRenderer        = 23;
    constexpr int32_t Renderer            = 25;
    constexpr int32_t Texture2D           = 28;
    constexpr int32_t MeshFilter          = 33;
    constexpr int32_t Mesh                = 43;
    constexpr int32_t AnimationClip       = 74;
    constexpr int32_t SkinnedMeshRenderer = 137;
}
