#include "gcl/exporter/fbxexporterskeleton.h"
#include "granny.h"

namespace GCL::Exporter {

void FbxExporterSkeleton::exportBones(Model::SharedPtr model)
{
    for (auto bone : model->getBones()) {
        exportBone(model, bone);
    }
}

void FbxExporterSkeleton::exportBone(Model::SharedPtr model, Bone::SharedPtr bone)
{
    auto granny_bone = bone->getData();
    auto parentIndex = granny_bone.ParentIndex;

    // Calculate local transformation for bone node.

    auto localTransform = bone->getData().LocalTransform;

    auto finalTransform = FbxAMatrix(
        FbxDouble3(0, 0, 0),
        FbxDouble3(0, 0, 0),
        FbxDouble3(1, 1, 1));

    if (localTransform.Flags & granny_transform_flags::GrannyHasPosition) {
        finalTransform.SetT(FbxVector4(
            static_cast<double>(localTransform.Position[0]),
            static_cast<double>(localTransform.Position[1]),
            static_cast<double>(localTransform.Position[2])));
    }

    if (localTransform.Flags & granny_transform_flags::GrannyHasScaleShear) {
        finalTransform.SetS(FbxDouble3(
            static_cast<double>(localTransform.ScaleShear[0][0]),
            static_cast<double>(localTransform.ScaleShear[1][1]),
            static_cast<double>(localTransform.ScaleShear[2][2])));
    }

    if (localTransform.Flags & granny_transform_flags::GrannyHasOrientation) {
        finalTransform.SetQ(FbxQuaternion(
            static_cast<double>(localTransform.Orientation[0]),
            static_cast<double>(localTransform.Orientation[1]),
            static_cast<double>(localTransform.Orientation[2]),
            static_cast<double>(localTransform.Orientation[3])));

        if (localTransform.Flags & granny_transform_flags::GrannyHasScaleShear && (localTransform.ScaleShear[0][0] < 0 || localTransform.ScaleShear[1][1] < 0 || localTransform.ScaleShear[2][2] < 0)) {
            finalTransform.MultSM(FbxDouble3(
                static_cast<double>(localTransform.ScaleShear[0][0]),
                static_cast<double>(localTransform.ScaleShear[1][1]),
                static_cast<double>(localTransform.ScaleShear[2][2])));
        }
    }

    // Apply initial placement to the transformation of the root bone.

    if (parentIndex == GrannyNoParentBone) {
        auto initialPlacement = model->getData()->InitialPlacement;
        auto initialTransform = FbxAMatrix(
            FbxDouble3(0, 0, 0),
            FbxDouble3(0, 0, 0),
            FbxDouble3(1, 1, 1));

        if (initialPlacement.Flags & granny_transform_flags::GrannyHasPosition) {
            initialTransform.SetT(FbxDouble3(
                static_cast<double>(initialPlacement.Position[0]),
                static_cast<double>(initialPlacement.Position[1]),
                static_cast<double>(initialPlacement.Position[2])));
        }

        if (initialPlacement.Flags & granny_transform_flags::GrannyHasOrientation) {
            initialTransform.SetQ(FbxQuaternion(
                static_cast<double>(initialPlacement.Orientation[0]),
                static_cast<double>(initialPlacement.Orientation[1]),
                static_cast<double>(initialPlacement.Orientation[2]),
                static_cast<double>(initialPlacement.Orientation[3])));
        }

        if (initialPlacement.Flags & granny_transform_flags::GrannyHasScaleShear) {
            initialTransform.SetS(FbxDouble3(
                static_cast<double>(initialPlacement.ScaleShear[0][0]),
                static_cast<double>(initialPlacement.ScaleShear[1][1]),
                static_cast<double>(initialPlacement.ScaleShear[2][2])));
        }

        finalTransform = initialTransform * finalTransform;
    }

    // Create the bone node for the fbx scene.

    auto boneName = granny_bone.Name;
    auto boneNode = FbxNode::Create(m_fbxScene, boneName);

    bone->setNode(boneNode);

    // Set local bone transformation.
    boneNode->LclTranslation.Set(finalTransform.GetT());
    boneNode->LclRotation.Set(finalTransform.GetR());

    if (localTransform.Flags & granny_transform_flags::GrannyHasScaleShear) {
        // Fix a issue with negative scaling because fbx will not going to support this..
        // So what does the fix? Let me explain.. The fix multiplies the granny scalar matrix
        // with the current local transform and then it applies the value of the rotation matrix
        // of the local transformation after the multiplication to the bone as rotation value.
        if (localTransform.ScaleShear[0][0] < 0 || localTransform.ScaleShear[1][1] < 0 || localTransform.ScaleShear[2][2] < 0) {
            if (!(localTransform.Flags & granny_transform_flags::GrannyHasOrientation)) {
                finalTransform.MultSM(finalTransform.GetS());
                boneNode->LclRotation.Set(finalTransform.GetR());
            }

            boneNode->LclScaling.Set(FbxDouble3(
                static_cast<double>(localTransform.ScaleShear[2][2]),
                static_cast<double>(localTransform.ScaleShear[2][2]),
                static_cast<double>(localTransform.ScaleShear[2][2])));
        } else {
            boneNode->LclScaling.Set(finalTransform.GetS());
        }
    }

    // Calculate inverse world matrix for geometry transformation.

    FbxAMatrix world;
    world.SetRow(0, FbxVector4(reinterpret_cast<double*>(granny_bone.InverseWorld4x4[0])));
    world.SetRow(1, FbxVector4(reinterpret_cast<double*>(granny_bone.InverseWorld4x4[1])));
    world.SetRow(2, FbxVector4(reinterpret_cast<double*>(granny_bone.InverseWorld4x4[2])));
    world.SetRow(3, FbxVector4(reinterpret_cast<double*>(granny_bone.InverseWorld4x4[3])));

    boneNode->SetGeometricTranslation(FbxNode::EPivotSet::eDestinationPivot, world.GetT());
    boneNode->SetGeometricRotation(FbxNode::EPivotSet::eDestinationPivot, world.GetR());
    boneNode->SetGeometricScaling(FbxNode::EPivotSet::eDestinationPivot, world.GetS());

    // Create the skeleton of this bone for the fbx scene.

    auto skeleton = FbxSkeleton::Create(m_fbxScene, boneName);
    boneNode->SetNodeAttribute(skeleton);

    // Add the bone node to the fbx scene as root or child node depending on
    // if the granny bone is a parent bone or not.

    if (parentIndex == GrannyNoParentBone) {
        // Add bone node as root to the fbx scene.
        m_fbxScene->GetRootNode()->AddChild(boneNode);

        // Set the skeleton type to root.
        // It's required so that the bones of the current skeleton will get visible.
        skeleton->SetSkeletonType(FbxSkeleton::eRoot);
    } else if (parentIndex > -1 && parentIndex < static_cast<int>(model->getBones().size())) {
        // Add the bone node as child node of a parent node to the fbx node.
        auto parentBoneNode = model->getBones().at(static_cast<unsigned>(parentIndex))->getNode();
        parentBoneNode->AddChild(boneNode);

        // Set the skeleton type to limb node.
        // It's also required so that the bones of the current skeleton will get visible.
        skeleton->SetSkeletonType(FbxSkeleton::eLimbNode);
    }
}

void FbxExporterSkeleton::exportPoses(Model::SharedPtr model)
{
    exportBindPose(model);
    exportRestPose(model);
}

void FbxExporterSkeleton::exportBindPose(Model::SharedPtr model)
{
    FbxNode* rootBone = model->getBones().at(0)->getNode();

    auto bindPose = FbxPose::Create(m_fbxScene, "BindPose");
    bindPose->SetIsBindPose(true);

    for (auto mesh : model->getMeshes()) {
        if (!mesh->isExcluded() && mesh->getNode()) {
            bindPose->Add(mesh->getNode(), mesh->getNode()->EvaluateGlobalTransform());
        }
    }

    bindPose->Add(rootBone, rootBone->EvaluateGlobalTransform());
    m_fbxScene->AddPose(bindPose);
}

void FbxExporterSkeleton::exportRestPose(Model::SharedPtr model)
{
    FbxNode* rootBone = model->getBones().at(0)->getNode();

    auto restPose = FbxPose::Create(m_fbxScene, "RestPose");
    restPose->SetIsBindPose(false);

    FbxMatrix restPoseMatrix;
    FbxVector4
        restPoseTransform,
        restPoseRotation,
        restPoseScale(1.0, 1.0, 1.0);

    restPoseMatrix.SetTRS(restPoseTransform, restPoseRotation, restPoseScale);

    for (auto mesh : model->getMeshes()) {
        if (!mesh->isExcluded() && mesh->getNode()) {
            restPose->Add(mesh->getNode(), restPoseMatrix);
        }
    }

    restPose->Add(rootBone, rootBone->EvaluateGlobalTransform());
    m_fbxScene->AddPose(restPose);
}

} // namespace GCL::Exporter
