#pragma once

#include "gcl/bindings/curvepositionkey.h"
#include "gcl/bindings/curverotationkey.h"
#include "gcl/bindings/curvescalekey.h"
#include "gcl/importer/grannyformat.h"

#include <fbxsdk.h>

#include <iostream>
#include <vector>

namespace GCL::Bindings {

using namespace std;

///
/// \brief The Track class.
///
class Track {
public:
    ///
    /// \brief Shared pointer alias
    ///
    using SharedPtr = shared_ptr<Track>;

    ///
    /// \brief Constructer with data initialization.
    /// \param data Granny track data
    ///
    Track(granny_transform_track data);

    ///
    /// \brief Returns the track name.
    /// \return Track name
    ///
    string getName();

    ///
    /// \brief Sets the track name.
    /// \param Track name
    ///
    void setName(string name);

    ///
    /// \brief Returns the keys of the position curve of this track.
    /// \return Position curve keys
    ///
    vector<CurvePositionKey> getPositionKeys();

    ///
    /// \brief Returns the keys of the rotation curve of this track.
    /// \return Rotation curve keys
    ///
    vector<CurveRotationKey> getRotationKeys();

    ///
    /// \brief Returns the keys of the scale curve of this track.
    /// \return Scale curve keys
    ///
    vector<CurveScaleKey> getScaleKeys();

    ///
    /// \brief Add a key to the position curve.
    /// \param Position curve key
    ///
    void addPositionKey(CurvePositionKey key);

    ///
    /// \brief Add a key to the rotation curve.
    /// \param Rotation curve key
    ///
    void addRotationKey(CurveRotationKey key);

    ///
    /// \brief Add a key to the scale curve.
    /// \param Scale curve key
    ///
    void addScaleKey(CurveScaleKey key);

protected:
    ///
    /// \brief Granny data of the track.
    ///
    granny_transform_track m_data;

    ///
    /// \brief Fbx node of the track.
    ///
    FbxNode* m_node = nullptr;

    ///
    /// \brief Track name from granny data.
    ///
    string m_name;

    ///
    /// \brief Keys for the position curve of this track.
    ///
    vector<CurvePositionKey> m_positionKeys;

    ///
    /// \brief Keys for the rotation curve of this track.
    ///
    vector<CurveRotationKey> m_rotationKeys;

    ///
    /// \brief Keys for the scale curve of this track.
    ///
    vector<CurveScaleKey> m_scaleKeys;
};

} // namespace GCL::Bindings
