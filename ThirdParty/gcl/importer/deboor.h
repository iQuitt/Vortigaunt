#pragma once

#include "gcl/importer/grannyformat.h"

#include <fbxsdk.h>

#include <memory>
#include <vector>

namespace GCL::Importer {

using namespace std;

// De Boor's algorithm to evaluate to interpolate keyframes to bézier spline.
// Thanks to https://github.com/Arbos/nwn2mdk/blob/master/mdb2fbx/export_gr2.cpp

///
/// \brief padded_knots
/// \param knots
/// \param degree
/// \return
///
vector<float> padded_knots(const vector<float>& knots, unsigned degree);

///
/// \brief de_boor_position
/// \param degree
/// \param time
/// \param knots
/// \param controls
/// \return
///
FbxDouble3 de_boor_position(unsigned degree, float time, const vector<float>& knots, const vector<FbxDouble3>& controls);

///
/// \brief de_boor_position
/// \param degree
/// \param time
/// \param knots
/// \param controls
/// \return
///
FbxDouble3 de_boor_position(unsigned degree, float time, vector<float>& knots, vector<FbxDouble3>& controls);

///
/// \brief de_boor_rotation
/// \param degree
/// \param time
/// \param knots
/// \param controls
/// \return
///
FbxQuaternion de_boor_rotation(unsigned degree, float time, const vector<float>& knots, const vector<FbxQuaternion>& controls);

///
/// \brief de_boor_rotation
/// \param degree
/// \param time
/// \param knots
/// \param controls
/// \return
///
FbxQuaternion de_boor_rotation(unsigned degree, float time, vector<float>& knots, vector<FbxQuaternion>& controls);

} // namespace GCL::Importer
