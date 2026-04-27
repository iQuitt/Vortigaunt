#pragma once

#include "gcl/bindings/abstractcurvekey.h"
#include "gcl/bindings/scene.h"
#include "gcl/bindings/track.h"
#include "gcl/exporter/fbxexportermodule.h"
#include "gcl/importer/grannyformat.h"
#include "gcl/utilities/fbxsdkcommon.h"

#include <fbxsdk.h>

namespace GCL::Exporter {

using namespace std;
using namespace GCL::Bindings;

///
/// \brief The ExporterAnimations class.
///
class FbxExporterAnimation : public FbxExporterModule {
public:
    // Inherit constructor.
    using FbxExporterModule::FbxExporterModule;

    ///
    /// \brief Export animations to the fbx scene.
    ///
    void exportAnimations();

    ///
    /// \brief Export all animation curves for an track to the fbx scene.
    /// \param track Track which should be exported.
    /// \param boneNode Bone node of the track which should be affected by the track curves.
    /// \param animLayer Anim layer for the exporting animation.
    ///
    void exportCurves(Track::SharedPtr track, FbxNode* boneNode, FbxAnimLayer* animLayer);

    ///
    /// \brief Export an curve key to the fbx scene.
    /// \param key Curve key of a track at a specific time of the animation.
    /// \param animCurveX Anim curve for x-axis value.
    /// \param animCurveY Anim curve for y-axis value.
    /// \param animCurveZ Anim curve for z-axis value.
    ///
    void exportCurveKey(
        AbstractCurveKey key,
        FbxAnimCurve* animCurveX,
        FbxAnimCurve* animCurveY,
        FbxAnimCurve* animCurveZ);
};

} // namespace GCL::Exporter
