// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#pragma once

#include <QColor>
#include <QElapsedTimer>
#include <QImage>
#include <QMatrix4x4>
#include <QOpenGLFunctions_3_3_Core>
#include <QWidget>
#include <QPoint>
#include <QVector3D>

#include <assimp/matrix4x4.h>

#include <functional>
#include <memory>
#include <vector>

class QOffscreenSurface;
class QOpenGLContext;
class QOpenGLPaintDevice;
class QOpenGLShaderProgram;
class QTimer;
class StudioModel;
struct StudioSequence;
struct StudioSubModel;
struct StudioTexture;

// renders to its own native window, or offscreen when grabbed
class StudioModelView : public QWidget, protected QOpenGLFunctions_3_3_Core
{
    Q_OBJECT
public:
    explicit StudioModelView(QWidget* parent = nullptr);
    ~StudioModelView() override;

    int addModel(std::shared_ptr<const StudioModel> model, int mergeInto = -1);
    void clearModels();
    // same model edited: display and playback state kept
    void replaceModel(int index, std::shared_ptr<const StudioModel> model);
    int modelCount() const { return static_cast<int>(m_models.size()); }
    std::shared_ptr<const StudioModel> model(int index) const;

    void setSequence(int model, int sequence);
    int sequence(int model) const;
    void setGaitSequence(int model, int sequence);
    void setAimPitch(int model, float degrees);
    // first model's frame; framePosition includes the fraction toward the next
    int frame() const;
    float framePosition() const;
    void setFrame(int frame);
    void setFramePosition(float frame);
    void setPlaying(bool playing);
    bool isPlaying() const { return m_playing; }
    void setTime(float seconds);
    float time() const { return m_time; }
    // non-looping sequences hold their last frame
    void setHoldUnlooped(bool hold);
    enum Looping { SequenceLooping, AlwaysLoop, NeverLoop };
    void setLooping(int looping);
    void setSpeed(float speed);
    // rest pose
    void setRestPose(int model, bool rest);
    void setBody(int model, int part, int subModel);
    void setSkin(int model, int skin);
    // one value per bone controller
    void setControllers(int model, const std::vector<float>& values);
    // hues 0-255 for remap textures; -1 keeps them
    void setColormap(int model, int top, int bottom);
    // transform of a whole model
    void setPlacement(int model, const aiMatrix4x4& placement);

    void setBoneOffset(int model, int bone, const aiMatrix4x4& offset);
    aiMatrix4x4 boneTransform(int model, int bone) const;

    void fitView(int onlyModel = -1);
    void setCameraAngles(float yaw, float pitch);
    void setOrthographic(bool orthographic);
    enum Camera { ArcBall, FreeLook, FirstPerson };
    void setCamera(int camera);
    void setFieldOfView(float degrees);
    struct ViewPlace
    {
        QVector3D target, freeEye;
        float distance = 100.0f, yaw = 30.0f, pitch = 15.0f;
    };
    ViewPlace viewPlace() const;
    void setViewPlace(const ViewPlace& place);
    // invalid colour = palette's
    void setBackground(const QColor& colour);

    struct Display
    {
        enum Mode { Textured, Smooth, Flat, Wireframe };
        int mode = Textured;
        float opacity = 1.0f;
        bool backfaceCulling = true, wireframeOverlay = false;
        bool hitboxes = false, bones = false, attachments = false, eyePosition = false;
        bool boundingBox = false, clippingBox = false, normals = false, axes = false;
        bool ground = false, playerBox = false, crosshair = false;
        bool mirrorX = false, mirrorY = false, mirrorZ = false;
        // shadows on the ground; fixShadowZFighting lifts them slightly
        bool shadows = false, fixShadowZFighting = false;
        // aspect frame; offscreenAreas darkens outside it
        bool guidelines = false, offscreenAreas = true;
        int aspectWidth = 16, aspectHeight = 9;
    };
    void setDisplay(const Display& display);
    const Display& display() const { return m_display; }

    struct Scene
    {
        float groundLength = 384.0f;  // half size
        QVector3D groundOrigin;
        QImage groundTexture;
        bool groundTextured = false;
        bool tiling = false;
        int tileSize = 64;
        bool mirror = false;  // reflect models in the ground
        QImage background;
        bool showBackground = false;
    };
    void setScene(const Scene& scene);
    const Scene& scene() const { return m_scene; }
    // highlights in the first model; -1 for none
    void setHighlight(int bone, int attachment, int hitbox);
    float framesPerSecond() const { return m_fps; }
    int drawnTriangles() const { return m_triangles; }
    bool bounds(QVector3D& low, QVector3D& high, int onlyModel = -1) const;
    // offscreen render at the view's size
    QImage grabFramebuffer();
    // RGBA texture, remapped when its name asks
    static QImage textureImage(const StudioTexture& texture, int top = -1, int bottom = -1);

    // see-through box drawn among the models
    struct Guide
    {
        QVector3D middle, half;
        float yaw = 0.0f;  // about Z, in radians
    };
    void setGuides(const std::vector<Guide>& guides);

    // Blender-style X/Y/Z rotation rings, reported by turnedByHandle
    void setTurnHandles(bool shown);
    // re-centre on the models, keeping distance and angles
    void recenter();
    // a left click (not a drag) on the first model emits surfacePicked
    void setSurfacePicking(bool on);

    QString placeholder;

signals:
    void turnedByHandle(int axis, float degrees);
    // position in the bone's space
    void surfacePicked(int bone, const QVector3D& position);

protected:
    void showEvent(QShowEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    class Surface;

    // creates the context on first use; current on the offscreen surface
    bool makeCurrent();
    void doneCurrent();
    // schedule the next frame
    void redraw();
    void renderToWindow();
    void paintFrame(unsigned int framebuffer, const QSize& pixels, qreal ratio);
    void initializeGL();
    void paintGL();

    struct Mesh
    {
        int texture = -1;
        int first = 0;  // in the model's vertex buffer
        int count = 0;
        int skinRef = -1;
    };

    struct Model
    {
        std::shared_ptr<const StudioModel> data;
        int mergeInto = -1;
        std::vector<int> mergeBones;  
        int sequence = 0;
        int gaitSequence = -1;
        bool rest = false;
        std::vector<int> body;  // shown submodel per body part
        int skin = 0;
        std::vector<float> controllers;
        int top = -1, bottom = -1;
        bool arranged = false;  // meshes match the body
        bool textured = false;  // textures match the hues
        float aimPitch = 0.0f;
        int offsetBone = -1;
        aiMatrix4x4 offset;
        aiMatrix4x4 placement;
        std::vector<aiMatrix4x4> bones;  
        std::vector<Mesh> meshes;
        std::vector<unsigned int> textures; 
        unsigned int vao = 0;
        unsigned int vbo = 0;
        bool uploaded = false;
    };

    void updateBones();
    // current frame, with fraction, at the view's time
    float frameOf(const StudioSequence& sequence) const;
    const StudioSubModel* shownSubModel(const Model& model, size_t part) const;
    void arrange(Model& model);
    void uploadTextures(Model& model);
    void releaseTextures(Model& model);
    void buildVertices(const Model& model, std::vector<float>& out) const;
    void upload(Model& model);
    void release(Model& model);
    void releaseAll();
    void drawModel(Model& model, bool additivePass);
    void drawGuides();
    void bindGuideBuffer();
    // points: xyz each
    void drawLines(const std::vector<float>& points, const QColor& colour, unsigned int primitive);
    void drawOverlays();
    void drawBackground();
    // reflection, ground and shadows; restoreMode resets the draw mode
    void drawScenery(const std::function<void()>& restoreMode);
    void drawGuidelines();
    unsigned int sceneTexture(unsigned int& texture, const QImage& image, bool& dirty);
    void drawHandles();
    QPointF onScreen(const QVector3D& p) const;
    QVector3D ringPoint(int axis, float turn) const;
    // the ring under a point, or -1
    int handleAt(const QPointF& at, float* turn) const;
    float handleTurn(const QPointF& from, const QPointF& to) const;
    void pickSurface(const QPointF& at);

    std::vector<Model> m_models;
    std::vector<Guide> m_guides;
    unsigned int m_guideVao = 0;
    unsigned int m_guideVbo = 0;
    Scene m_scene;
    unsigned int m_groundTexture = 0, m_backgroundTexture = 0;
    bool m_groundDirty = false, m_backgroundDirty = false;
    QOpenGLShaderProgram* m_program = nullptr;
    QOpenGLContext* m_context = nullptr;
    QOffscreenSurface* m_offscreen = nullptr;
    Surface* m_surface = nullptr;
    std::unique_ptr<QOpenGLPaintDevice> m_paintDevice;
    bool m_glReady = false;
    bool m_fitPending = false;

    QElapsedTimer m_clock;
    bool m_playing = true;
    float m_time = 0.0f;
    bool m_holdUnlooped = false;
    bool m_orthographic = false;
    QColor m_background;
    Display m_display;
    int m_highlightBone = -1, m_highlightAttachment = -1, m_highlightHitbox = -1;
    int m_looping = SequenceLooping;
    float m_speed = 1.0f;
    int m_camera = ArcBall;
    float m_fieldOfView = 45.0f;
    QVector3D m_freeEye;
    QElapsedTimer m_frameClock;
    float m_fps = 0.0f;
    int m_framesCounted = 0;
    bool m_framePending = false;
    QTimer* m_frameTimer = nullptr;
    QElapsedTimer m_lastFrame;  // start of the last on-screen frame
    int m_triangles = 0;

    QVector3D m_target;
    float m_distance = 100.0f;
    float m_yaw = 30.0f; 
    float m_pitch = 15.0f;
    QPoint m_lastMouse;
    QPoint m_pressedAt;
    bool m_surfacePicking = false;
    Qt::MouseButton m_dragButton = Qt::NoButton;
    QMatrix4x4 m_projection;
    QMatrix4x4 m_viewProjection;
    QVector3D m_eye;
    bool m_turnHandles = false;
    QVector3D m_handleMiddle;
    float m_handleRadius = 0.0f;
    int m_handleAxis = -1;   // the ring dragged
    int m_hoverAxis = -1;
    float m_handleAt = 0.0f; // where on it it was taken
};
