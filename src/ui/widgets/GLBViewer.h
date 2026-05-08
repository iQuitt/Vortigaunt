#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QMatrix4x4>
#include <QVector3D>
#include <QQuaternion>
#include <QString>
#include <QTimer>
#include <QElapsedTimer>
#include <QStringList>
#include <functional>
#include <memory>
#include <vector>
#include <set>
#include <QNetworkAccessManager>
#include <QNetworkReply>

// Forward declarations
struct aiScene;
struct aiAnimation;
struct aiNode;
struct aiNodeAnim;
struct aiVectorKey;
struct aiQuatKey;
struct aiBone;

struct MeshData {
    QOpenGLVertexArrayObject* vao;
    QOpenGLBuffer* vertexBuffer;
    QOpenGLBuffer* indexBuffer;
    QOpenGLBuffer* boneIndexBuffer;  // Buffer for bone indices
    QOpenGLBuffer* boneWeightBuffer;  // Buffer for bone weights
    int indexCount;
    QVector3D center;
    float radius;
    unsigned int diffuseTexture;  // OpenGL texture ID
    bool hasTexture;
    const aiNode* node;  // Node this mesh belongs to (for animation)
    QMatrix4x4 baseTransform;  // Base transform from node hierarchy
    bool hasBones;  // Whether this mesh has skeletal animation
    unsigned int numBones;  // Number of bones affecting this mesh
    std::vector<QMatrix4x4> boneTransforms;  // Current bone transform matrices
    std::vector<aiBone*> bonePointers;  // Pointers to aiBone structures for this mesh
    std::vector<const aiNode*> boneNodes;  // Cached bone nodes (for performance)
    std::vector<int> flatBoneIndices; // Indices into m_flatNodes for O(1) access
    std::vector<QMatrix4x4> boneOffsetMatrices;  // Cached offset matrices (for performance)
    unsigned int sceneMeshIndex;  // Index of this mesh in the scene
    QString name;  // Mesh name from GLB
    bool visible = true;  // Whether to render this mesh
    unsigned int baseTexture = 0;  // Original texture ID (for chroma switching)
    int textureImageIndex = -1;  // GLB images[] index for chroma texture matching
};

// Chroma data structure for texture variants
struct ChromaData {
    QString id;
    QString name;
    std::vector<unsigned int> meshTextures; // Texture ID per mesh
};

class GLBViewer : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core
{
    Q_OBJECT

public:
    explicit GLBViewer(QWidget* parent = nullptr);
    ~GLBViewer() override;

    bool loadGLB(const QString& filepath);
    void clearModel();
    
    // Animation controls
    void playAnimation();
    void pauseAnimation();
    void stopAnimation();
    void setAnimationTime(float time);
    float getAnimationTime() const;
    float getAnimationDuration() const;
    QStringList getAnimationNames() const;
    void setCurrentAnimation(const QString& animationName);
    
    QString getCurrentModelPath() const { return m_currentModelPath; }
    
    // Mesh visibility controls
    QStringList getMeshNames() const;
    void setMeshVisible(int index, bool visible);
    bool isMeshVisible(int index) const;
    int getMeshCount() const;
    std::set<unsigned int> getHiddenMeshIndices() const;
    
    // Chroma controls
    void loadChromaTextures(const QString& championName, const QString& modelId, const QStringList& chromaIds);
    QStringList getChromaNames() const;
    void setCurrentChroma(int index); // -1 for base texture
    int getCurrentChromaIndex() const;
    
    // Log callback
    void setLogCallback(std::function<void(const QString&)> callback) { m_logCallback = callback; }

signals:
    void modelLoaded(bool success);
    void chromasLoaded(int count);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void paintEvent(QPaintEvent* event) override;
    
    void showEvent(QShowEvent* event) override;
    
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void log(const QString& message);
    bool loadModelWithAssimp(const QString& filepath);
    unsigned int loadTexture(const aiScene* scene, const char* texturePath, const QString& modelPath);
    void setupShaders();
    void cleanup();
    void updateAnimationTransforms(float time);
    void updateBoneTransforms();
    QMatrix4x4 getNodeTransform(const aiNode* node, float animationTime, const aiAnimation* animation);
    QMatrix4x4 interpolateTranslation(const aiVectorKey* keys, unsigned int numKeys, float time);
    QMatrix4x4 interpolateRotation(const aiQuatKey* keys, unsigned int numKeys, float time);
    QMatrix4x4 interpolateScaling(const aiVectorKey* keys, unsigned int numKeys, float time);
    
    QOpenGLShaderProgram* m_shaderProgram;
    std::vector<std::unique_ptr<MeshData>> m_meshes;
    
    // Camera
    QVector3D m_cameraPosition;
    QVector3D m_cameraTarget;
    float m_cameraDistance;
    float m_cameraRotationX;
    float m_cameraRotationY;
    
    // Mouse controls
    QPoint m_lastMousePos;
    bool m_mousePressed;
    
    // Matrices
    QMatrix4x4 m_projectionMatrix;
    QMatrix4x4 m_viewMatrix;
    
    QVector3D m_modelCenter;
    float m_modelRadius;
    
    bool m_cameraChanged;
    bool m_projectionChanged;
    float m_lastAnimationTime;
    
    // Animation
    struct AnimationData {
        QString name;
        float duration;
        unsigned int numChannels;
        const aiAnimation* animation;  // Pointer to animation in scene
    };
    std::vector<AnimationData> m_animations;
    int m_currentAnimationIndex;
    float m_animationTime;
    bool m_animationPlaying;
    QTimer* m_animationTimer;
    
    struct NodeInfo {
        const aiNode* node;
        int parentIndex; // Index into m_flatNodes
        QMatrix4x4 globalTransform;
        const aiNodeAnim* nodeAnim; // Cached animation channel
    };
    std::vector<NodeInfo> m_flatNodes;
    
    const aiScene* m_scene;  // Keep scene for animation (NOTE: owned by Assimp::Importer)
    class AssimpImporterHolder;  // Forward declaration
    std::unique_ptr<AssimpImporterHolder> m_importerHolder;  // Keep importer alive to maintain scene
    QMatrix4x4 m_globalInverseTransform;  // Inverse of root node transform for skeletal animation
    
    std::function<void(const QString&)> m_logCallback;
    bool m_glInitialized;
    
    // Animation timing
    QElapsedTimer m_animationElapsedTimer;
    
    // Chroma support
    std::vector<ChromaData> m_chromas;
    int m_currentChromaIndex = -1; // -1 = base texture
    QString m_currentChampionName;
    QString m_currentModelId;
    QString m_currentModelPath;
    QString m_loadedGlbPath;  // Persisted GLB path for chroma texture name extraction
    
    QNetworkAccessManager* m_networkManager = nullptr;
    int m_pendingChromaDownloads = 0;
};

