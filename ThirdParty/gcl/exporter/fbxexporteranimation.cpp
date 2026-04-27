#include "gcl/exporter/fbxexporteranimation.h"

#include <map>
#include <vector>

namespace GCL::Exporter {

void FbxExporterAnimation::exportAnimations()
{
    // Create a list with names of all model.
    vector<string> modelList;

    // Create bone map for easier access later.
    map<string, FbxNode*> boneMap;

    for (const auto& model : m_scene->getModels()) {
        // Add model name to model list.
        modelList.push_back(model->getData()->Name);

        // Add bone to bone map.
        auto bones = model->getBones();

        if (bones.size() > 1) {
            for (auto bone : bones) {
                boneMap[bone->getData().Name] = bone->getNode();
            }
            break;
        }
    }

    int i = 0;
    for (auto animation : m_scene->getAnimations()) {
        if (animation->isExcluded()) {
            continue;
        }

        char animationName[256];
        sprintf_s(animationName, "animation_%d", i++);

        // Create animation stack and add at least one animation layer.
        FbxAnimStack* animStack = FbxAnimStack::Create(m_fbxScene, animationName);
        FbxAnimLayer* animLayer = FbxAnimLayer::Create(m_fbxScene, animationName);

        animStack->AddMember(animLayer);

        if (boneMap.size() == 0 && animation->getTracks().size() > 0) {
            string groupName = animation->getTracks().at(0)->getName();

            for (auto track : animation->getTracks()) {
                if (find(modelList.begin(), modelList.end(), track->getName()) != modelList.end()) {
                    groupName = track->getName();
                    break;
                }
            }

            for (auto track : animation->getTracks()) {
                auto trackName = track->getName();
                auto trackNameC = trackName.c_str();
                auto boneNode = FbxNode::Create(m_fbxScene, trackNameC);
                auto skeleton = FbxSkeleton::Create(m_fbxScene, trackNameC);

                if (groupName == trackName) {
                    skeleton->SetSkeletonType(FbxSkeleton::eRoot);
                } else {
                    skeleton->SetSkeletonType(FbxSkeleton::eLimbNode);
                }

                boneNode->SetNodeAttribute(skeleton);
                boneMap[trackName] = boneNode;
            }
        }

        // Export all tracks.
        for (auto track : animation->getTracks()) {
            auto trackName = track->getName();

            // Export curves if bone exist in current loaded skeleton.
            if (boneMap[trackName]) {
                exportCurves(track, boneMap[trackName], animLayer);
            }
        }
    }
}

void FbxExporterAnimation::exportCurves(Track::SharedPtr track, FbxNode* boneNode, FbxAnimLayer* animLayer)
{
    // Export position curve.

    {
        auto animCurveX = boneNode->LclTranslation.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
        auto animCurveY = boneNode->LclTranslation.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
        auto animCurveZ = boneNode->LclTranslation.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);

        for (auto key : track->getPositionKeys()) {
            exportCurveKey(key, animCurveX, animCurveY, animCurveZ);
        }
    }

    // Export rotation curve.

    {
        auto animCurveX = boneNode->LclRotation.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
        auto animCurveY = boneNode->LclRotation.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
        auto animCurveZ = boneNode->LclRotation.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);

        for (auto key : track->getRotationKeys()) {
            exportCurveKey(key, animCurveX, animCurveY, animCurveZ);
        }
    }

    // Export scale curve.

    {
        auto animCurveX = boneNode->LclScaling.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
        auto animCurveY = boneNode->LclScaling.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
        auto animCurveZ = boneNode->LclScaling.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);

        for (auto key : track->getScaleKeys()) {
            exportCurveKey(key, animCurveX, animCurveY, animCurveZ);
        }
    }

    {
        FbxAnimCurve* fbxTCurves[3];
        FbxAnimCurve* fbxRCurves[3];
        FbxAnimCurve* fbxSCurves[3];

        fbxTCurves[0] = boneNode->LclTranslation.GetCurve(animLayer, "X");
        fbxTCurves[1] = boneNode->LclTranslation.GetCurve(animLayer, "Y");
        fbxTCurves[2] = boneNode->LclTranslation.GetCurve(animLayer, "Z");

        fbxRCurves[0] = boneNode->LclRotation.GetCurve(animLayer, "X");
        fbxRCurves[1] = boneNode->LclRotation.GetCurve(animLayer, "Y");
        fbxRCurves[2] = boneNode->LclRotation.GetCurve(animLayer, "Z");

        fbxSCurves[0] = boneNode->LclScaling.GetCurve(animLayer, "X");
        fbxSCurves[1] = boneNode->LclScaling.GetCurve(animLayer, "Y");
        fbxSCurves[2] = boneNode->LclScaling.GetCurve(animLayer, "Z");

        FbxAnimCurveFilterUnroll unroll;
        unroll.SetForceAutoTangents(true);
        unroll.Apply(fbxRCurves, 3);

        FbxAnimCurveFilterResample resample;
        resample.Apply(fbxTCurves, 3);
        resample.Apply(fbxRCurves, 3);
        resample.Apply(fbxSCurves, 3);
    }
}

void FbxExporterAnimation::exportCurveKey(
    AbstractCurveKey key,
    FbxAnimCurve* animCurveX,
    FbxAnimCurve* animCurveY,
    FbxAnimCurve* animCurveZ)
{
    auto keyValue = key.getValue();

    animCurveX->KeyModifyBegin();
    animCurveY->KeyModifyBegin();
    animCurveZ->KeyModifyBegin();

    // Set key for x-axis.

    auto keyIndexX = animCurveX->KeyAdd(key.getTime());
    animCurveX->KeySetValue(keyIndexX, static_cast<float>(keyValue[0]));
    animCurveX->KeySetInterpolation(keyIndexX, FbxAnimCurveDef::eInterpolationCubic);

    // Set key for y-axis.

    auto keyIndexY = animCurveY->KeyAdd(key.getTime());
    animCurveY->KeySetValue(keyIndexY, static_cast<float>(keyValue[1]));
    animCurveY->KeySetInterpolation(keyIndexY, FbxAnimCurveDef::eInterpolationCubic);

    // Set key for z-axis.

    auto keyIndexZ = animCurveZ->KeyAdd(key.getTime());
    animCurveZ->KeySetValue(keyIndexZ, static_cast<float>(keyValue[2]));
    animCurveZ->KeySetInterpolation(keyIndexZ, FbxAnimCurveDef::eInterpolationCubic);

    animCurveX->KeyModifyEnd();
    animCurveY->KeyModifyEnd();
    animCurveZ->KeyModifyEnd();
}

} // namespace GCL::Exporter
