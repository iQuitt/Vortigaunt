#include "LoLModelDownloader.h"
#include "core/VortigauntLog.h"

#define Vortimsg(msg) \
    if (m_logCallback) { \
        m_logCallback(msg); \
    } else { \
        VortigauntLog::Vortigaunt_Printf(msg); \
    }

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QApplication>
#include <QEventLoop>
#include <QTimer>
#include <QSet>
#include <QTextStream>
#include <QSysInfo>
#include <QMap>
#include <QMutex>

#include <filesystem>
#include <fstream>
#include <vector>
#include <algorithm>
#include <cstring>

LoLModelDownloader::LoLModelDownloader()
{
}



void LoLModelDownloader::progress(int percent)
{
    if (m_progressCallback) {
        m_progressCallback(percent);
    }
}

bool LoLModelDownloader::downloadModel(
    const QString& championName,
    const QString& modelId,
    const QString& modelName,
    const QString& outputDir,
    bool autoFix,
    bool splitPrimitives,
    bool downloadChromas)
{
    // Start progress
    progress(5);
    
    // Create output directory structure
    QString championDir = QDir(outputDir).filePath(championName);
    QDir().mkpath(championDir);
    
    // Sanitize model name for filename
    QString safeName = modelName;
    for (QChar& c : safeName) {
        if (c == ' ' || c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || 
            c == '"' || c == '<' || c == '>' || c == '|') {
            c = '_';
        }
    }
    
    QString filename = QString("%1_%2.glb").arg(safeName, modelId);
    QString filepath = QDir(championDir).filePath(filename);
    
    Vortimsg(QString("Downloading GLB from CDN..."));
    progress(10);
    QString cdnUrl = QString("https://cdn.modelviewer.lol/lol/models/%1/%2/model.glb")
                        .arg(championName.toLower(), modelId);
    
    if (!downloadGLB(cdnUrl, filepath)) {
        Vortimsg("Failed to download GLB");
        return false;
    }
    
    Vortimsg(QString("GLB downloaded: %1").arg(QFileInfo(filepath).fileName()));
    progress(30);
    
    QString currentFile = filepath;
    
    if (autoFix) {
        Vortimsg("Fixing transforms...");
        QString fixedFile = filepath;
        fixedFile.replace(".glb", "_FIXED.glb");
        
        if (fixTransforms(currentFile, fixedFile, modelId)) {
            Vortimsg("Transforms fixed");
            currentFile = fixedFile;
        } else {
            Vortimsg("Transform fix skipped");
        }
    }
    progress(50);
    
    // Split mesh primitives
    if (splitPrimitives) {
        Vortimsg("Splitting mesh primitives...");
        QString splitFile = currentFile;
        splitFile.replace("_FIXED.glb", "_SPLIT.glb").replace(".glb", "_SPLIT.glb");
        
        if (splitMeshPrimitives(currentFile, splitFile)) {
            Vortimsg("Mesh primitives split");
            currentFile = splitFile;
        } else {
            Vortimsg("Primitive split skipped");
        }
    }
    progress(70);
    
    //  Download chromas
    if (downloadChromas) {
        Vortimsg("Checking for chromas...");
        QStringList chromaIDs = detectChromaIDs(modelId, championName, currentFile);
        
        if (!chromaIDs.isEmpty()) {
            Vortimsg(QString("Found %1 chroma(s)").arg(chromaIDs.size()));
            
            for (const QString& chromaId : chromaIDs) {
                if (chromaId == modelId) {
                    continue; // Skip base model
                }
                
                Vortimsg(QString("Downloading chroma %1...").arg(chromaId));
                
                QString chromaDir = QDir(championDir).filePath(QString("chromas_%1").arg(chromaId));
                
                if (downloadChromaTextures(championName, modelId, chromaId, chromaDir, currentFile)) {
                    QString chromaGLB = currentFile;
                    chromaGLB.replace(".glb", QString("_chroma_%1.glb").arg(chromaId));
                    
                    if (createChromaGLB(currentFile, chromaGLB, chromaDir)) {
                        Vortimsg(QString("Chroma %1 created").arg(chromaId));
                    }
                }
            }
        } else {
            Vortimsg("No chromas found");
        }
    }
    
    Vortimsg(QString("Done: %1").arg(QFileInfo(currentFile).fileName()));
    progress(100);
    return true;
}

bool LoLModelDownloader::downloadGLB(const QString& url, const QString& outputPath)
{
    QNetworkAccessManager manager;
    QUrl urlObj(url);
    QNetworkRequest request(urlObj);
    request.setHeader(QNetworkRequest::UserAgentHeader, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
    
    QNetworkReply* reply = manager.get(request);
    
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    
    // Timeout
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(30000); // 30 seconds
    
    loop.exec();
    
    if (reply->error() != QNetworkReply::NoError) {
        Vortimsg(QString("QNetwork error: %1").arg(reply->errorString()));
        reply->deleteLater();
        return false;
    }
    
    QByteArray data = reply->readAll();
    reply->deleteLater();
    
    if (data.isEmpty()) {
        Vortimsg("Error: Empty response");
        return false;
    }
    
    // Verify downloaded data is a valid GLB file
    if (data.size() < 12) {
        Vortimsg(QString("Error: Downloaded data too small: %1 bytes").arg(data.size()));
        return false;
    }
    
    // Check GLB magic number
    if (data.left(4) != "glTF") {
        Vortimsg("Error: Invalid GLB file");
        return false;
    }
    
    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly)) {
        Vortimsg(QString("Error: Cannot write file: %1").arg(outputPath));
        return false;
    }
    
    qint64 written = file.write(data);
    file.close();
    
    if (written != data.size()) {
        Vortimsg("Error: File write incomplete");
        return false;
    }
    
    return true;
}


QJsonObject LoLModelDownloader::parseGLB(const QString& filepath, QByteArray& binaryChunk)
{
    QJsonObject empty;
    
    QFile file(filepath);
    if (!file.open(QIODevice::ReadOnly)) {
        Vortimsg(QString("[ERROR] Cannot open GLB file: %1").arg(filepath));
        return empty;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    if (data.size() < 12) {
        Vortimsg("[ERROR] Invalid GLB file(too small)");
        return empty;
    }
    
    // Check file
    if (data.mid(0, 4) != "glTF") {
        Vortimsg("[ERROR] Invalid GLB file");
        return empty;
    }
    
    // Read header (little-endian)
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(data.constData());
    uint32_t version = bytes[4] | (bytes[5] << 8) | (bytes[6] << 16) | (bytes[7] << 24);
    uint32_t length = bytes[8] | (bytes[9] << 8) | (bytes[10] << 16) | (bytes[11] << 24);
    
    Q_UNUSED(version);
    Q_UNUSED(length);
    
    // Read JSON chunk
    if (data.size() < 20) {
        Vortimsg("[ERROR] Invalid GLB file (no JSON chunk)");
        return empty;
    }
    
    uint32_t jsonChunkLength = bytes[12] | (bytes[13] << 8) | (bytes[14] << 16) | (bytes[15] << 24);
    QByteArray jsonChunkType = data.mid(16, 4);
    
    if (jsonChunkType != "JSON") {
        Vortimsg("[ERROR] Invalid JSON chunk type");
        return empty;
    }
    
    QByteArray jsonData = data.mid(20, jsonChunkLength);
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &error);
    
    if (error.error != QJsonParseError::NoError) {
        Vortimsg(QString("[ERROR] JSON parse error: %1").arg(error.errorString()));
        return empty;
    }
    
    int binaryOffset = 20 + jsonChunkLength;
    
    if (data.size() >= binaryOffset + 8) {
        const uint8_t* binBytes = reinterpret_cast<const uint8_t*>(data.constData() + binaryOffset);
        uint32_t binChunkLength = binBytes[0] | (binBytes[1] << 8) | (binBytes[2] << 16) | (binBytes[3] << 24);
        
        // Do byte-by-byte comparison (string comparison with null char may cause issues)
        const char* binChunkTypeBytes = reinterpret_cast<const char*>(data.constData() + binaryOffset + 4);
        bool isBinChunk = (binChunkTypeBytes[0] == 'B' && 
                          binChunkTypeBytes[1] == 'I' && 
                          binChunkTypeBytes[2] == 'N' && 
                          binChunkTypeBytes[3] == '\x00');
        
        if (isBinChunk) {
            int expectedSize = binaryOffset + 8 + binChunkLength;
            if (data.size() >= expectedSize) {
                binaryChunk = data.mid(binaryOffset + 8, binChunkLength);
            } else {
                // Still read available data
                if (data.size() > binaryOffset + 8) {
                    binaryChunk = data.mid(binaryOffset + 8, data.size() - binaryOffset - 8);
                }
            }
        }
    }
    
    return doc.object();
}

bool LoLModelDownloader::writeGLB(const QString& filepath, const QJsonObject& gltf, const QByteArray& binaryChunk)
{
  
    QJsonDocument doc(gltf);
    QByteArray jsonBytes = doc.toJson(QJsonDocument::Compact);
    
    int jsonPadding = calculatePadding(jsonBytes.size());
    jsonBytes.append(QByteArray(jsonPadding, ' '));
    
    int totalLength = 12 + 8 + jsonBytes.size();
    if (!binaryChunk.isEmpty()) {
        totalLength += 8 + binaryChunk.size();
    }
    
    QFile file(filepath);
    if (!file.open(QIODevice::WriteOnly)) {
        Vortimsg(QString("[ERROR] Cannot open file for writing: %1").arg(filepath));
        return false;
    }
    
    file.write("glTF", 4);
    
    uint32_t version = 2;
    QByteArray versionBytes(reinterpret_cast<const char*>(&version), 4);
    // Ensure little-endian on all platforms
    if (QSysInfo::ByteOrder == QSysInfo::BigEndian) {
        std::reverse(versionBytes.begin(), versionBytes.end());
    }
    file.write(versionBytes);
    
    // Write total length (little-endian)
    QByteArray lengthBytes(reinterpret_cast<const char*>(&totalLength), 4);
    if (QSysInfo::ByteOrder == QSysInfo::BigEndian) {
        std::reverse(lengthBytes.begin(), lengthBytes.end());
    }
    file.write(lengthBytes);
    
    uint32_t jsonChunkLength = jsonBytes.size();
    QByteArray jsonLengthBytes(reinterpret_cast<const char*>(&jsonChunkLength), 4);
    if (QSysInfo::ByteOrder == QSysInfo::BigEndian) {
        std::reverse(jsonLengthBytes.begin(), jsonLengthBytes.end());
    }
    file.write(jsonLengthBytes);
    file.write("JSON", 4);
    file.write(jsonBytes);
    
    if (!binaryChunk.isEmpty()) {
        uint32_t binChunkLength = binaryChunk.size();
        QByteArray binLengthBytes(reinterpret_cast<const char*>(&binChunkLength), 4);
        if (QSysInfo::ByteOrder == QSysInfo::BigEndian) {
            std::reverse(binLengthBytes.begin(), binLengthBytes.end());
        }
        file.write(binLengthBytes);
        file.write("BIN\x00", 4);
        file.write(binaryChunk);
    }
    
    file.close();
    
    // Verify file size matches total length
    QFileInfo fileInfo(filepath);
    if (fileInfo.size() != totalLength) {
        Vortimsg(QString("[WARNING] File size (%1) doesn't match total length (%2)").arg(fileInfo.size()).arg(totalLength));
    }
    
    return true;
}

bool LoLModelDownloader::splitMeshPrimitives(const QString& inputGLB, const QString& outputGLB)
{
    QByteArray binaryChunk;
    QJsonObject gltf = parseGLB(inputGLB, binaryChunk);
    
    if (gltf.isEmpty()) {
        Vortimsg("  Failed to parse GLB");
        return false;
    }
    
    if (!gltf.contains("meshes") || !gltf["meshes"].isArray()) {
        Vortimsg("  No meshes found in GLB");
        return false;
    }
    
    QJsonArray meshes = gltf["meshes"].toArray();
    QJsonArray newMeshes;
    QSet<int> usedMaterials;
    QSet<int> usedAccessors;
    QSet<int> usedBufferViews;
    QSet<int> usedImages;
    QSet<int> usedTextures;
    QSet<int> usedSamplers;
    QSet<int> usedSkins;
    QSet<int> usedJointNodes;
    QMap<int, QList<int>> oldMeshToNewMeshes; 
    
    // 1. Process each mesh and separate primitives into individual meshes
    for (int meshIdx = 0; meshIdx < meshes.size(); ++meshIdx) {
        QJsonObject mesh = meshes[meshIdx].toObject();
        QString meshName = mesh["name"].toString();
        
        if (!mesh.contains("primitives") || !mesh["primitives"].isArray()) {
            continue;
        }
        
        QJsonArray primitives = mesh["primitives"].toArray();
        QList<int> newMeshIndicesForOld;
        
        
        // Create new mesh for each primitive
        for (int primIdx = 0; primIdx < primitives.size(); ++primIdx) {
            QJsonObject primitive = primitives[primIdx].toObject();
            
            // Get material name
            QString materialName = "Unknown";
            int materialIdx = -1;
            if (primitive.contains("material")) {
                materialIdx = primitive["material"].toInt();
                if (gltf.contains("materials") && gltf["materials"].isArray()) {
                    QJsonArray materials = gltf["materials"].toArray();
                    if (materialIdx >= 0 && materialIdx < materials.size()) {
                        QJsonObject material = materials[materialIdx].toObject();
                        materialName = material["name"].toString();
                    }
                }
            }
            
            // Create new mesh - contains only this primitive
            QJsonObject newMesh;
            newMesh["name"] = (materialName != "Unknown") ? materialName : QString("%1_Primitive_%2").arg(meshName).arg(primIdx);
            
            QJsonArray newPrimitives;
            newPrimitives.append(primitive);
            newMesh["primitives"] = newPrimitives;
            
            int newMeshIdx = newMeshes.size();
            newMeshes.append(newMesh);
            newMeshIndicesForOld.append(newMeshIdx);
            
            if (materialIdx >= 0) {
                usedMaterials.insert(materialIdx);
            }
            
            // Collect accessors
            if (primitive.contains("attributes") && primitive["attributes"].isObject()) {
                QJsonObject attributes = primitive["attributes"].toObject();
                for (auto it = attributes.begin(); it != attributes.end(); ++it) {
                    usedAccessors.insert(it.value().toInt());
                }
            }
            if (primitive.contains("indices")) {
                usedAccessors.insert(primitive["indices"].toInt());
            }
            
        }
        
        oldMeshToNewMeshes[meshIdx] = newMeshIndicesForOld;
    }
    
    
    // 2. Copy materials and collect texture/image/samplers
    QJsonArray newMaterialsList;
    QMap<int, int> materialIndexMapping;
    if (gltf.contains("materials") && gltf["materials"].isArray()) {
        QJsonArray materials = gltf["materials"].toArray();
        for (int i = 0; i < materials.size(); ++i) {
            if (usedMaterials.contains(i)) {
                newMaterialsList.append(materials[i]);
                materialIndexMapping[i] = newMaterialsList.size() - 1;
                
                // Collect textures
                QJsonObject mat = materials[i].toObject();
                
                // pbrMetallicRoughness.baseColorTexture
                if (mat.contains("pbrMetallicRoughness") && mat["pbrMetallicRoughness"].isObject()) {
                    QJsonObject pbr = mat["pbrMetallicRoughness"].toObject();
                    if (pbr.contains("baseColorTexture") && pbr["baseColorTexture"].isObject()) {
                        QJsonObject tex = pbr["baseColorTexture"].toObject();
                        if (tex.contains("index")) {
                            usedTextures.insert(tex["index"].toInt());
                        }
                    }
                }
                
                // normalTexture, occlusionTexture, emissiveTexture
                QStringList textureProps = {"normalTexture", "occlusionTexture", "emissiveTexture"};
                for (const QString& prop : textureProps) {
                    if (mat.contains(prop) && mat[prop].isObject()) {
                        QJsonObject tex = mat[prop].toObject();
                        if (tex.contains("index")) {
                            usedTextures.insert(tex["index"].toInt());
                        }
                    }
                }
            }
        }
    }
    
    // 3. Copy textures and collect images
    QJsonArray newTexturesList;
    QMap<int, int> textureIndexMapping;
    if (gltf.contains("textures") && gltf["textures"].isArray()) {
        QJsonArray textures = gltf["textures"].toArray();
        for (int i = 0; i < textures.size(); ++i) {
            if (usedTextures.contains(i)) {
                newTexturesList.append(textures[i]);
                textureIndexMapping[i] = newTexturesList.size() - 1;
                
                QJsonObject tex = textures[i].toObject();
                if (tex.contains("source")) {
                    usedImages.insert(tex["source"].toInt());
                }
                if (tex.contains("sampler")) {
                    usedSamplers.insert(tex["sampler"].toInt());
                }
            }
        }
    }
    
    // 4. Copy images and collect buffer views
    QJsonArray newImagesList;
    QMap<int, int> imageIndexMapping;
    if (gltf.contains("images") && gltf["images"].isArray()) {
        QJsonArray images = gltf["images"].toArray();
        for (int i = 0; i < images.size(); ++i) {
            if (usedImages.contains(i)) {
                newImagesList.append(images[i]);
                imageIndexMapping[i] = newImagesList.size() - 1;
                
                QJsonObject img = images[i].toObject();
                if (img.contains("bufferView")) {
                    usedBufferViews.insert(img["bufferView"].toInt());
                }
            }
        }
    }
    
    // 5. Copy samplers
    QJsonArray newSamplersList;
    QMap<int, int> samplerIndexMapping;
    if (gltf.contains("samplers") && gltf["samplers"].isArray()) {
        QJsonArray samplers = gltf["samplers"].toArray();
        for (int i = 0; i < samplers.size(); ++i) {
            if (usedSamplers.contains(i)) {
                newSamplersList.append(samplers[i]);
                samplerIndexMapping[i] = newSamplersList.size() - 1;
            }
        }
    }
    
    if (gltf.contains("nodes") && gltf["nodes"].isArray()) {
        QJsonArray nodes = gltf["nodes"].toArray();
        for (int i = 0; i < nodes.size(); ++i) {
            QJsonObject node = nodes[i].toObject();
            if (node.contains("skin")) {
                usedSkins.insert(node["skin"].toInt());
            }
        }
    }
    
    QJsonArray newSkinsList;
    QMap<int, int> skinIndexMapping;
    if (gltf.contains("skins") && gltf["skins"].isArray()) {
        QJsonArray skins = gltf["skins"].toArray();
        for (int i = 0; i < skins.size(); ++i) {
            if (usedSkins.contains(i)) {
                newSkinsList.append(skins[i]);
                skinIndexMapping[i] = newSkinsList.size() - 1;
                
                QJsonObject skin = skins[i].toObject();
                if (skin.contains("joints") && skin["joints"].isArray()) {
                    QJsonArray joints = skin["joints"].toArray();
                    for (int j = 0; j < joints.size(); ++j) {
                        usedJointNodes.insert(joints[j].toInt());
                    }
                }
                if (skin.contains("inverseBindMatrices")) {
                    usedAccessors.insert(skin["inverseBindMatrices"].toInt());
                }
            }
        }
    }
    
    // 7. Copy animations
    QJsonArray newAnimationsList;
    if (gltf.contains("animations") && gltf["animations"].isArray()) {
        QJsonArray animations = gltf["animations"].toArray();
        for (int i = 0; i < animations.size(); ++i) {
            QJsonObject anim = animations[i].toObject();
            if (anim.contains("samplers") && anim["samplers"].isArray()) {
                QJsonArray samplers = anim["samplers"].toArray();
                for (int j = 0; j < samplers.size(); ++j) {
                    QJsonObject sampler = samplers[j].toObject();
                    if (sampler.contains("input")) {
                        usedAccessors.insert(sampler["input"].toInt());
                    }
                    if (sampler.contains("output")) {
                        usedAccessors.insert(sampler["output"].toInt());
                    }
                }
            }
            newAnimationsList.append(anim);
        }
    }
    
    // 8. Copy accessors and collect buffer views
    QJsonArray newAccessorsList;
    QMap<int, int> accessorIndexMapping;
    if (gltf.contains("accessors") && gltf["accessors"].isArray()) {
        QJsonArray accessors = gltf["accessors"].toArray();
        for (int i = 0; i < accessors.size(); ++i) {
            if (usedAccessors.contains(i)) {
                newAccessorsList.append(accessors[i]);
                accessorIndexMapping[i] = newAccessorsList.size() - 1;
                
                QJsonObject acc = accessors[i].toObject();
                if (acc.contains("bufferView")) {
                    usedBufferViews.insert(acc["bufferView"].toInt());
                }
            }
        }
    }
    
    // 9. Copy buffer view
    // buffer views are copied without checking buffer index
    // But in GLB format buffer index should always be 0 (single buffer)
    QJsonArray newBufferViewsList;
    QMap<int, int> bufferViewIndexMapping;
    if (gltf.contains("bufferViews") && gltf["bufferViews"].isArray()) {
        QJsonArray bufferViews = gltf["bufferViews"].toArray();
        for (int i = 0; i < bufferViews.size(); ++i) {
            if (usedBufferViews.contains(i)) {
                QJsonObject bv = bufferViews[i].toObject();
                // Buffer index should always be 0 (single buffer in GLB format)
                bv["buffer"] = 0;
                newBufferViewsList.append(bv);
                bufferViewIndexMapping[i] = newBufferViewsList.size() - 1;
            }
        }
    }
    
    // 10. Update indices
    // Update material indices
    for (int i = 0; i < newMeshes.size(); ++i) {
        QJsonObject mesh = newMeshes[i].toObject();
        if (mesh.contains("primitives") && mesh["primitives"].isArray()) {
            QJsonArray primitives = mesh["primitives"].toArray();
            for (int j = 0; j < primitives.size(); ++j) {
                QJsonObject prim = primitives[j].toObject();
                if (prim.contains("material")) {
                    int oldMatIdx = prim["material"].toInt();
                    if (materialIndexMapping.contains(oldMatIdx)) {
                        prim["material"] = materialIndexMapping[oldMatIdx];
                        primitives[j] = prim;
                    }
                }
            }
            mesh["primitives"] = primitives;
            newMeshes[i] = mesh;
        }
    }
    
    // Update texture indices
    for (int i = 0; i < newTexturesList.size(); ++i) {
        QJsonObject tex = newTexturesList[i].toObject();
        if (tex.contains("source")) {
            int oldImgIdx = tex["source"].toInt();
            if (imageIndexMapping.contains(oldImgIdx)) {
                tex["source"] = imageIndexMapping[oldImgIdx];
            }
        }
        if (tex.contains("sampler")) {
            int oldSampIdx = tex["sampler"].toInt();
            if (samplerIndexMapping.contains(oldSampIdx)) {
                tex["sampler"] = samplerIndexMapping[oldSampIdx];
            }
        }
        newTexturesList[i] = tex;
    }
    
    // Update texture indices in materials
    for (int i = 0; i < newMaterialsList.size(); ++i) {
        QJsonObject mat = newMaterialsList[i].toObject();
        
        if (mat.contains("pbrMetallicRoughness") && mat["pbrMetallicRoughness"].isObject()) {
            QJsonObject pbr = mat["pbrMetallicRoughness"].toObject();
            if (pbr.contains("baseColorTexture") && pbr["baseColorTexture"].isObject()) {
                QJsonObject tex = pbr["baseColorTexture"].toObject();
                if (tex.contains("index")) {
                    int oldTexIdx = tex["index"].toInt();
                    if (textureIndexMapping.contains(oldTexIdx)) {
                        tex["index"] = textureIndexMapping[oldTexIdx];
                        pbr["baseColorTexture"] = tex;
                    }
                }
            }
            mat["pbrMetallicRoughness"] = pbr;
        }
        
        QStringList textureProps = {"normalTexture", "occlusionTexture", "emissiveTexture"};
        for (const QString& prop : textureProps) {
            if (mat.contains(prop) && mat[prop].isObject()) {
                QJsonObject tex = mat[prop].toObject();
                if (tex.contains("index")) {
                    int oldTexIdx = tex["index"].toInt();
                    if (textureIndexMapping.contains(oldTexIdx)) {
                        tex["index"] = textureIndexMapping[oldTexIdx];
                        mat[prop] = tex;
                    }
                }
            }
        }
        
        newMaterialsList[i] = mat;
    }
    
    // Update buffer_view indices in images
    for (int i = 0; i < newImagesList.size(); ++i) {
        QJsonObject img = newImagesList[i].toObject();
        if (img.contains("bufferView")) {
            int oldBvIdx = img["bufferView"].toInt();
            if (bufferViewIndexMapping.contains(oldBvIdx)) {
                img["bufferView"] = bufferViewIndexMapping[oldBvIdx];
            }
        }
        newImagesList[i] = img;
    }
    
    // Update buffer view indices in accessors
    for (int i = 0; i < newAccessorsList.size(); ++i) {
        QJsonObject acc = newAccessorsList[i].toObject();
        if (acc.contains("bufferView")) {
            int oldBvIdx = acc["bufferView"].toInt();
            if (bufferViewIndexMapping.contains(oldBvIdx)) {
                acc["bufferView"] = bufferViewIndexMapping[oldBvIdx];
            }
        }
        newAccessorsList[i] = acc;
    }
    
    // Update inverse bind matrices accessor index in skins
    for (int i = 0; i < newSkinsList.size(); ++i) {
        QJsonObject skin = newSkinsList[i].toObject();
        if (skin.contains("inverseBindMatrices")) {
            int oldAccIdx = skin["inverseBindMatrices"].toInt();
            if (accessorIndexMapping.contains(oldAccIdx)) {
                skin["inverseBindMatrices"] = accessorIndexMapping[oldAccIdx];
            }
        }
        newSkinsList[i] = skin;
    }
    
    // Update animation sampler accessor indices
    for (int i = 0; i < newAnimationsList.size(); ++i) {
        QJsonObject anim = newAnimationsList[i].toObject();
        if (anim.contains("samplers") && anim["samplers"].isArray()) {
            QJsonArray samplers = anim["samplers"].toArray();
            for (int j = 0; j < samplers.size(); ++j) {
                QJsonObject sampler = samplers[j].toObject();
                if (sampler.contains("input")) {
                    int oldAccIdx = sampler["input"].toInt();
                    if (accessorIndexMapping.contains(oldAccIdx)) {
                        sampler["input"] = accessorIndexMapping[oldAccIdx];
                    }
                }
                if (sampler.contains("output")) {
                    int oldAccIdx = sampler["output"].toInt();
                    if (accessorIndexMapping.contains(oldAccIdx)) {
                        sampler["output"] = accessorIndexMapping[oldAccIdx];
                    }
                }
                samplers[j] = sampler;
            }
            anim["samplers"] = samplers;
        }
        newAnimationsList[i] = anim;
    }
    
    // Update primitive accessor indices
    for (int i = 0; i < newMeshes.size(); ++i) {
        QJsonObject mesh = newMeshes[i].toObject();
        if (mesh.contains("primitives") && mesh["primitives"].isArray()) {
            QJsonArray primitives = mesh["primitives"].toArray();
            for (int j = 0; j < primitives.size(); ++j) {
                QJsonObject prim = primitives[j].toObject();
                
                if (prim.contains("attributes") && prim["attributes"].isObject()) {
                    QJsonObject attributes = prim["attributes"].toObject();
                    QJsonObject newAttributes;
                    for (auto it = attributes.begin(); it != attributes.end(); ++it) {
                        int oldAccIdx = it.value().toInt();
                        if (accessorIndexMapping.contains(oldAccIdx)) {
                            newAttributes[it.key()] = accessorIndexMapping[oldAccIdx];
                        } else {
                            newAttributes[it.key()] = it.value();
                        }
                    }
                    prim["attributes"] = newAttributes;
                }
                
                if (prim.contains("indices")) {
                    int oldAccIdx = prim["indices"].toInt();
                    if (accessorIndexMapping.contains(oldAccIdx)) {
                        prim["indices"] = accessorIndexMapping[oldAccIdx];
                    }
                }
                
                primitives[j] = prim;
            }
            mesh["primitives"] = primitives;
            newMeshes[i] = mesh;
        }
    }
    
    QJsonArray newNodes;
    QMap<int, int> nodeIndexMapping;
    
    // Copy skeleton joint nodes
    if (gltf.contains("nodes") && gltf["nodes"].isArray()) {
        QJsonArray nodes = gltf["nodes"].toArray();
        for (int i = 0; i < nodes.size(); ++i) {
            if (usedJointNodes.contains(i)) {
                QJsonObject node = nodes[i].toObject();
                
                // Skin referansını güncelle
                if (node.contains("skin")) {
                    int oldSkinIdx = node["skin"].toInt();
                    if (skinIndexMapping.contains(oldSkinIdx)) {
                        node["skin"] = skinIndexMapping[oldSkinIdx];
                    }
                }
                
                newNodes.append(node);
                nodeIndexMapping[i] = newNodes.size() - 1;
            }
        }
    }
    
    // Create separate node for each new mesh
    QList<int> meshNodeIndices;
    for (int i = 0; i < newMeshes.size(); ++i) {
        QJsonObject mesh = newMeshes[i].toObject();
        if (mesh.contains("primitives") && mesh["primitives"].isArray()) {
            QJsonObject node;
            node["name"] = mesh["name"];
            node["mesh"] = i;
            newNodes.append(node);
            meshNodeIndices.append(newNodes.size() - 1);
        }
    }
    
    // Add skins from original mesh nodes to ALL new mesh nodes
    if (gltf.contains("nodes") && gltf["nodes"].isArray()) {
        QJsonArray nodes = gltf["nodes"].toArray();
        for (int oldIdx = 0; oldIdx < nodes.size(); ++oldIdx) {
            QJsonObject oldNode = nodes[oldIdx].toObject();
            if (oldNode.contains("mesh")) {
                int oldMeshIdx = oldNode["mesh"].toInt();
                if (oldMeshToNewMeshes.contains(oldMeshIdx)) {
                    QList<int> newMeshIndices = oldMeshToNewMeshes[oldMeshIdx];
                    if (oldNode.contains("skin") && !newMeshIndices.isEmpty()) {
                        int oldSkinIdx = oldNode["skin"].toInt();
                        if (skinIndexMapping.contains(oldSkinIdx)) {
                            int mappedSkinIdx = skinIndexMapping[oldSkinIdx];
                            // Add skin to each new mesh node
                            for (int newMeshIdx : newMeshIndices) {
                                if (newMeshIdx < meshNodeIndices.size()) {
                                    int meshNodeIdx = meshNodeIndices[newMeshIdx];
                                    if (meshNodeIdx < newNodes.size()) {
                                        QJsonObject meshNode = newNodes[meshNodeIdx].toObject();
                                        meshNode["skin"] = mappedSkinIdx;
                                        newNodes[meshNodeIdx] = meshNode;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Update children references
    for (int i = 0; i < newNodes.size(); ++i) {
        QJsonObject node = newNodes[i].toObject();
        if (node.contains("children") && node["children"].isArray()) {
            QJsonArray children = node["children"].toArray();
            QJsonArray newChildren;
            for (int j = 0; j < children.size(); ++j) {
                int childIdx = children[j].toInt();
                if (nodeIndexMapping.contains(childIdx)) {
                    newChildren.append(nodeIndexMapping[childIdx]);
                }
            }
            if (newChildren.isEmpty()) {
                node.remove("children");
            } else {
                node["children"] = newChildren;
            }
            newNodes[i] = node;
        }
    }
    
    // Update joint indices in skins
    for (int i = 0; i < newSkinsList.size(); ++i) {
        QJsonObject skin = newSkinsList[i].toObject();
        if (skin.contains("joints") && skin["joints"].isArray()) {
            QJsonArray joints = skin["joints"].toArray();
            QJsonArray newJoints;
            for (int j = 0; j < joints.size(); ++j) {
                int jointIdx = joints[j].toInt();
                if (nodeIndexMapping.contains(jointIdx)) {
                    newJoints.append(nodeIndexMapping[jointIdx]);
                }
            }
            skin["joints"] = newJoints;
            newSkinsList[i] = skin;
        }
    }
    
    // Update node references in animation channels
    for (int i = 0; i < newAnimationsList.size(); ++i) {
        QJsonObject anim = newAnimationsList[i].toObject();
        if (anim.contains("channels") && anim["channels"].isArray()) {
            QJsonArray channels = anim["channels"].toArray();
            for (int j = 0; j < channels.size(); ++j) {
                QJsonObject channel = channels[j].toObject();
                if (channel.contains("target") && channel["target"].isObject()) {
                    QJsonObject target = channel["target"].toObject();
                    if (target.contains("node")) {
                        int oldNodeIdx = target["node"].toInt();
                        if (nodeIndexMapping.contains(oldNodeIdx)) {
                            target["node"] = nodeIndexMapping[oldNodeIdx];
                            channel["target"] = target;
                        }
                    }
                }
                channels[j] = channel;
            }
            anim["channels"] = channels;
            newAnimationsList[i] = anim;
        }
    }
    
    QJsonObject newGltf;
    
    // Asset
    if (gltf.contains("asset")) {
        newGltf["asset"] = gltf["asset"];
    } else {
        QJsonObject asset;
        asset["version"] = "2.0";
        newGltf["asset"] = asset;
    }
    
    // Scene
    QJsonObject scene;
    QJsonArray sceneNodes;
    for (int idx : meshNodeIndices) {
        sceneNodes.append(idx);
    }
    scene["nodes"] = sceneNodes;
    QJsonArray scenes;
    scenes.append(scene);
    newGltf["scene"] = 0;
    newGltf["scenes"] = scenes;
    
    // Nodes
    newGltf["nodes"] = newNodes;
    
    // Meshes
    newGltf["meshes"] = newMeshes;
    
    // Diğer verileri ekle
    if (!newMaterialsList.isEmpty()) {
        newGltf["materials"] = newMaterialsList;
    }
    if (!newTexturesList.isEmpty()) {
        newGltf["textures"] = newTexturesList;
    }
    if (!newImagesList.isEmpty()) {
        newGltf["images"] = newImagesList;
    }
    if (!newSamplersList.isEmpty()) {
        newGltf["samplers"] = newSamplersList;
    }
    if (!newAccessorsList.isEmpty()) {
        newGltf["accessors"] = newAccessorsList;
    }
    if (!newBufferViewsList.isEmpty()) {
        newGltf["bufferViews"] = newBufferViewsList;
    }
    if (!newSkinsList.isEmpty()) {
        newGltf["skins"] = newSkinsList;
    }
    if (!newAnimationsList.isEmpty()) {
        newGltf["animations"] = newAnimationsList;
    }
    bool hasBufferViews = !newBufferViewsList.isEmpty();
    bool needsBuffer = false;
    
    if (gltf.contains("buffers") && !binaryChunk.isEmpty()) {
        needsBuffer = true;
    } else if (hasBufferViews && !binaryChunk.isEmpty()) {
        // If buffer views exist, buffer MUST be added (buffer views reference buffer: 0)
        needsBuffer = true;
    }
    
    if (needsBuffer) {
        QJsonObject buffer;
        buffer["byteLength"] = static_cast<qint64>(binaryChunk.size());
        QJsonArray buffers;
        buffers.append(buffer);
        newGltf["buffers"] = buffers;
    }

    return writeGLB(outputGLB, newGltf, binaryChunk);
}

bool LoLModelDownloader::fixTransforms(const QString& inputGLB, const QString& outputGLB, const QString& modelId)
{
    QByteArray binaryChunk;
    QJsonObject gltf = parseGLB(inputGLB, binaryChunk);
    
    if (gltf.isEmpty()) {
        return false;
    }
    

    
    if (gltf.contains("nodes") && gltf["nodes"].isArray()) {
        QJsonArray nodes = gltf["nodes"].toArray();
        bool changed = false;
        
        for (int i = 0; i < nodes.size(); ++i) {
            QJsonObject node = nodes[i].toObject();
            QString nodeName = node["name"].toString();
            
            // Apply common fixes
            if (nodeName.contains("Mesh", Qt::CaseInsensitive) || 
                nodeName.contains("Skeleton", Qt::CaseInsensitive)) {
                QJsonArray scale;
                scale.append(-1);
                scale.append(1);
                scale.append(1);
                node["scale"] = scale;
                nodes[i] = node;
                changed = true;
            }
        }
        
        if (changed) {
            gltf["nodes"] = nodes;
        }
    }

    bool hasBufferViews = gltf.contains("bufferViews") && gltf["bufferViews"].isArray() && 
                          !gltf["bufferViews"].toArray().isEmpty();
    
    if (!gltf.contains("buffers") || !gltf["buffers"].isArray() || gltf["buffers"].toArray().isEmpty()) {
        // No buffer info - add if buffer views exist or binary chunk exists
        if (hasBufferViews || !binaryChunk.isEmpty()) {
            QJsonObject buffer;
            buffer["byteLength"] = static_cast<qint64>(binaryChunk.size());
            QJsonArray buffers;
            buffers.append(buffer);
            gltf["buffers"] = buffers;
        }
    }
    
    return writeGLB(outputGLB, gltf, binaryChunk);
}

QStringList LoLModelDownloader::detectChromaIDs(const QString& modelId, const QString& championName, const QString& glbPath)
{
    QStringList chromaIDs;
    int baseId = modelId.toInt();
    
    Vortimsg(QString("  Champion: %1, Model ID: %2").arg(championName).arg(modelId));
    
    // Get texture names from GLB for CDN checking
    QStringList textureNamesForCDN;
    if (!glbPath.isEmpty()) {
        textureNamesForCDN = getTextureNamesFromGLB(glbPath);
        if (!textureNamesForCDN.isEmpty()) {
            Vortimsg(QString("  Extracted %1 texture names from GLB (for CDN check)").arg(textureNamesForCDN.size()));
        }
    }
    
    // Determine texture names
    QStringList testTextures;
    if (!textureNamesForCDN.isEmpty()) {
        testTextures = textureNamesForCDN.mid(0, 3); // First 3 textures
        Vortimsg("  Checking chromas on CDN...");
    } else {
        Vortimsg("  [WARNING] No texture names found in GLB, cannot detect chromas");
        return chromaIDs;
    }
    
    // CDN check - sped up with PARALLEL REQUESTS
    QNetworkAccessManager cdnManager;
    QSet<QString> foundChromaIDs;
    QList<QNetworkReply*> pendingReplies;
    QMap<QNetworkReply*, QString> replyToChromaId; // Reply -> Chroma ID mapping
    
    // Send all requests in parallel (HEAD request - faster)
    for (int offset = -30; offset <= 30; ++offset) {
        if (offset == 0) continue;
        
        int testId = baseId + offset;
        QString testIdStr = QString::number(testId);
        
        // Check with first texture (most common)
        QString testUrl = QString("https://cdn.modelviewer.lol/lol/models/%1/%2/chromas/%3/%4")
                         .arg(championName.toLower(), modelId, testIdStr, testTextures.first());
        
        QUrl urlObj(testUrl);
        QNetworkRequest cdnRequest(urlObj);
        cdnRequest.setHeader(QNetworkRequest::UserAgentHeader, 
                            "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
        
        QNetworkReply* cdnReply = cdnManager.head(cdnRequest); // HEAD request - only checks headers, faster
        pendingReplies.append(cdnReply);
        replyToChromaId[cdnReply] = testIdStr;
    }
    
    // Wait for all requests to complete (parallel)
    QEventLoop cdnLoop;
    int completedCount = 0;
    int totalRequests = pendingReplies.size();
    QMutex countMutex;
    
    for (QNetworkReply* reply : pendingReplies) {
        QObject::connect(reply, &QNetworkReply::finished, [&, reply, totalRequests]() {
            QString chromaId = replyToChromaId[reply];
            
            if (reply->error() == QNetworkReply::NoError && 
                reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
                // Texture found, add chroma ID
                if (!foundChromaIDs.contains(chromaId)) {
                    foundChromaIDs.insert(chromaId);
                    Vortimsg(QString("      Found: %1").arg(chromaId));
                }
            }
            
            reply->deleteLater();
            
            countMutex.lock();
            completedCount++;
            bool allDone = (completedCount >= totalRequests);
            countMutex.unlock();
            
            if (allDone) {
                cdnLoop.quit();
            }
        });
    }
    
    // Add timeout (for all requests)
    QTimer cdnTimer;
    cdnTimer.setSingleShot(true);
    QObject::connect(&cdnTimer, &QTimer::timeout, [&]() {
        countMutex.lock();
        bool allDone = (completedCount >= totalRequests);
        countMutex.unlock();
        
        if (!allDone) {
            // Timeout occurred, cancel remaining requests
            for (QNetworkReply* reply : pendingReplies) {
                if (reply && !reply->isFinished()) {
                    reply->abort();
                }
            }
            cdnLoop.quit();
        }
    });
    cdnTimer.start(5000); // 5 seconds timeout (shorter for parallel)
    
    cdnLoop.exec();
    
    // Convert to sorted list
    QList<int> sortedIds;
    for (const QString& idStr : foundChromaIDs) {
        sortedIds.append(idStr.toInt());
    }
    std::sort(sortedIds.begin(), sortedIds.end());
    for (int id : sortedIds) {
        chromaIDs.append(QString::number(id));
    }
    
    if (!chromaIDs.isEmpty()) {
        Vortimsg(QString("  Found %1 chroma(s) from CDN").arg(chromaIDs.size()));
    } else {
        Vortimsg("  [WARNING] No chromas found on CDN");
    }
    
    return chromaIDs;
}

bool LoLModelDownloader::downloadChromaTextures(const QString& championName, const QString& modelId,
                                                 const QString& chromaId, const QString& outputDir, const QString& glbPath)
{
    // Get texture names from GLB
    QStringList textureNames = getTextureNamesFromGLB(glbPath);
    
    if (textureNames.isEmpty()) {
        textureNames = getTextureNamesFromMaterials(glbPath, championName);
    }
    
    if (textureNames.isEmpty()) {
        Vortimsg("  [WARNING] No texture names found");
        return false;
    }
    
    QDir().mkpath(outputDir);
    
    // Download textures
    QNetworkAccessManager manager;
    int downloaded = 0;
    
    for (const QString& textureName : textureNames) {
        QString urlStr = QString("https://cdn.modelviewer.lol/lol/models/%1/%2/chromas/%3/%4")
                         .arg(championName.toLower(), modelId, chromaId, textureName);
        
        QUrl urlObj(urlStr);
        QNetworkRequest request(urlObj);
        request.setHeader(QNetworkRequest::UserAgentHeader, 
                         "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
        
        QNetworkReply* reply = manager.get(request);
        
        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        QTimer timer;
        timer.setSingleShot(true);
        QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        timer.start(5000);
        loop.exec();
        
        if (reply->error() == QNetworkReply::NoError && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
            QByteArray data = reply->readAll();
            QString filepath = QDir(outputDir).filePath(textureName);
            
            QFile file(filepath);
            if (file.open(QIODevice::WriteOnly)) {
                file.write(data);
                file.close();
                downloaded++;
            }
        }
        
        reply->deleteLater();
    }
    
    Vortimsg(QString("  Downloaded %1/%2 textures").arg(downloaded).arg(textureNames.size()));
    return downloaded > 0;
}

bool LoLModelDownloader::createChromaGLB(const QString& inputGLB, const QString& outputGLB, const QString& textureDir)
{
    Vortimsg("  Creating chroma GLB...");
    
    // 1. Parse GLB
    QByteArray binaryChunk;
    QJsonObject gltf = parseGLB(inputGLB, binaryChunk);
    
    if (gltf.isEmpty()) {
        Vortimsg("  ✗ Failed to parse GLB");
        return false;
    }
    
    if (binaryChunk.isEmpty()) {
        Vortimsg("  [ERROR] Binary chunk not found");
        return false;
    }
    
    // 2. Load chroma textures from directory
    QDir dir(textureDir);
    if (!dir.exists()) {
        Vortimsg(QString("  [WARNING] Chroma texture directory not found: %1").arg(textureDir));
        return false;
    }
    
    QMap<QString, QByteArray> chromaTextures;
    QStringList filters;
    filters << "*.png" << "*.jpg" << "*.jpeg";
    QFileInfoList textureFiles = dir.entryInfoList(filters, QDir::Files);
    
    for (const QFileInfo& fileInfo : textureFiles) {
        QFile file(fileInfo.absoluteFilePath());
        if (file.open(QIODevice::ReadOnly)) {
            chromaTextures[fileInfo.fileName()] = file.readAll();
            file.close();
        }
    }
    
    if (chromaTextures.isEmpty()) {
        Vortimsg(QString("  [WARNING] No chroma texture files found in: %1").arg(textureDir));
        return false;
    }
    
    Vortimsg(QString("  Found %1 chroma texture(s)").arg(chromaTextures.size()));
    
    // 3. Copy existing buffer views, images, textures
    QJsonArray newBufferViews;
    QMap<int, int> bufferViewIndexMapping;
    if (gltf.contains("bufferViews") && gltf["bufferViews"].isArray()) {
        QJsonArray bufferViews = gltf["bufferViews"].toArray();
        for (int i = 0; i < bufferViews.size(); ++i) {
            newBufferViews.append(bufferViews[i]);
            bufferViewIndexMapping[i] = i;
        }
    }
    
    QJsonArray newImages;
    QMap<int, int> imageIndexMapping;
    if (gltf.contains("images") && gltf["images"].isArray()) {
        QJsonArray images = gltf["images"].toArray();
        for (int i = 0; i < images.size(); ++i) {
            newImages.append(images[i]);
            imageIndexMapping[i] = i;
        }
    }
    
    QJsonArray newTextures;
    QMap<int, int> textureIndexMapping;
    if (gltf.contains("textures") && gltf["textures"].isArray()) {
        QJsonArray textures = gltf["textures"].toArray();
        for (int i = 0; i < textures.size(); ++i) {
            newTextures.append(textures[i]);
            textureIndexMapping[i] = i;
        }
    }
    
    // 4. Add chroma textures to binary chunk and create buffer views/images/textures
    QByteArray newBinaryChunk = binaryChunk;
    
    for (auto it = chromaTextures.begin(); it != chromaTextures.end(); ++it) {
        QString textureName = it.key();
        QByteArray textureData = it.value();
        
        // Add to binary chunk (4-byte aligned)
        int offset = newBinaryChunk.size();
        int padding = calculatePadding(offset);
        newBinaryChunk.append(QByteArray(padding, '\0'));
        offset = newBinaryChunk.size();
        newBinaryChunk.append(textureData);
        
        // Create BufferView
        QJsonObject bufferView;
        bufferView["buffer"] = 0;
        bufferView["byteOffset"] = offset;
        bufferView["byteLength"] = textureData.size();
        int bvIdx = newBufferViews.size();
        newBufferViews.append(bufferView);
        
        // Create Image
        QJsonObject image;
        QString mimeType = textureName.toLower().endsWith(".png") ? "image/png" : "image/jpeg";
        image["mimeType"] = mimeType;
        image["bufferView"] = bvIdx;
        int imgIdx = newImages.size();
        newImages.append(image);
        
        bool textureMatched = false;
        QString chromaTextureNameBase = QFileInfo(textureName).baseName().toLower();
        
        for (int texIdx = 0; texIdx < newTextures.size(); ++texIdx) {
            QJsonObject tex = newTextures[texIdx].toObject();
            if (tex.contains("source")) {
                int oldImgIdx = tex["source"].toInt();
                if (oldImgIdx < gltf["images"].toArray().size()) {
                    QJsonObject oldImg = gltf["images"].toArray()[oldImgIdx].toObject();
                    
                    QString oldTextureName;
                    if (oldImg.contains("uri")) {
                        oldTextureName = QFileInfo(oldImg["uri"].toString()).baseName().toLower();
                    } else if (oldImg.contains("name")) {
                        oldTextureName = QFileInfo(oldImg["name"].toString()).baseName().toLower();
                    }
                    
                    if (!oldTextureName.isEmpty() && chromaTextureNameBase == oldTextureName) {
                        // Update texture source to new image
                        tex["source"] = imgIdx;
                        newTextures[texIdx] = tex;
                        textureMatched = true;
                        break;
                    }
                }
            }
        }
        
        // If no match found, create new texture
        if (!textureMatched) {
            int samplerIdx = 0;
            if (gltf.contains("samplers") && gltf["samplers"].isArray() && 
                !gltf["samplers"].toArray().isEmpty()) {
                samplerIdx = 0; // Use first sampler
            }
            
            QJsonObject newTexture;
            newTexture["source"] = imgIdx;
            newTexture["sampler"] = samplerIdx;
            newTextures.append(newTexture);
        }
    }
    
    // 5. Update GLTF
    QJsonObject newGltf = gltf;
    newGltf["bufferViews"] = newBufferViews;
    newGltf["images"] = newImages;
    newGltf["textures"] = newTextures;
    
    // Update buffer size
    if (newGltf.contains("buffers") && newGltf["buffers"].isArray() && 
        !newGltf["buffers"].toArray().isEmpty()) {
        QJsonArray buffers = newGltf["buffers"].toArray();
        QJsonObject buffer = buffers[0].toObject();
        buffer["byteLength"] = static_cast<qint64>(newBinaryChunk.size());
        buffers[0] = buffer;
        newGltf["buffers"] = buffers;
    } else {
        // Add buffer if it doesn't exist
        QJsonObject buffer;
        buffer["byteLength"] = static_cast<qint64>(newBinaryChunk.size());
        QJsonArray buffers;
        buffers.append(buffer);
        newGltf["buffers"] = buffers;
    }
    
    // 6. Write GLB
    if (!writeGLB(outputGLB, newGltf, newBinaryChunk)) {
        Vortimsg("  [ERROR] Failed to write chroma GLB");
        return false;
    }
    
    Vortimsg(QString("  Chroma GLB created: %1").arg(QFileInfo(outputGLB).fileName()));
    return true;
}

QStringList LoLModelDownloader::getTextureNamesFromGLB(const QString& filepath)
{
    QStringList textureNames;
    
    QByteArray binaryChunk;
    QJsonObject gltf = parseGLB(filepath, binaryChunk);
    
    if (gltf.isEmpty()) {
        return textureNames;
    }
    
    // Extract texture names from images — one per image, in order (index must match)
    if (gltf.contains("images") && gltf["images"].isArray()) {
        QJsonArray images = gltf["images"].toArray();
        
        for (int i = 0; i < images.size(); ++i) {
            QJsonObject image = images[i].toObject();
            QString textureName;
            
            // Prefer uri (external file reference)
            if (image.contains("uri")) {
                QString uri = image["uri"].toString();
                if (!uri.startsWith("data:")) {
                    textureName = QFileInfo(uri).fileName();
                }
            }
            
            // Fallback to name field (embedded textures)
            if (textureName.isEmpty() && image.contains("name")) {
                textureName = image["name"].toString();
                if (!textureName.isEmpty() && !textureName.endsWith(".png") && !textureName.endsWith(".jpg") && !textureName.endsWith(".jpeg")) {
                    textureName += ".png";
                }
            }
            
            // Last resort — use index
            if (textureName.isEmpty()) {
                textureName = QString("texture_%1.png").arg(i);
            }
            
            textureNames.append(textureName);
        }
    }
    
    return textureNames;
}

QStringList LoLModelDownloader::getTextureNamesFromMaterials(const QString& filepath, const QString& championName)
{
    QStringList textureNames;
    
    QByteArray binaryChunk;
    QJsonObject gltf = parseGLB(filepath, binaryChunk);
    
    if (gltf.isEmpty()) {
        return textureNames;
    }
    
    // Extract material names and generate texture names
    if (gltf.contains("materials") && gltf["materials"].isArray()) {
        QJsonArray materials = gltf["materials"].toArray();
        
        for (int i = 0; i < materials.size(); ++i) {
            QJsonObject material = materials[i].toObject();
            QString matName = material["name"].toString();
            
            if (!matName.isEmpty()) {
                // Generate variations
                textureNames.append(QString("%1_MAT.png").arg(matName));
                textureNames.append(QString("%1.png").arg(matName));
                textureNames.append(QString("%1_%2_MAT.png").arg(championName, matName));
            }
        }
    }
    
    return textureNames;
}



QByteArray LoLModelDownloader::alignTo4Bytes(const QByteArray& data)
{
    int padding = calculatePadding(data.size());
    QByteArray aligned = data;
    aligned.append(QByteArray(padding, '\0'));
    return aligned;
}

int LoLModelDownloader::calculatePadding(int offset)
{
    return (4 - (offset % 4)) % 4;
}

bool LoLModelDownloader::filterAndExportGLB(const QString& inputGLB, const QString& outputGLB, const QStringList& allowedMeshNames)
{
    Vortimsg("Filtering and exporting GLB...");
    QByteArray binaryChunk;
    QJsonObject gltf = parseGLB(inputGLB, binaryChunk);
    
    if (gltf.isEmpty()) {
        Vortimsg("Failed to parse GLB");
        return false;
    }
    
    if (!gltf.contains("nodes") || !gltf["nodes"].isArray()) {
        Vortimsg("Invalid GLB structure (no nodes)");
        return false;
    }
    
    QJsonArray nodes = gltf["nodes"].toArray();
    QJsonArray meshes;
    if (gltf.contains("meshes") && gltf["meshes"].isArray()) {
        meshes = gltf["meshes"].toArray();
    }
    
    int hiddenCount = 0;
    
    for (int i = 0; i < nodes.size(); ++i) {
        QJsonObject node = nodes[i].toObject();
        
        if (node.contains("mesh")) {
            int meshIdx = node["mesh"].toInt();
            QString effectiveName;
            QString nodeName = node["name"].toString();
            QString meshName;
            
            if (meshIdx >= 0 && meshIdx < meshes.size()) {
                meshName = meshes[meshIdx].toObject()["name"].toString();
            }
            
            // Logic must match GLBViewer::loadModelWithAssimp naming Vortimsgic
            // 1. Try mesh name first
            effectiveName = meshName;
            
            // 2. If generic or empty, use node name
            if (effectiveName.isEmpty() || effectiveName.startsWith("Mesh", Qt::CaseInsensitive)) {
                if (!nodeName.isEmpty()) {
                    effectiveName = nodeName;
                }
            }
            
            // Check if allowed
            bool allowed = false;
            for (const QString& allowedName : allowedMeshNames) {
                if (effectiveName == allowedName) {
                    allowed = true;
                    break;
                }
            }
            
            if (!allowed) {
                // Hide mesh by removing reference to it, preserving the node transform/hierarchy
                node.remove("mesh");
                nodes[i] = node;
                hiddenCount++;
            }
        }
    }
    
    if (hiddenCount > 0) {
        gltf["nodes"] = nodes;
        Vortimsg(QString("  Exported with %1 meshes hidden").arg(hiddenCount));
    } else {
        Vortimsg("  No meshes hidden (all visible or no match)");
    }
    
    return writeGLB(outputGLB, gltf, binaryChunk);
}

