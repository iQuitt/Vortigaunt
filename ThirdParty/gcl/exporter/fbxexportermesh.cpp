#include "gcl/exporter/fbxexportermesh.h"

#include <map>
#include <vector>

namespace GCL::Exporter {

void FbxExporterMesh::exportMeshes(Model::SharedPtr model, bool exportSkeleton)
{
    for (auto mesh : model->getMeshes()) {
        if (!mesh->isExcluded()) {
            exportMesh(model, mesh, exportSkeleton);
        }
    }
}

void FbxExporterMesh::exportMesh(Model::SharedPtr model, Mesh::SharedPtr mesh, bool exportSkeleton)
{
    auto meshNode = FbxNode::Create(m_fbxScene, mesh->getData()->Name);
    mesh->setNode(meshNode);

    m_fbxScene->GetRootNode()->AddChild(meshNode);

    auto fbxMesh = exportFbxMesh(mesh);

    if (exportSkeleton && model->getBones().size() > 0) {
        vector<FbxCluster*> clusters;
        vector<BoneBinding::SharedPtr> bindings;
        map<string, Bone::SharedPtr> boneMap;

        for (auto bone : model->getBones()) {
            boneMap[bone->getData().Name] = bone;
        }

        for (auto i = 0; i < mesh->getData()->BoneBindingCount; i++) {
            auto boneName = mesh->getData()->BoneBindings[i].BoneName;
            auto cluster = FbxCluster::Create(m_fbxScene, boneName);
            auto binding = make_shared<BoneBinding>(boneMap[boneName], cluster);

            bindings.push_back(binding);
        }

        auto grannyVertices = mesh->getRigidVertices();
        auto grannyVertexCounter = 0;

        if (mesh->isRigid()) {
            for (unsigned i = 0; i < grannyVertices.size(); i++) {
                for (auto binds : bindings) {
                    binds->getCluster()->AddControlPointIndex(grannyVertexCounter, 255.0);
                }

                grannyVertexCounter++;
            }

        } else {
            for (auto vertex : grannyVertices) {
                for (int i = 0; i < 4; i++) {
                    if (bindings[vertex.BoneIndices[i]] && vertex.BoneWeights[i]) {
                        bindings[vertex.BoneIndices[i]]->getCluster()->AddControlPointIndex(grannyVertexCounter, static_cast<double>(vertex.BoneWeights[i] / 255.0));
                    }
                }

                grannyVertexCounter++;
            }
        }

        auto initialTransform = FbxAMatrix(
            FbxDouble3(0, 0, 0),
            FbxDouble3(0, 0, 0),
            FbxDouble3(1, 1, 1));

        if (model->getBones()[0]->getData().LocalTransform.Flags == 0) {
            auto initialPlacement = model->getData()->InitialPlacement;

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
        }

        FbxAMatrix Mat = meshNode->EvaluateGlobalTransform();

        auto skin = FbxSkin::Create(m_fbxScene, "MeshSkin");

        // Set skinning type to rigid.
        skin->SetSkinningType(FbxSkin::EType::eRigid);

        for (auto binding : bindings) {
            auto bone = binding->getBone();
            if (bone) {
                auto boneNode = bone->getNode();
                if (boneNode) {
                    auto cluster = binding->getCluster();

                    // Set cluster bone link and linking mode.
                    cluster->SetLink(boneNode);
                    cluster->SetLinkMode(FbxCluster::ELinkMode::eNormalize);

                    // Set cluster transform matrix.
                    cluster->SetTransformMatrix(Mat);
                    cluster->SetTransformLinkMatrix(boneNode->EvaluateGlobalTransform());

                    skin->AddCluster(cluster);
                }
            }
        }

        fbxMesh->AddDeformer(skin);
    }
}

FbxMesh* FbxExporterMesh::exportFbxMesh(Mesh::SharedPtr mesh)
{
    // Create the mesh and three data sources for the vertex positions, normals and texture coordinates.
    FbxMesh* fbxMesh = FbxMesh::Create(m_fbxScene, mesh->getData()->Name);

    // Get vertices.
    auto grannyVertices = mesh->getRigidVertices();
    unsigned VertCount(grannyVertices.size());

    // Create control points.
    fbxMesh->InitControlPoints(static_cast<int>(VertCount));
    FbxVector4* ControlPoints = fbxMesh->GetControlPoints();

    // Get default layer or create.
    FbxLayer* defaultLayer = fbxMesh->GetLayer(0);

    if (defaultLayer == nullptr) {
        fbxMesh->CreateLayer();
        defaultLayer = fbxMesh->GetLayer(0);
    }

    // Create normal layer.
    FbxLayerElementNormal* normalLayer = FbxLayerElementNormal::Create(fbxMesh, "");
    normalLayer->SetMappingMode(FbxLayerElement::eByControlPoint);
    normalLayer->SetReferenceMode(FbxLayerElement::eDirect);

    // Create diffuse layer.
    FbxLayerElementUV* diffuseLayer = FbxLayerElementUV::Create(fbxMesh, "DiffuseUV");
    diffuseLayer->SetMappingMode(FbxLayerElement::eByControlPoint);
    diffuseLayer->SetReferenceMode(FbxLayerElement::eDirect);
    defaultLayer->SetUVs(diffuseLayer, FbxLayerElement::eTextureDiffuse);

    for (unsigned i = 0; i < grannyVertices.size(); i++) {
        ControlPoints[i] = FbxVector4(
            static_cast<double>(grannyVertices[i].Position[0]),
            static_cast<double>(grannyVertices[i].Position[1]),
            static_cast<double>(grannyVertices[i].Position[2]));

        normalLayer->GetDirectArray().Add(FbxVector4(
            static_cast<double>(grannyVertices[i].Normal[0]),
            static_cast<double>(grannyVertices[i].Normal[1]),
            static_cast<double>(grannyVertices[i].Normal[2])));

        diffuseLayer->GetDirectArray().Add(FbxVector2(
            static_cast<double>(grannyVertices[i].UV[0]),
            static_cast<double>(-grannyVertices[i].UV[1])));
    }

    defaultLayer->SetNormals(normalLayer);
    defaultLayer->SetUVs(diffuseLayer);

    // Create material layer.
    FbxLayerElementMaterial* materialLayer = FbxLayerElementMaterial::Create(fbxMesh, "");
    materialLayer->SetMappingMode(FbxLayerElement::eByPolygon);
    materialLayer->SetReferenceMode(FbxLayerElement::eIndexToDirect);
    defaultLayer->SetMaterials(materialLayer);

    const auto indexCount = GrannyGetMeshIndexCount(mesh->getData());
    const auto indexArray = new int[static_cast<unsigned>(indexCount)];

    GrannyCopyMeshIndices(mesh->getData(), 4, indexArray);

    // TODO: Refactor
    vector<Material*> added;

    for (auto i = 0; i < mesh->getData()->MaterialBindingCount; i++) {
        const auto material = mesh->getData()->MaterialBindings[i].Material;

        for (auto sceneMaterial : m_scene->getMaterials()) {
            if (sceneMaterial->getData() == material && find(added.begin(), added.end(), sceneMaterial.get()) == added.end()) {
                added.push_back(sceneMaterial.get());
                mesh->getNode()->AddMaterial(sceneMaterial->getNode());
                break;
            }
        }
    }

    // TODO: Refactor
    for (auto i = 0; i < indexCount; i += 3) {
        auto materialIndex = -1;

        if (mesh->getData()->MaterialBindingCount) {
            for (auto groupIndex = 0; groupIndex < mesh->getData()->PrimaryTopology->GroupCount; groupIndex++) {
                auto group = mesh->getData()->PrimaryTopology->Groups[groupIndex];
                auto groupTriOffset = i / 3;

                if (groupTriOffset >= group.TriFirst && groupTriOffset < (group.TriFirst + group.TriCount)) {
                    FbxSurfaceMaterial* currentMaterial = nullptr;

                    for (auto sceneMaterial : m_scene->getMaterials()) {
                        if (sceneMaterial->getData() == mesh->getData()->MaterialBindings[group.MaterialIndex].Material) {
                            currentMaterial = sceneMaterial->getNode();
                            break;
                        }
                    }

                    for (auto r = 0; r < mesh->getNode()->GetMaterialCount(); r++) {
                        if (mesh->getNode()->GetMaterial(r) == currentMaterial) {
                            materialIndex = r;
                            break;
                        }
                    }
                    break;
                }
            }
        }

        fbxMesh->BeginPolygon(materialIndex);
        fbxMesh->AddPolygon(indexArray[i]);
        fbxMesh->AddPolygon(indexArray[i + 1]);
        fbxMesh->AddPolygon(indexArray[i + 2]);
        fbxMesh->EndPolygon();
    }

    delete[] indexArray;

    mesh->getNode()->SetNodeAttribute(fbxMesh);

    return fbxMesh;
}

string FbxExporterMesh::sanitizeMaterialName(string name)
{
    return sanitizeName(name);
}

} // namespace GCL::Exporter
