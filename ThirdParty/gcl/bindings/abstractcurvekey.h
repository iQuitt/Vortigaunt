#pragma once

#include "gcl/importer/grannyformat.h"

#include <fbxsdk.h>

#include <vector>
#include "granny.h"

namespace GCL::Bindings {

///
/// \brief The AbstractCurveKey class.
///
class AbstractCurveKey {
public:
    ///
    /// \brief Constructer with data initialization.
    /// \param data Granny curve data
    ///
    AbstractCurveKey(granny_curve2 data);

    ///
    /// \brief Returns the time of this key.
    /// \return Key time
    ///
    FbxTime getTime();

    ///
    /// \brief Sets the time of this key.
    /// \param time Animation key time
    ///
    void setTime(FbxDouble time);

    ///
    /// \brief Returns the value of this key.
    /// \return Key value
    ///
    FbxDouble3 getValue();

    ///
    /// \brief Sets the value of this key.
    /// \param Animation key value
    ///
    void setValue(FbxDouble3 value);

protected:
    ///
    /// \brief Granny curve data
    ///
    granny_curve2 m_data;

    ///
    /// \brief Time of this key.
    ///
    FbxTime m_time;

    ///
    /// \brief Value of this key at a specific time, see m_time.
    ///
    FbxDouble3 m_value;
};

} // namespace GCL::Bindings
