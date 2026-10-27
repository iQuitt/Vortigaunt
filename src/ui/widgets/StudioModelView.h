#pragma once

#include <QColor>
#include <QElapsedTimer>
#include <QMatrix4x4>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLWidget>
#include <QPoint>
#include <QVector3D>

#include <assimp/matrix4x4.h>

#include <memory>
#include <vector>

class QOpenGLShaderProgram;
class QTimer;
class StudioModel;

class StudioModelView : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core
{
    Q_OBJECT
public:
    explicit StudioModelView(QWidget* parent = nullptr);
    ~StudioModelView() override;

    int addModel(std::shared_ptr<const StudioModel> model, int mergeInto = -1);
    void clearModels();
    int modelCount() const { return static_cast<int>(m_models.size()); }
    std::shared_ptr<const StudioModel> model(int index) const;

    void setSequence(int model, int sequence);
    int sequence(int model) const;
    void setGaitSequence(int model, int sequence);
    void setAimPitch(int model, float degrees);
    void setPlaying(bool playing);
    bool isPlaying() const { return m_playing; }
    void setTime(float seconds);
    float time() const { return m_time; }
    // non-looping sequences hold their last frame
    void setHoldUnlooped(bool hold);
    // transform of a whole model
    void setPlacement(int model, const aiMatrix4x4& placement);

    void setBoneOffset(int model, int bone, const aiMatrix4x4& offset);
    aiMatrix4x4 boneTransform(int model, int bone) const;

    void fitView(int onlyModel = -1);
    void setCameraAngles(float yaw, float pitch);
    void setOrthographic(bool orthographic);
    // invalid colour = palette's
    void setBackground(const QColor& colour);

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

    QString placeholder;

signals:
    void turnedByHandle(int axis, float degrees);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    struct Mesh
    {
        int texture = -1;
        int first = 0;  // in the model's vertex buffer
        int count = 0;
    };

    struct Model
    {
        std::shared_ptr<const StudioModel> data;
        int mergeInto = -1;
        std::vector<int> mergeBones;  
        int sequence = 0;
        int gaitSequence = -1;
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
    void buildVertices(const Model& model, std::vector<float>& out) const;
    void upload(Model& model);
    void release(Model& model);
    void releaseAll();
    void drawModel(Model& model, bool additivePass);
    void drawGuides();
    bool bounds(QVector3D& low, QVector3D& high, int onlyModel = -1) const;
    void drawHandles();
    QPointF onScreen(const QVector3D& p) const;
    QVector3D ringPoint(int axis, float turn) const;
    // the ring under a point, or -1
    int handleAt(const QPointF& at, float* turn) const;
    float handleTurn(const QPointF& from, const QPointF& to) const;

    std::vector<Model> m_models;
    std::vector<Guide> m_guides;
    unsigned int m_guideVao = 0;
    unsigned int m_guideVbo = 0;
    QOpenGLShaderProgram* m_program = nullptr;
    bool m_glReady = false;
    bool m_fitPending = false;

    QTimer* m_timer = nullptr;
    QElapsedTimer m_clock;
    bool m_playing = true;
    float m_time = 0.0f;
    bool m_holdUnlooped = false;
    bool m_orthographic = false;
    QColor m_background;

    QVector3D m_target;
    float m_distance = 100.0f;
    float m_yaw = 30.0f; 
    float m_pitch = 15.0f;
    QPoint m_lastMouse;
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
