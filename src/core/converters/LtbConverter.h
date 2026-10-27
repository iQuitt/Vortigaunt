// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>

#include "core/converters/IConverter.h"
#include "utils/FileIO.h"

struct ltbConverterSetting
{
	bool IgnoreMeshes = false;
	bool IgnoreAnimations = false;
	bool WriteQC = false;
};

#ifdef ENABLE_LITHTECH

#include "de_file.h"
#include "model.h"
#include "LzmaDecoder.h"
#include "ltb.h"
#include "assimp/scene.h"
#include "core/smd/SmdWriter.h"
enum 
{
	CONVERT_RET_OK = 0,
	CONVERT_RET_INVALID_INPUT_FILE = 1,
	CONVERT_RET_INVALID_LOAD_REQUEST = 2,
	CONVERT_RET_INVALID_HEADER = 3,
	CONVERT_RET_DECODING_FAILED = 4,
	CONVERT_RET_LOADING_MODEL_FAILED = 5,
};

typedef Model LTBModel;
typedef std::shared_ptr<LTBModel> LTBModelPtr;
typedef AnimInfo LTBAnimInfo;
typedef ModelAnim LTBAnim;
typedef AnimNode LTBAnimNode;
typedef AnimKeyFrame LTBAnimKeyFrame;
//
typedef aiScene ExportScene;
//
typedef aiMesh Mesh;
//
typedef aiNode Node;
typedef std::vector<Node*> NodesPtrVec;
//
typedef aiBone Bone;
typedef std::vector<Bone> BonesVec;
typedef std::vector<Bone*> BonesPtrVec;
//
typedef aiMaterial Material;
typedef std::vector<Material*> MaterialsPtrVec;
//
typedef aiAnimation Animation;
typedef std::vector<Animation*> AnimationsPtrVec;
typedef aiNodeAnim NodeAnimation;
typedef std::vector<NodeAnimation*> NodeAnimationsPtrVec;
//



/**
 * @brief Converts LithTech LTB models to SMD format
 */
class ltbConverter : public IConverter
{
public:
	ltbConverter();
	~ltbConverter();

	const char* Name() const override { return "LTB"; }
	bool CanConvert(const std::string& path) const override;
	// writes <outputDir>/<name>/<name>_reference.smd, its animations and, with WriteQC, <name>.qc
	ConvertResult ConvertFile(const std::string& inputPath, const std::string& outputDir) override;

	int ConvertSingleLTBFile(const std::string& inputFilePath, const std::string& outFilePath);
	int LoadLTBModel(const std::string& modelFilePath, LTBModelPtr ltbModel);
	void SetConvertSetting(ltbConverterSetting setting);
private:

	bool readLTBHeader(LTB_Header* head, DosFileStream* stream);

	bool decodingLTBFile(const std::string& inputFilePath, DosFileStream* fileStream);

	bool doConvertLTB(LTBModelPtr ltbModel, std::string outFilePath);

	void grabSkeletonNodesFromLTB(LTBModelPtr ltbModel);

	void grabAnimationsFromLTB(LTBModelPtr ltbModel);

	void grabMaterialsPerMeshFromLTB(LTBModelPtr ltbModel);

	void grabMeshesFromLTB(LTBModelPtr ltbModel);

	void grabBonesPerMeshFromLTB(LTBModelPtr ltbModel);

	aiMatrix4x4 recursCalcuMat(Node* sceneNode);

	void releaseGrabbedData();

	aiNode* getSkeletonNodeByName(const std::string& name);

	void printExportOverview(ExportScene* exportScene,LTBModelPtr ltbModel);
	//
	LzmaDecoder* m_lzmaDecoder;
	SmdWriter* m_smdWriter;
	//
	std::vector<Mesh*> m_meshesPtrVec;
	//
	NodesPtrVec m_skeletonNodes;
	std::map<std::string, Node*> m_name2SkeNode;
	std::map<std::string, int> m_name2SkeNodeIdx;
	//
	std::vector<BonesPtrVec> m_bonesPtrArrPerMeshVec;
	//
	MaterialsPtrVec m_materialsPtrList;
	//
	AnimationsPtrVec m_animPtrVec;
	std::vector<NodeAnimationsPtrVec> m_nodeAnimPtrVecList;
	//
	int m_maxNumOutputAnim = -1;
	//

	ltbConverterSetting m_setting;
};

#else // !ENABLE_LITHTECH

// Stub implementation for linux builds
// lithtech sdk does not support linux.
enum 
{
	CONVERT_RET_OK = 0,
	CONVERT_RET_INVALID_INPUT_FILE = 1,
	CONVERT_RET_INVALID_LOAD_REQUEST = 2,
	CONVERT_RET_INVALID_HEADER = 3,
	CONVERT_RET_DECODING_FAILED = 4,
	CONVERT_RET_LOADING_MODEL_FAILED = 5,
};



// just stubs for compile
class ltbConverter : public IConverter
{
public:
	ltbConverter() = default;
	~ltbConverter() = default;

	const char* Name() const override { return "LTB"; }
	bool CanConvert(const std::string& path) const override { return FileIO::lowerExtension(path) == ".ltb"; }
	ConvertResult ConvertFile(const std::string&, const std::string&) override { return { false, "LTB needs the LithTech SDK, which is Windows only" }; }

	int ConvertSingleLTBFile(const std::string&, const std::string&) { return CONVERT_RET_LOADING_MODEL_FAILED; }
	void SetConvertSetting(ltbConverterSetting) {}
};

#endif // ENABLE_LITHTECH
