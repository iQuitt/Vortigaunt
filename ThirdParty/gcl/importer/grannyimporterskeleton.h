#pragma once

#include "gcl/bindings/bone.h"
#include "gcl/bindings/scene.h"
#include "gcl/importer/grannyformat.h"

#include <vector>

namespace GCL::Importer {

using namespace std;
using namespace GCL::Bindings;

///
/// \brief The GrannyImporterSkeleton class.
///
class GrannyImporterSkeleton {
public:
    ///
    /// \brief Constructor
    /// \param scene Scene which needs to be exported.
    ///
    GrannyImporterSkeleton(Scene::SharedPtr scene);

    ///
    /// \brief Destructor
    ///
    ~GrannyImporterSkeleton();

    ///
    /// \brief Load all bones from the granny model and return.
    /// \param granny_model Granny model
    /// \return Bones of the granny model.
    ///
    vector<Bone::SharedPtr> loadBones(granny_model* granny_model) const;

protected:
    ///
    /// \brief Scene of the importing granny file.
    ///
    Scene::SharedPtr m_scene;
};

} // namespace GCL::Importer
