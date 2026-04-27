#pragma once

#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QJsonObject>
#include <QJsonArray>
#include <functional>

class LoLModelDownloader
{
public:
    LoLModelDownloader();
    ~LoLModelDownloader() = default;
    
    // Main download function
    bool downloadModel(
        const QString& championName,
        const QString& modelId,
        const QString& modelName,
        const QString& outputDir,
        bool autoFix = true,
        bool splitPrimitives = true,
        bool downloadChromas = true
    );
    
    // Callbacks
    void setProgressCallback(std::function<void(int)> callback) { m_progressCallback = callback; }
    void setLogCallback(std::function<void(const QString&)> callback) { m_logCallback = callback; }
    
    
    // Direct file download (public for viewer access)
    bool downloadGLB(const QString& url, const QString& outputPath);
    
    // Chroma detection (public for viewer access)
    QStringList detectChromaIDs(const QString& modelId, const QString& championName, const QString& glbPath);
    
    // Filter and export GLB based on allowed mesh names
    bool filterAndExportGLB(const QString& inputGLB, const QString& outputGLB, const QStringList& allowedMeshNames);

private:
    // GLB parsing and writing
    QJsonObject parseGLB(const QString& filepath, QByteArray& binaryChunk);
    bool writeGLB(const QString& filepath, const QJsonObject& gltf, const QByteArray& binaryChunk);
    
    // Mesh processing
    bool splitMeshPrimitives(const QString& inputGLB, const QString& outputGLB);

    

    
    // Transform fixing
    bool fixTransforms(const QString& inputGLB, const QString& outputGLB, const QString& modelId);

    
    // Chroma download utilities
    bool downloadChromaTextures(const QString& championName, const QString& modelId, 
                                const QString& chromaId, const QString& outputDir, const QString& glbPath);
    bool createChromaGLB(const QString& inputGLB, const QString& outputGLB, const QString& textureDir);
    
    // Texture utilities
    QStringList getTextureNamesFromGLB(const QString& filepath);
    QStringList getTextureNamesFromMaterials(const QString& filepath, const QString& championName);
    
    // Helper functions
    void progress(int percent);
    QByteArray alignTo4Bytes(const QByteArray& data);
    int calculatePadding(int offset);
    
    // Callbacks
    std::function<void(int)> m_progressCallback;
    std::function<void(const QString&)> m_logCallback;
};

