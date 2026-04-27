#pragma once

#include "gcl/bindings/abstractcurvekey.h"
#include "gcl/importer/grannyformat.h"

#include <vector>

namespace GCL::Bindings {

///
/// \brief The CurvePositionKey class.
///
class CurvePositionKey : public AbstractCurveKey {
public:
    ///
    /// \brief Constructer with data initialization.
    /// \param data Granny curve data
    ///
    CurvePositionKey(granny_curve2 data);
};

} // namespace GCL::Bindings
