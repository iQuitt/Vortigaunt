#pragma once

#include "gcl/bindings/bone.h"
#include "gcl/bindings/scene.h"
#include "gcl/importer/deboor.h"
#include "gcl/importer/grannyformat.h"

namespace GCL::Importer {

using namespace std;
using namespace GCL::Bindings;

///
/// \brief The GrannyImporterAnimation class.
///
class GrannyImporterAnimation {
public:
    ///
    /// \brief Constructor
    /// \param scene Scene which needs to be exported.
    ///
    GrannyImporterAnimation(Scene::SharedPtr scene);

    ///
    /// \brief Destructor
    ///
    virtual ~GrannyImporterAnimation();

    ///
    /// \brief Imports all animations from the granny file as scene animation to the scene.
    /// \param granny_file_info Granny file info
    ///
    void importAnimations(granny_file_info* granny_file_info) const;

public:
    ///
    /// \brief Animation tracks in the scene.
    ///
    vector<granny_transform_track> m_tracks;

    ///
    /// \brief Animations in the scene.
    ///
    vector<granny_animation*> m_animations;

protected:
    ///
    /// \brief Construct a track.
    /// \param animation Animation
    /// \param granny_transform_track Granny transform track
    /// \return Animation tracks of animation.
    ///
    virtual Track::SharedPtr importTrack(Animation::SharedPtr animation, granny_transform_track granny_transform_track) const;

    ///
    /// \brief Imports a scale keys from scale curve.
    /// \param track Granny transform track
    /// \param granny_transform_track Granny transform track
    ///
    void importScaleCurve(Track::SharedPtr track, granny_transform_track granny_transform_track) const;

    ///
    /// \brief Imports a position keys from scale curve.
    /// \param animation Animation
    /// \param track Track
    /// \param granny_transform_track Granny transform track
    ///
    void importPositionCurve(Animation::SharedPtr animation, Track::SharedPtr track, granny_transform_track granny_transform_track) const;

    ///
    /// \brief Imports a rotation keys from scale curve.
    /// \param animation Animation
    /// \param track Track
    /// \param granny_transform_track Granny transform track
    ///
    void importRotationCurve(Animation::SharedPtr animation, Track::SharedPtr track, granny_transform_track granny_transform_track) const;

    ///
    /// \brief Calculcates time to a 30 frames per second rate.
    /// \param time
    /// \return Time at a 30 frames per second rate.
    ///
    double calculateTime(const double time) const;

protected:
    ///
    /// \brief Scene of the importing granny file.
    ///
    Scene::SharedPtr m_scene;
};

} // namespace GCL::Importer
