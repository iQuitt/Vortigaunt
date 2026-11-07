// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#include "StudioModelView.h"

#include "utils/util.hpp"
#include "core/mdl/StudioModel.h"
#include "core/VortigauntLog.h"

#include <QKeyEvent>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLPaintDevice>
#include <QMouseEvent>
#include <QOpenGLShaderProgram>
#include <QPainter>
#include <QRegularExpression>
#include <QScreen>
#include <QSurfaceFormat>
#include <QTimer>
#include <QVBoxLayout>
#include <QVector4D>
#include <QWheelEvent>
#include <QWindow>

#include <algorithm>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")
#endif

#include <cctype>
#include <cmath>

namespace {

constexpr int kFloatsPerVertex = 8;  // position, normal, texture coordinates

const char* kVertexShader = R"(
#version 330 core
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoord;
uniform mat4 viewProjection;
out vec3 vNormal;
out vec3 vPosition;
out vec2 vTexCoord;
void main()
{
    vNormal = normal;
    vPosition = position;
    vTexCoord = texCoord;
    gl_Position = viewProjection * vec4(position, 1.0);
}
)";

const char* kFragmentShader = R"(
#version 330 core
in vec3 vNormal;
in vec3 vPosition;
in vec2 vTexCoord;
uniform sampler2D tex;
uniform int mode;
uniform vec3 lightDirection;
uniform vec4 tint;
uniform float opacity;
out vec4 color;
void main()
{
    if (mode == 5)
    {
        color = tint;
        return;
    }
    if (mode == 4)
    {
        vec3 facing = normalize(cross(dFdx(vPosition), dFdy(vPosition)));
        color = vec4(tint.rgb * min(0.55 + 0.6 * abs(dot(facing, lightDirection)), 1.2), tint.a);
        return;
    }
    float light = 0.55 + 0.6 * max(dot(normalize(vNormal), -lightDirection), 0.0);
    if (mode == 3)
    {
        color = vec4(tint.rgb * min(light, 1.2), tint.a);
        return;
    }
    vec4 texel = texture(tex, vTexCoord);
    if (mode == 1 && texel.a < 0.5)
        discard;
    if (mode == 2)
    {
        color = vec4(texel.rgb * opacity, 1.0);
        return;
    }
    color = vec4(texel.rgb * min(light, 1.2), opacity);
}
)";

constexpr float kPi = 3.14159265358979f;
constexpr int kRingPoints = 96;
constexpr float kHandleReach = 7.0f;  // pixels from a ring that take it
const QColor kAxisColours[3] = { QColor(232, 62, 62), QColor(110, 205, 60), QColor(62, 120, 240) };
const QColor kGroupColours[8] = { QColor(255, 255, 255), QColor(255, 80, 80), QColor(80, 255, 80), QColor(255, 255, 80),
                                  QColor(80, 80, 255), QColor(255, 80, 255), QColor(80, 255, 255), QColor(255, 160, 60) };
constexpr float kNormalLength = 2.0f;
constexpr float kGroundCell = 32.0f;
constexpr float kFreeStep = 8.0f;  // free camera step per key

QVector3D toQt(const aiVector3D& v)
{
    return QVector3D(v.x, v.y, v.z);
}

void addPoint(std::vector<float>& out, const aiVector3D& p)
{
    out.insert(out.end(), { p.x, p.y, p.z });
}

// 12 box edges, transformed
void addBox(std::vector<float>& out, const aiVector3D& low, const aiVector3D& high, const aiMatrix4x4& through)
{
    aiVector3D corner[8];
    for (int i = 0; i < 8; ++i)
        corner[i] = through * aiVector3D((i & 1) ? high.x : low.x, (i & 2) ? high.y : low.y, (i & 4) ? high.z : low.z);
    for (int i = 0; i < 8; ++i)
        for (const int bit : { 1, 2, 4 })
            if (!(i & bit))
            {
                addPoint(out, corner[i]);
                addPoint(out, corner[i | bit]);
            }
}

aiVector3D boneOrigin(const aiMatrix4x4& bone)
{
    return aiVector3D(bone.a4, bone.b4, bone.c4);
}

// remap palette spans from the texture name; false if none
bool remapSpans(const std::string& name, int& low, int& mid, int& high)
{
    const QString lower = QString::fromStdString(name).toLower();
    if (lower == QLatin1String("dm_base.bmp"))
    {
        low = 160;
        mid = 191;
        high = 223;
        return true;
    }
    static const QRegularExpression three(QStringLiteral("^remap._(\\d{3})_(-?\\d{2,3})_(\\d{3})\\.bmp$"));
    static const QRegularExpression two(QStringLiteral("^remapc_(\\d{3})_(\\d{3})\\.bmp$"));
    QRegularExpressionMatch found = three.match(lower);
    if (found.hasMatch())
        high = found.captured(3).toInt();
    else
    {
        found = two.match(lower);
        if (!found.hasMatch())
            return false;
        high = 0;
    }
    low = found.captured(1).toInt();
    mid = found.captured(2).toInt();
    return true;
}

QSurfaceFormat viewFormat()
{
    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    // vsync
    format.setSwapInterval(1);
    return format;
}

} // namespace

// native window the view renders into; input goes back to the view
class StudioModelView::Surface : public QWindow
{
public:
    explicit Surface(StudioModelView* view)
        : m_view(view)
    {
        setSurfaceType(QWindow::OpenGLSurface);
        setFormat(viewFormat());
    }

protected:
    void exposeEvent(QExposeEvent*) override
    {
        if (isExposed())
            m_view->renderToWindow();
    }

    bool event(QEvent* event) override
    {
        switch (event->type())
        {
        case QEvent::UpdateRequest: m_view->renderToWindow(); return true;
        case QEvent::MouseButtonPress:
            requestActivate();
            m_view->mousePressEvent(static_cast<QMouseEvent*>(event));
            return true;
        case QEvent::MouseButtonRelease: m_view->mouseReleaseEvent(static_cast<QMouseEvent*>(event)); return true;
        case QEvent::MouseMove: m_view->mouseMoveEvent(static_cast<QMouseEvent*>(event)); return true;
        case QEvent::MouseButtonDblClick: m_view->mouseDoubleClickEvent(static_cast<QMouseEvent*>(event)); return true;
        case QEvent::Wheel: m_view->wheelEvent(static_cast<QWheelEvent*>(event)); return true;
        case QEvent::KeyPress: m_view->keyPressEvent(static_cast<QKeyEvent*>(event)); return true;
        default: return QWindow::event(event);
        }
    }

private:
    StudioModelView* m_view;
};

StudioModelView::StudioModelView(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(200, 200);
    setFocusPolicy(Qt::ClickFocus);
    m_frameTimer = new QTimer(this);
    m_frameTimer->setSingleShot(true);
    m_frameTimer->setTimerType(Qt::PreciseTimer);
    connect(m_frameTimer, &QTimer::timeout, this, &StudioModelView::renderToWindow);
    m_clock.start();
}

StudioModelView::~StudioModelView()
{
    if (m_context && makeCurrent())
    {
        releaseAll();
        m_paintDevice.reset();
        delete m_program;
        m_program = nullptr;
        doneCurrent();
    }
    delete m_offscreen;
#ifdef Q_OS_WIN
    if (m_surface)
        timeEndPeriod(1);
#endif
}

bool StudioModelView::makeCurrent()
{
    if (m_context)
        return m_context->isValid() && m_context->makeCurrent(m_offscreen);
    m_context = new QOpenGLContext(this);
    m_context->setFormat(viewFormat());
    m_context->setShareContext(QOpenGLContext::globalShareContext());
    m_offscreen = new QOffscreenSurface();
    if (!m_context->create())
        return false;
    m_offscreen->setFormat(m_context->format());
    m_offscreen->create();
    if (!m_context->makeCurrent(m_offscreen))
        return false;
    initializeGL();
    return true;
}

void StudioModelView::doneCurrent()
{
    m_context->doneCurrent();
}

void StudioModelView::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    // hidden views need no native surface; they render only when grabbed
    if (!m_surface && !window()->testAttribute(Qt::WA_DontShowOnScreen))
    {
        m_surface = new Surface(this);
#ifdef Q_OS_WIN
        // 1 ms timer resolution (default 15.6 ms), kept under Windows 11 power throttling
        timeBeginPeriod(1);
        static const bool kept = []() {
            PROCESS_POWER_THROTTLING_STATE state = {};
            state.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
            state.ControlMask = PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION;
            state.StateMask = 0;
            return SetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, &state, sizeof(state)) != 0;
        }();
        Q_UNUSED(kept);
#endif
        QWidget* container = QWidget::createWindowContainer(m_surface, this);
        container->setFocusPolicy(Qt::ClickFocus);
        // otherwise the container forwards drops to the window and back, recursing
        container->setAcceptDrops(false);
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(container);
    }
    redraw();
}

void StudioModelView::paintEvent(QPaintEvent*)
{
    // no native window: paint an offscreen frame so grab() and render() work
    if (m_surface)
        return;
    const QImage frame = grabFramebuffer();
    QPainter painter(this);
    painter.drawImage(rect(), frame);
}

void StudioModelView::redraw()
{
    if (!m_surface)
        update();
    else if (m_surface->isExposed() && !m_framePending)
    {
        // one frame per refresh; on Windows only a precise timer >= 1 ms keeps pace
        // (update requests, posted calls and 0 ms timers wait ~10 ms)
        m_framePending = true;
        const QScreen* screen = m_surface->screen();
        const double period = 1000.0 / (screen && screen->refreshRate() > 1.0 ? screen->refreshRate() : 60.0);
        const double since = m_lastFrame.isValid() ? m_lastFrame.nsecsElapsed() / 1e6 : period;
        m_frameTimer->start(std::max(1, static_cast<int>(std::lround(period - since))));
    }
}

void StudioModelView::renderToWindow()
{
    m_framePending = false;
    m_lastFrame.start();
    if (!m_surface || !m_surface->isExposed() || (!m_context && !makeCurrent()) || !m_context->isValid() || !m_context->makeCurrent(m_surface))
        return;
    const qreal ratio = m_surface->devicePixelRatio();
    paintFrame(m_context->defaultFramebufferObject(), m_surface->size() * ratio, ratio);
    m_context->swapBuffers(m_surface);
    // next frame at display rate while animating
    if (!m_models.empty())
        redraw();
}

void StudioModelView::paintFrame(unsigned int framebuffer, const QSize& pixels, qreal ratio)
{
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glViewport(0, 0, pixels.width(), pixels.height());
    if (!m_paintDevice || m_paintDevice->size() != pixels)
        m_paintDevice = std::make_unique<QOpenGLPaintDevice>(pixels);
    m_paintDevice->setDevicePixelRatio(ratio);
    paintGL();
}

QImage StudioModelView::grabFramebuffer()
{
    if (!makeCurrent())
        return {};
    const qreal ratio = devicePixelRatioF();
    const QSize pixels = QSize(std::max(1, width()), std::max(1, height())) * ratio;
    QOpenGLFramebufferObject target(pixels, QOpenGLFramebufferObject::CombinedDepthStencil);
    target.bind();
    paintFrame(target.handle(), pixels, ratio);
    // opaque: blending leaves junk in alpha
    QImage image = target.toImage().convertToFormat(QImage::Format_RGB32);
    target.release();
    doneCurrent();
    image.setDevicePixelRatio(ratio);
    return image;
}

int StudioModelView::addModel(std::shared_ptr<const StudioModel> model, int mergeInto)
{
    Model entry;
    entry.data = std::move(model);
    entry.mergeInto = mergeInto >= 0 && mergeInto < modelCount() ? mergeInto : -1;
    if (entry.mergeInto >= 0)
    {
        const StudioModel& parent = *m_models[entry.mergeInto].data;
        for (const StudioBone& bone : entry.data->bones)
        {
            int match = -1;
            for (size_t j = 0; j < parent.bones.size() && match < 0; ++j)
            {
                if (String_EqualsNoCase(bone.name, parent.bones[j].name))
                    match = static_cast<int>(j);
            }
            entry.mergeBones.push_back(match);
        }
    }
    m_models.push_back(std::move(entry));
    if (m_models.size() == 1)
        m_fitPending = true;
    redraw();
    return modelCount() - 1;
}

void StudioModelView::clearModels()
{
    if (m_context && makeCurrent())
    {
        releaseAll();
        doneCurrent();
    }
    m_models.clear();
    redraw();
}

void StudioModelView::replaceModel(int index, std::shared_ptr<const StudioModel> model)
{
    if (index < 0 || index >= modelCount() || !model)
        return;
    Model& shown = m_models[index];
    shown.data = std::move(model);
    // rebuild meshes and textures
    shown.arranged = false;
    shown.textured = false;
    redraw();
}

std::shared_ptr<const StudioModel> StudioModelView::model(int index) const
{
    return index >= 0 && index < modelCount() ? m_models[index].data : nullptr;
}

void StudioModelView::setSequence(int model, int sequence)
{
    if (model < 0 || model >= modelCount())
        return;
    m_models[model].sequence = sequence;
    m_time = 0.0f;
    redraw();
}

int StudioModelView::sequence(int model) const
{
    return model >= 0 && model < modelCount() ? m_models[model].sequence : -1;
}

void StudioModelView::setPlaying(bool playing)
{
    m_playing = playing;
    redraw();
}

void StudioModelView::setTime(float seconds)
{
    m_time = std::max(0.0f, seconds);
    redraw();
}

void StudioModelView::setHoldUnlooped(bool hold)
{
    m_holdUnlooped = hold;
    redraw();
}

void StudioModelView::setLooping(int looping)
{
    m_looping = looping;
    redraw();
}

void StudioModelView::setSpeed(float speed)
{
    m_speed = speed;
}

void StudioModelView::setRestPose(int model, bool rest)
{
    if (model < 0 || model >= modelCount())
        return;
    m_models[model].rest = rest;
    redraw();
}

void StudioModelView::setBody(int model, int part, int subModel)
{
    if (model < 0 || model >= modelCount() || part < 0)
        return;
    Model& entry = m_models[model];
    if (entry.body.size() <= static_cast<size_t>(part))
        entry.body.resize(part + 1, 0);
    entry.body[part] = subModel;
    entry.arranged = false;
    redraw();
}

void StudioModelView::setSkin(int model, int skin)
{
    if (model < 0 || model >= modelCount())
        return;
    m_models[model].skin = skin;
    redraw();
}

void StudioModelView::setControllers(int model, const std::vector<float>& values)
{
    if (model < 0 || model >= modelCount())
        return;
    m_models[model].controllers = values;
    redraw();
}

void StudioModelView::setColormap(int model, int top, int bottom)
{
    if (model < 0 || model >= modelCount())
        return;
    m_models[model].top = top;
    m_models[model].bottom = bottom;
    m_models[model].textured = false;
    redraw();
}

void StudioModelView::setDisplay(const Display& display)
{
    m_display = display;
    redraw();
}

void StudioModelView::setHighlight(int bone, int attachment, int hitbox)
{
    m_highlightBone = bone;
    m_highlightAttachment = attachment;
    m_highlightHitbox = hitbox;
    redraw();
}

float StudioModelView::frameOf(const StudioSequence& sequence) const
{
    // nudge so an exact frame time is not read as the previous frame
    const float at = m_time * std::max(1.0f, sequence.fps) + 1e-3f;
    const int last = std::max(0, sequence.frames - 1);
    const bool hold = m_looping == NeverLoop || (m_looping == SequenceLooping && m_holdUnlooped && !(sequence.flags & 1));
    if (hold || last == 0)
        return std::min(at, static_cast<float>(last));
    // looping: the last frame equals the first, wrap there
    return std::fmod(at, static_cast<float>(last));
}

float StudioModelView::framePosition() const
{
    if (m_models.empty())
        return 0.0f;
    const Model& model = m_models.front();
    if (model.sequence < 0 || model.sequence >= static_cast<int>(model.data->sequences.size()))
        return 0.0f;
    return frameOf(model.data->sequences[model.sequence]);
}

int StudioModelView::frame() const
{
    return static_cast<int>(framePosition());
}

void StudioModelView::setFrame(int frame)
{
    setFramePosition(static_cast<float>(frame));
}

void StudioModelView::setFramePosition(float frame)
{
    if (m_models.empty())
        return;
    const Model& model = m_models.front();
    if (model.sequence < 0 || model.sequence >= static_cast<int>(model.data->sequences.size()))
        return;
    setTime(std::max(0.0f, frame) / std::max(1.0f, model.data->sequences[model.sequence].fps));
}

void StudioModelView::setPlacement(int model, const aiMatrix4x4& placement)
{
    if (model < 0 || model >= modelCount())
        return;
    m_models[model].placement = placement;
    redraw();
}

void StudioModelView::setBoneOffset(int model, int bone, const aiMatrix4x4& offset)
{
    if (model < 0 || model >= modelCount())
        return;
    m_models[model].offsetBone = bone;
    m_models[model].offset = offset;
    redraw();
}

aiMatrix4x4 StudioModelView::boneTransform(int model, int bone) const
{
    if (model < 0 || model >= modelCount() || bone < 0 || bone >= static_cast<int>(m_models[model].bones.size()))
        return aiMatrix4x4();
    return m_models[model].bones[bone];
}

void StudioModelView::setGaitSequence(int model, int sequence)
{
    if (model < 0 || model >= modelCount())
        return;
    m_models[model].gaitSequence = sequence;
    redraw();
}

void StudioModelView::setAimPitch(int model, float degrees)
{
    if (model < 0 || model >= modelCount())
        return;
    m_models[model].aimPitch = degrees;
    redraw();
}

void StudioModelView::updateBones()
{
    auto playing = [this](const StudioModel& data, int index, float pitch, int& frame, float& blend, float& between) {
        frame = 0;
        blend = 0.0f;
        between = 0.0f;
        if (index < 0 || index >= static_cast<int>(data.sequences.size()))
            return;
        const StudioSequence& sequence = data.sequences[index];
        const float position = frameOf(sequence);
        frame = static_cast<int>(position);
        between = position - static_cast<float>(frame);
        if (sequence.blends > 1 && sequence.blendEnd != sequence.blendStart)
            blend = std::clamp((pitch - sequence.blendStart) / (sequence.blendEnd - sequence.blendStart), 0.0f, 1.0f);
    };

    for (Model& model : m_models)
    {
        const StudioModel& data = *model.data;
        int frame = 0;
        float blend = 0.0f, between = 0.0f;
        playing(data, model.sequence, model.aimPitch, frame, blend, between);
        // apply bone controllers
        std::vector<std::array<float, 6>> adjust;
        for (size_t i = 0; i < model.controllers.size() && i < data.controllers.size(); ++i)
        {
            const StudioBoneController& controller = data.controllers[i];
            if (controller.bone < 0 || controller.bone >= static_cast<int>(data.bones.size()))
                continue;
            adjust.resize(data.bones.size());
            const int motion = controller.type & 0x3f;
            for (int axis = 0; axis < 6; ++axis)
                if (motion & (1 << axis))
                    adjust[controller.bone][axis] += axis < 3 ? model.controllers[i] : qDegreesToRadians(model.controllers[i]);
        }
        std::vector<aiMatrix4x4> local = data.LocalBoneTransforms(model.rest ? -1 : model.sequence, frame, blend, adjust.empty() ? nullptr : &adjust, between);

        if (model.mergeInto < 0 && model.gaitSequence >= 0 && !model.rest)
        {
            int gaitFrame = 0;
            float gaitBlend = 0.0f;
            float gaitBetween = 0.0f;
            playing(data, model.gaitSequence, model.aimPitch, gaitFrame, gaitBlend, gaitBetween);
            const std::vector<aiMatrix4x4> gait = data.LocalBoneTransforms(model.gaitSequence, gaitFrame, gaitBlend, nullptr, gaitBetween);
            for (size_t i = 0; i < data.bones.size() && !String_EqualsNoCase(data.bones[i].name, "Bip01 Spine"); ++i)
                local[i] = gait[i];
        }

        const Model* parent = model.mergeInto >= 0 ? &m_models[model.mergeInto] : nullptr;
        model.bones.assign(data.bones.size(), aiMatrix4x4());
        std::vector<bool> done(data.bones.size(), false);
        std::vector<int> chain;
        for (size_t i = 0; i < data.bones.size(); ++i)
        {
            chain.clear();
            for (int bone = static_cast<int>(i); bone >= 0 && !done[bone]; bone = data.bones[bone].parent)
                chain.push_back(bone);
            for (size_t k = chain.size(); k-- > 0;)
            {
                const int bone = chain[k];
                const int match = parent ? model.mergeBones[bone] : -1;
                const int up = data.bones[bone].parent;
                if (match >= 0 && match < static_cast<int>(parent->bones.size()))
                    model.bones[bone] = parent->bones[match];
                else
                    model.bones[bone] = up >= 0 ? model.bones[up] * local[bone] : model.placement * local[bone];
                done[bone] = true;
            }
        }
        if (model.offsetBone >= 0 && model.offsetBone < static_cast<int>(model.bones.size()))
            model.bones[model.offsetBone] = model.bones[model.offsetBone] * model.offset;
    }
}

const StudioSubModel* StudioModelView::shownSubModel(const Model& model, size_t part) const
{
    const StudioBodyPart& body = model.data->bodyParts[part];
    if (body.models.empty())
        return nullptr;
    const int chosen = part < model.body.size() ? model.body[part] : 0;
    return &body.models[std::clamp(chosen, 0, static_cast<int>(body.models.size()) - 1)];
}

void StudioModelView::buildVertices(const Model& model, std::vector<float>& out) const
{
    size_t corners = 0;
    for (size_t part = 0; part < model.data->bodyParts.size(); ++part)
        if (const StudioSubModel* shown = shownSubModel(model, part))
            for (const StudioMesh& mesh : shown->meshes) corners += mesh.triangles.size();
    out.resize(corners * kFloatsPerVertex);
    float* write = out.data();
    // each vertex and normal moved once, then copied to the corners that share it
    static thread_local std::vector<aiVector3D> placed, turned;
    const aiMatrix4x4* bones = model.bones.data();
    for (size_t part = 0; part < model.data->bodyParts.size(); ++part)
    {
        const StudioSubModel* shown = shownSubModel(model, part);
        if (!shown)
            continue;
        const StudioSubModel& sub = *shown;
        placed.resize(sub.vertices.size());
        turned.resize(sub.normals.size());
        const aiVector3D* vertices = sub.vertices.data();
        const int* vertexBones = sub.vertexBones.data();
        for (size_t v = 0; v < placed.size(); ++v)
            placed[v] = bones[vertexBones[v]] * vertices[v];
        const aiVector3D* normals = sub.normals.data();
        const int* normalBones = sub.normalBones.data();
        for (size_t n = 0; n < turned.size(); ++n)
            turned[n] = (aiMatrix3x3(bones[normalBones[n]]) * normals[n]).NormalizeSafe();
        const aiVector3D* positions = placed.data();
        const aiVector3D* directions = turned.data();
        for (const StudioMesh& mesh : sub.meshes)
        {
            const StudioTriVertex* corner = mesh.triangles.data();
            for (const StudioTriVertex* end = corner + mesh.triangles.size(); corner != end; ++corner, write += kFloatsPerVertex)
            {
                const aiVector3D& position = positions[corner->vertex];
                const aiVector3D& normal = directions[corner->normal];
                write[0] = position.x;
                write[1] = position.y;
                write[2] = position.z;
                write[3] = normal.x;
                write[4] = normal.y;
                write[5] = normal.z;
                write[6] = corner->u;
                write[7] = corner->v;
            }
        }
    }
}

void StudioModelView::arrange(Model& model)
{
    model.meshes.clear();
    int first = 0;
    for (size_t part = 0; part < model.data->bodyParts.size(); ++part)
    {
        const StudioSubModel* sub = shownSubModel(model, part);
        if (!sub)
            continue;
        for (const StudioMesh& mesh : sub->meshes)
        {
            const int count = static_cast<int>(mesh.triangles.size());
            model.meshes.push_back({ mesh.texture, first, count, mesh.skinRef });
            first += count;
        }
    }
    model.arranged = true;
}

QImage StudioModelView::textureImage(const StudioTexture& texture, int top, int bottom)
{
    if (texture.pixels.empty() || texture.palette.size() < 768)
        return {};
    uint8_t palette[768];
    std::copy_n(texture.palette.begin(), 768, palette);
    int low = 0, mid = 0, high = 0;
    if ((top >= 0 || bottom >= 0) && remapSpans(texture.name, low, mid, high))
    {
        // recolour the span, keep brightness
        auto paint = [&](int from, int to, int hue) {
            for (int i = std::max(0, from); hue >= 0 && i <= to && i < 256; ++i)
            {
                const QColor seen(palette[i * 3], palette[i * 3 + 1], palette[i * 3 + 2]);
                const QColor made = QColor::fromHsv(hue * 359 / 255, std::max(seen.saturation(), 160), seen.value());
                palette[i * 3] = static_cast<uint8_t>(made.red());
                palette[i * 3 + 1] = static_cast<uint8_t>(made.green());
                palette[i * 3 + 2] = static_cast<uint8_t>(made.blue());
            }
        };
        paint(low, mid, top);
        if (high > 0)
            paint(mid + 1, high, bottom);
    }
    const bool masked = (texture.flags & STUDIO_NF_MASKED) != 0;
    QImage image(texture.width, texture.height, QImage::Format_RGBA8888);
    for (int y = 0; y < texture.height; ++y)
    {
        uchar* line = image.scanLine(y);
        for (int x = 0; x < texture.width; ++x)
        {
            const uint8_t index = texture.pixels[static_cast<size_t>(y) * texture.width + x];
            line[x * 4 + 0] = palette[index * 3 + 0];
            line[x * 4 + 1] = palette[index * 3 + 1];
            line[x * 4 + 2] = palette[index * 3 + 2];
            line[x * 4 + 3] = masked && index == 255 ? 0 : 255;
        }
    }
    return image;
}

void StudioModelView::uploadTextures(Model& model)
{
    const StudioModel& data = *model.data;
    releaseTextures(model);
    model.textures.assign(data.textures.size(), 0);
    for (size_t i = 0; i < data.textures.size(); ++i)
    {
        const StudioTexture& texture = data.textures[i];
        const QImage image = textureImage(texture, model.top, model.bottom);
        if (image.isNull())
            continue;
        const bool masked = (texture.flags & STUDIO_NF_MASKED) != 0;
        glGenTextures(1, &model.textures[i]);
        glBindTexture(GL_TEXTURE_2D, model.textures[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texture.width, texture.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image.constBits());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, masked ? GL_NEAREST : GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, masked ? GL_NEAREST : GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }
    model.textured = true;
}

void StudioModelView::releaseTextures(Model& model)
{
    for (unsigned int texture : model.textures)
    {
        if (texture)
            glDeleteTextures(1, &texture);
    }
    model.textures.clear();
}

void StudioModelView::upload(Model& model)
{
    glGenVertexArrays(1, &model.vao);
    glGenBuffers(1, &model.vbo);
    glBindVertexArray(model.vao);
    glBindBuffer(GL_ARRAY_BUFFER, model.vbo);
    const GLsizei stride = kFloatsPerVertex * sizeof(float);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(6 * sizeof(float)));
    glBindVertexArray(0);
    model.uploaded = true;
}

void StudioModelView::release(Model& model)
{
    if (!model.uploaded)
        return;
    releaseTextures(model);
    model.textured = false;
    glDeleteBuffers(1, &model.vbo);
    glDeleteVertexArrays(1, &model.vao);
    model.vbo = model.vao = 0;
    model.uploaded = false;
}

void StudioModelView::setScene(const Scene& scene)
{
    m_groundDirty = m_groundDirty || scene.groundTexture.cacheKey() != m_scene.groundTexture.cacheKey();
    m_backgroundDirty = m_backgroundDirty || scene.background.cacheKey() != m_scene.background.cacheKey();
    m_scene = scene;
    redraw();
}

unsigned int StudioModelView::sceneTexture(unsigned int& texture, const QImage& image, bool& dirty)
{
    if (dirty)
    {
        dirty = false;
        if (texture)
            glDeleteTextures(1, &texture);
        texture = 0;
        if (!image.isNull())
        {
            const QImage pixels = image.convertToFormat(QImage::Format_RGBA8888);
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, pixels.width(), pixels.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.constBits());
            glGenerateMipmap(GL_TEXTURE_2D);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        }
    }
    return texture;
}

void StudioModelView::drawBackground()
{
    if (!m_scene.showBackground || !sceneTexture(m_backgroundTexture, m_scene.background, m_backgroundDirty))
        return;
    // fullscreen quad in clip space
    const float quad[] = { -1, -1, 0.999f, 0, 0, 1, 0, 1, 1, -1, 0.999f, 0, 0, 1, 1, 1, 1, 1, 0.999f, 0, 0, 1, 1, 0,
                           -1, -1, 0.999f, 0, 0, 1, 0, 1, 1, 1, 0.999f, 0, 0, 1, 1, 0, -1, 1, 0.999f, 0, 0, 1, 0, 0 };
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    bindGuideBuffer();
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STREAM_DRAW);
    glBindTexture(GL_TEXTURE_2D, m_backgroundTexture);
    m_program->setUniformValue("viewProjection", QMatrix4x4());
    m_program->setUniformValue("mode", 2);
    m_program->setUniformValue("opacity", 1.0f);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    m_program->setUniformValue("viewProjection", m_viewProjection);
    m_program->setUniformValue("opacity", m_display.opacity);
    glEnable(GL_DEPTH_TEST);
}

void StudioModelView::drawScenery(const std::function<void()>& restoreMode)
{
    if (!m_display.ground && !m_display.shadows)
        return;
    const float height = m_scene.groundOrigin.z();
    // reflection: models mirrored below the ground plane
    if (m_display.ground && m_scene.mirror)
    {
        QMatrix4x4 reflect;
        reflect.translate(0.0f, 0.0f, height);
        reflect.scale(1.0f, 1.0f, -1.0f);
        reflect.translate(0.0f, 0.0f, -height);
        GLint front = GL_CW;
        glGetIntegerv(GL_FRONT_FACE, &front);
        glFrontFace(front == GL_CW ? GL_CCW : GL_CW);
        m_program->setUniformValue("viewProjection", m_viewProjection * reflect);
        for (Model& model : m_models)
            drawModel(model, false);
        glFrontFace(front);
        m_program->setUniformValue("viewProjection", m_viewProjection);
    }
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDisable(GL_CULL_FACE);
    // textured ground, translucent when mirroring
    if (m_display.ground && m_scene.groundTextured && sceneTexture(m_groundTexture, m_scene.groundTexture, m_groundDirty))
    {
        const float reach = m_scene.groundLength;
        const QVector3D middle = m_scene.groundOrigin;
        std::vector<float> quad;
        const float corners[6][2] = { { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, -1 }, { 1, 1 }, { -1, 1 } };
        for (const auto& corner : corners)
        {
            const float x = middle.x() + corner[0] * reach, y = middle.y() + corner[1] * reach;
            // tiled, or one image across the ground
            const float u = m_scene.tiling ? x / std::max(1, m_scene.tileSize) : (corner[0] + 1.0f) * 0.5f;
            const float v = m_scene.tiling ? y / std::max(1, m_scene.tileSize) : (corner[1] + 1.0f) * 0.5f;
            quad.insert(quad.end(), { x, y, height, 0.0f, 0.0f, 1.0f, u, v });
        }
        bindGuideBuffer();
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(quad.size() * sizeof(float)), quad.data(), GL_STREAM_DRAW);
        glBindTexture(GL_TEXTURE_2D, m_groundTexture);
        m_program->setUniformValue("mode", 0);
        if (m_scene.mirror)
        {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            m_program->setUniformValue("opacity", 0.6f);
        }
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
        glDisable(GL_BLEND);
        m_program->setUniformValue("opacity", m_display.opacity);
    }
    // planar shadows; the stencil darkens each pixel once
    if (m_display.shadows)
    {
        const QVector3D light = QVector3D(-0.4f, 0.3f, -1.0f).normalized();
        const float lift = m_display.fixShadowZFighting ? 0.05f : 0.0f;
        const float h = height + lift, dx = light.x() / light.z(), dy = light.y() / light.z();
        const QMatrix4x4 flatten(1, 0, -dx, dx * h, 0, 1, -dy, dy * h, 0, 0, 0, h, 0, 0, 0, 1);
        glEnable(GL_STENCIL_TEST);
        glClear(GL_STENCIL_BUFFER_BIT);
        glStencilFunc(GL_EQUAL, 0, 0xff);
        glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
        if (m_display.fixShadowZFighting)
        {
            glEnable(GL_POLYGON_OFFSET_FILL);
            glPolygonOffset(-1.0f, -1.0f);
        }
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        m_program->setUniformValue("viewProjection", m_viewProjection * flatten);
        m_program->setUniformValue("mode", 5);
        m_program->setUniformValue("tint", QVector4D(0.0f, 0.0f, 0.0f, 0.45f));
        const int kept = m_display.mode;
        m_display.mode = Display::Wireframe;  // flat colour, no texture
        for (Model& model : m_models)
            drawModel(model, false);
        m_display.mode = kept;
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glDisable(GL_POLYGON_OFFSET_FILL);
        glDisable(GL_STENCIL_TEST);
        m_program->setUniformValue("viewProjection", m_viewProjection);
    }
    if (m_display.backfaceCulling)
        glEnable(GL_CULL_FACE);
    restoreMode();
}

void StudioModelView::drawGuidelines()
{
    if (!m_display.guidelines)
        return;
    // largest frame of the aspect that fits, centred
    const double shape = std::max(1, m_display.aspectWidth) / static_cast<double>(std::max(1, m_display.aspectHeight));
    const QRectF whole = rect();
    QSizeF size(whole.height() * shape, whole.height());
    if (size.width() > whole.width())
        size = QSizeF(whole.width(), whole.width() / shape);
    const QRectF frame(whole.center() - QPointF(size.width() / 2, size.height() / 2), size);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    QPainter painter(m_paintDevice.get());
    if (m_display.offscreenAreas)
    {
        const QColor shade(0, 0, 0, 140);
        painter.fillRect(QRectF(whole.left(), whole.top(), frame.left() - whole.left(), whole.height()), shade);
        painter.fillRect(QRectF(frame.right(), whole.top(), whole.right() - frame.right(), whole.height()), shade);
        painter.fillRect(QRectF(frame.left(), whole.top(), frame.width(), frame.top() - whole.top()), shade);
        painter.fillRect(QRectF(frame.left(), frame.bottom(), frame.width(), whole.bottom() - frame.bottom()), shade);
    }
    painter.setPen(QPen(QColor(255, 255, 255, 160), 1, Qt::DashLine));
    painter.drawRect(frame);
    painter.drawLine(QPointF(frame.center().x(), frame.top()), QPointF(frame.center().x(), frame.bottom()));
    painter.drawLine(QPointF(frame.left(), frame.center().y()), QPointF(frame.right(), frame.center().y()));
}

void StudioModelView::releaseAll()
{
    if (!m_glReady)
        return;
    for (Model& model : m_models)
        release(model);
    if (m_guideVbo)
        glDeleteBuffers(1, &m_guideVbo);
    if (m_guideVao)
        glDeleteVertexArrays(1, &m_guideVao);
    m_guideVbo = m_guideVao = 0;
    for (unsigned int* texture : { &m_groundTexture, &m_backgroundTexture })
    {
        if (*texture)
            glDeleteTextures(1, texture);
        *texture = 0;
    }
    m_groundDirty = m_backgroundDirty = true;
}

void StudioModelView::setGuides(const std::vector<Guide>& guides)
{
    m_guides = guides;
    redraw();
}

void StudioModelView::drawGuides()
{
    std::vector<float> vertices;
    const int corners[6][4] = { { 0, 2, 3, 1 }, { 4, 5, 7, 6 }, { 0, 1, 5, 4 }, { 2, 6, 7, 3 }, { 0, 4, 6, 2 }, { 1, 3, 7, 5 } };
    const QVector3D normals[6] = { { 0, 0, -1 }, { 0, 0, 1 }, { 0, -1, 0 }, { 0, 1, 0 }, { -1, 0, 0 }, { 1, 0, 0 } };
    for (const Guide& guide : m_guides)
    {
        const float c = std::cos(guide.yaw), s = std::sin(guide.yaw);
        auto turned = [&](const QVector3D& v) { return QVector3D(v.x() * c - v.y() * s, v.x() * s + v.y() * c, v.z()); };
        QVector3D at[8];
        for (int i = 0; i < 8; ++i)
            at[i] = guide.middle + turned(QVector3D((i & 4) ? guide.half.x() : -guide.half.x(), (i & 2) ? guide.half.y() : -guide.half.y(),
                                                    (i & 1) ? guide.half.z() : -guide.half.z()));
        for (int face = 0; face < 6; ++face)
        {
            const QVector3D normal = turned(normals[face]);
            for (int k : { 0, 1, 2, 0, 2, 3 })
            {
                const QVector3D& p = at[corners[face][k]];
                vertices.insert(vertices.end(), { p.x(), p.y(), p.z(), normal.x(), normal.y(), normal.z(), 0.0f, 0.0f });
            }
        }
    }
    bindGuideBuffer();
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)), vertices.data(), GL_STREAM_DRAW);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    m_program->setUniformValue("mode", 3);
    m_program->setUniformValue("tint", QVector4D(0.45f, 0.65f, 1.0f, 0.28f));
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size() / kFloatsPerVertex));
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glBindVertexArray(0);
}

void StudioModelView::bindGuideBuffer()
{
    if (!m_guideVao)
    {
        glGenVertexArrays(1, &m_guideVao);
        glGenBuffers(1, &m_guideVbo);
        glBindVertexArray(m_guideVao);
        glBindBuffer(GL_ARRAY_BUFFER, m_guideVbo);
        const GLsizei stride = kFloatsPerVertex * sizeof(float);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(0));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(6 * sizeof(float)));
    }
    glBindVertexArray(m_guideVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_guideVbo);
}

void StudioModelView::drawLines(const std::vector<float>& points, const QColor& colour, unsigned int primitive)
{
    if (points.empty())
        return;
    std::vector<float> vertices;
    vertices.reserve(points.size() / 3 * kFloatsPerVertex);
    for (size_t i = 0; i + 2 < points.size(); i += 3)
        vertices.insert(vertices.end(), { points[i], points[i + 1], points[i + 2], 0.0f, 0.0f, 1.0f, 0.0f, 0.0f });
    bindGuideBuffer();
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)), vertices.data(), GL_STREAM_DRAW);
    m_program->setUniformValue("mode", 5);
    m_program->setUniformValue("tint", QVector4D(colour.redF(), colour.greenF(), colour.blueF(), colour.alphaF()));
    glDrawArrays(primitive, 0, static_cast<GLsizei>(vertices.size() / kFloatsPerVertex));
    glBindVertexArray(0);
}

void StudioModelView::drawOverlays()
{
    const Display& d = m_display;
    glDisable(GL_CULL_FACE);
    if (d.ground && !(m_scene.groundTextured && m_groundTexture))
    {
        std::vector<float> grid;
        const float reach = m_scene.groundLength;
        const int cells = std::clamp(static_cast<int>(reach / kGroundCell), 1, 256);
        const float step = reach / cells;
        const QVector3D at0 = m_scene.groundOrigin;
        for (int i = -cells; i <= cells; ++i)
        {
            const float at = i * step;
            grid.insert(grid.end(), { at0.x() + at, at0.y() - reach, at0.z(), at0.x() + at, at0.y() + reach, at0.z(),
                                      at0.x() - reach, at0.y() + at, at0.z(), at0.x() + reach, at0.y() + at, at0.z() });
        }
        drawLines(grid, QColor(128, 128, 128), GL_LINES);
    }
    if (d.playerBox)
    {
        std::vector<float> box;
        addBox(box, aiVector3D(-16, -16, 0), aiVector3D(16, 16, 72), aiMatrix4x4());
        drawLines(box, QColor(255, 200, 60), GL_LINES);
    }
    if (d.axes)
        for (int axis = 0; axis < 3; ++axis)
        {
            std::vector<float> line(6, 0.0f);
            line[3 + axis] = m_distance * 0.25f;
            drawLines(line, kAxisColours[axis], GL_LINES);
        }

    for (size_t index = 0; index < m_models.size(); ++index)
    {
        const Model& model = m_models[index];
        const StudioModel& data = *model.data;
        const bool first = index == 0;
        if (d.normals)
        {
            std::vector<float> built, lines;
            buildVertices(model, built);
            lines.reserve(built.size() / kFloatsPerVertex * 6);
            for (size_t i = 0; i + 5 < built.size(); i += kFloatsPerVertex)
                lines.insert(lines.end(), { built[i], built[i + 1], built[i + 2], built[i] + built[i + 3] * kNormalLength,
                                            built[i + 1] + built[i + 4] * kNormalLength, built[i + 2] + built[i + 5] * kNormalLength });
            drawLines(lines, QColor(90, 220, 255), GL_LINES);
        }
        if (d.boundingBox)
        {
            std::vector<float> box;
            addBox(box, data.boundsMin, data.boundsMax, model.placement);
            drawLines(box, QColor(255, 120, 60), GL_LINES);
        }
        if (d.clippingBox)
        {
            std::vector<float> box;
            addBox(box, data.clipMin, data.clipMax, model.placement);
            drawLines(box, QColor(255, 60, 200), GL_LINES);
        }
        for (size_t i = 0; i < data.hitboxes.size(); ++i)
        {
            const StudioHitbox& hitbox = data.hitboxes[i];
            const bool picked = first && static_cast<int>(i) == m_highlightHitbox;
            if ((!d.hitboxes && !picked) || hitbox.bone < 0 || hitbox.bone >= static_cast<int>(model.bones.size()))
                continue;
            std::vector<float> box;
            addBox(box, hitbox.min, hitbox.max, model.bones[hitbox.bone]);
            drawLines(box, picked ? QColor(255, 255, 0) : kGroupColours[std::abs(hitbox.group) % 8], GL_LINES);
        }

        // overlays: drawn over the model
        glDisable(GL_DEPTH_TEST);
        std::vector<float> points;
        if (d.bones)
        {
            std::vector<float> lines;
            for (size_t i = 0; i < data.bones.size() && i < model.bones.size(); ++i)
            {
                addPoint(points, boneOrigin(model.bones[i]));
                const int parent = data.bones[i].parent;
                if (parent < 0 || parent >= static_cast<int>(model.bones.size()))
                    continue;
                addPoint(lines, boneOrigin(model.bones[parent]));
                addPoint(lines, boneOrigin(model.bones[i]));
            }
            drawLines(lines, QColor(255, 230, 90), GL_LINES);
            glPointSize(4.0f);
            drawLines(points, QColor(255, 120, 40), GL_POINTS);
        }
        points.clear();
        std::vector<float> picks;
        for (size_t i = 0; i < data.attachments.size(); ++i)
        {
            const StudioAttachment& attachment = data.attachments[i];
            if (attachment.bone < 0 || attachment.bone >= static_cast<int>(model.bones.size()))
                continue;
            const bool picked = first && static_cast<int>(i) == m_highlightAttachment;
            if (!d.attachments && !picked)
                continue;
            addPoint(picked ? picks : points, model.bones[attachment.bone] * attachment.origin);
        }
        if (first && m_highlightBone >= 0 && m_highlightBone < static_cast<int>(model.bones.size()))
            addPoint(picks, boneOrigin(model.bones[m_highlightBone]));
        glPointSize(6.0f);
        drawLines(points, QColor(80, 255, 120), GL_POINTS);
        if (d.eyePosition)
        {
            std::vector<float> eye;
            addPoint(eye, model.placement * data.eyePosition);
            drawLines(eye, QColor(120, 160, 255), GL_POINTS);
        }
        glPointSize(9.0f);
        drawLines(picks, QColor(255, 255, 0), GL_POINTS);
        glPointSize(1.0f);
        glEnable(GL_DEPTH_TEST);
    }
    if (d.backfaceCulling)
        glEnable(GL_CULL_FACE);
}

void StudioModelView::initializeGL()
{
    if (!initializeOpenGLFunctions())
        return;
    m_program = new QOpenGLShaderProgram();
    m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, kVertexShader);
    m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, kFragmentShader);
    m_glReady = m_program->link();
    m_frameClock.start();
    // log the renderer once
    static bool told = false;
    if (!told)
    {
        told = true;
        VortigauntLog::Vortigaunt_Printf(QStringLiteral("OpenGL: %1, %2")
                                             .arg(QString::fromLatin1(reinterpret_cast<const char*>(glGetString(GL_RENDERER))),
                                                  QString::fromLatin1(reinterpret_cast<const char*>(glGetString(GL_VERSION)))));
    }
}

bool StudioModelView::bounds(QVector3D& low, QVector3D& high, int onlyModel) const
{
    bool any = false;
    for (int index = 0; index < modelCount(); ++index)
    {
        if (onlyModel >= 0 && index != onlyModel)
            continue;
        const Model& model = m_models[index];
        for (size_t part = 0; part < model.data->bodyParts.size(); ++part)
        {
            const StudioSubModel* shown = shownSubModel(model, part);
            if (!shown)
                continue;
            const StudioSubModel& sub = *shown;
            // only vertices that are drawn
            for (const StudioMesh& mesh : sub.meshes)
                for (const StudioTriVertex& corner : mesh.triangles)
                {
                    const size_t v = static_cast<size_t>(corner.vertex);
                    if (v >= sub.vertices.size())
                        continue;
                    const QVector3D p = toQt(model.bones[sub.vertexBones[v]] * sub.vertices[v]);
                    if (!any)
                    {
                        low = high = p;
                        any = true;
                    }
                    low = QVector3D(std::min(low.x(), p.x()), std::min(low.y(), p.y()), std::min(low.z(), p.z()));
                    high = QVector3D(std::max(high.x(), p.x()), std::max(high.y(), p.y()), std::max(high.z(), p.z()));
                }
        }
    }
    return any;
}

void StudioModelView::fitView(int onlyModel)
{
    updateBones();
    QVector3D low, high;
    if (!bounds(low, high, onlyModel))
        return;
    m_target = (low + high) * 0.5f;
    m_distance = std::max(4.0f, (high - low).length() * 1.1f);
    const float yaw = qDegreesToRadians(m_yaw), pitch = qDegreesToRadians(m_pitch);
    m_freeEye = m_target + m_distance * QVector3D(std::cos(pitch) * std::cos(yaw), std::cos(pitch) * std::sin(yaw), std::sin(pitch));
    redraw();
}

void StudioModelView::setCamera(int camera)
{
    // free camera starts at the orbit eye
    if (camera == FreeLook && m_camera != FreeLook)
        m_freeEye = m_eye;
    m_camera = camera;
    redraw();
}

void StudioModelView::setFieldOfView(float degrees)
{
    m_fieldOfView = degrees;
    redraw();
}

StudioModelView::ViewPlace StudioModelView::viewPlace() const
{
    return { m_target, m_freeEye, m_distance, m_yaw, m_pitch };
}

void StudioModelView::setViewPlace(const ViewPlace& place)
{
    m_target = place.target;
    m_freeEye = place.freeEye;
    m_distance = place.distance;
    m_yaw = place.yaw;
    m_pitch = place.pitch;
    redraw();
}

void StudioModelView::setCameraAngles(float yaw, float pitch)
{
    m_yaw = yaw;
    m_pitch = std::clamp(pitch, -89.0f, 89.0f);
    redraw();
}

void StudioModelView::setTurnHandles(bool shown)
{
    m_turnHandles = shown;
    setMouseTracking(shown);
    redraw();
}

void StudioModelView::recenter()
{
    updateBones();
    QVector3D low, high;
    if (!bounds(low, high))
        return;
    m_target = (low + high) * 0.5f;
    redraw();
}

QPointF StudioModelView::onScreen(const QVector3D& p) const
{
    const QVector4D clip = m_viewProjection * QVector4D(p, 1.0f);
    const float w = std::abs(clip.w()) < 1e-6f ? 1e-6f : clip.w();
    return QPointF((clip.x() / w + 1.0f) * 0.5f * width(), (1.0f - clip.y() / w) * 0.5f * height());
}

QVector3D StudioModelView::ringPoint(int axis, float turn) const
{
    // right-handed axes in the ring's plane
    static const QVector3D across[3][2] = { { { 0, 1, 0 }, { 0, 0, 1 } }, { { 0, 0, 1 }, { 1, 0, 0 } }, { { 1, 0, 0 }, { 0, 1, 0 } } };
    return m_handleMiddle + m_handleRadius * (across[axis][0] * std::cos(turn) + across[axis][1] * std::sin(turn));
}

int StudioModelView::handleAt(const QPointF& at, float* turn) const
{
    if (!m_turnHandles || m_handleRadius <= 0.0f)
        return -1;
    const QVector3D toEye = (m_eye - m_handleMiddle).normalized();
    int best = -1;
    float nearest = kHandleReach;
    for (int axis = 0; axis < 3; ++axis)
        for (int i = 0; i < kRingPoints; ++i)
        {
            const float t = 2.0f * kPi * i / kRingPoints;
            const QVector3D p = ringPoint(axis, t);
            const QPointF d = onScreen(p) - at;
            // prefer the front half
            const float behind = QVector3D::dotProduct(p - m_handleMiddle, toEye) < -m_handleRadius * 0.1f ? 3.0f : 0.0f;
            const float distance = static_cast<float>(std::sqrt(d.x() * d.x() + d.y() * d.y())) + behind;
            if (distance < nearest)
            {
                nearest = distance;
                best = axis;
                if (turn)
                    *turn = t;
            }
        }
    return best;
}

float StudioModelView::handleTurn(const QPointF& from, const QPointF& to) const
{
    static const QVector3D axes[3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
    const QVector3D toEye = (m_eye - m_handleMiddle).normalized();
    const float facing = QVector3D::dotProduct(axes[m_handleAxis], toEye);
    const QPointF middle = onScreen(m_handleMiddle);
    if (std::abs(facing) > 0.3f)
    {
        // ring facing the eye: mouse angle about its centre
        const float a = static_cast<float>(std::atan2(-(from.y() - middle.y()), from.x() - middle.x()));
        const float b = static_cast<float>(std::atan2(-(to.y() - middle.y()), to.x() - middle.x()));
        float turn = b - a;
        while (turn > kPi)
            turn -= 2.0f * kPi;
        while (turn < -kPi)
            turn += 2.0f * kPi;
        return qRadiansToDegrees(turn) * (facing > 0.0f ? 1.0f : -1.0f);
    }
    // ring edge on: mouse movement along it
    const QPointF ahead = onScreen(ringPoint(m_handleAxis, m_handleAt + 0.05f)) - onScreen(ringPoint(m_handleAxis, m_handleAt - 0.05f));
    const float length = static_cast<float>(std::sqrt(ahead.x() * ahead.x() + ahead.y() * ahead.y()));
    const QPointF side = onScreen(m_handleMiddle + m_handleRadius * QVector3D::crossProduct(toEye, QVector3D(0, 0, 1)).normalized()) - middle;
    const float radius = std::max(10.0f, static_cast<float>(std::sqrt(side.x() * side.x() + side.y() * side.y())));
    if (length < 1e-4f)
        return 0.0f;
    const QPointF move = to - from;
    const float along = static_cast<float>((move.x() * ahead.x() + move.y() * ahead.y()) / length);
    return qRadiansToDegrees(along / radius);
}

void StudioModelView::drawHandles()
{
    const QVector3D toEye = (m_eye - m_handleMiddle).normalized();
    const QPointF middle = onScreen(m_handleMiddle);
    QPainter painter(m_paintDevice.get());
    painter.setRenderHint(QPainter::Antialiasing);
    for (int axis = 0; axis < 3; ++axis)
    {
        const bool lit = axis == m_handleAxis || (m_handleAxis < 0 && axis == m_hoverAxis);
        QColor colour = kAxisColours[axis];
        if (lit)
            colour = colour.lighter(135);
        // back half faint
        QPointF previous = onScreen(ringPoint(axis, 0.0f)), dot = previous;
        float farthest = -1.0f;
        for (int i = 1; i <= kRingPoints; ++i)
        {
            const float t = 2.0f * kPi * i / kRingPoints;
            const QVector3D p = ringPoint(axis, t - kPi / kRingPoints);
            const QPointF next = onScreen(ringPoint(axis, t));
            const bool front = QVector3D::dotProduct(p - m_handleMiddle, toEye) >= -m_handleRadius * 0.1f;
            QColor c = colour;
            c.setAlpha(front ? 255 : 70);
            painter.setPen(QPen(c, front ? (lit ? 4.0 : 2.5) : 1.5, Qt::SolidLine, Qt::RoundCap));
            painter.drawLine(previous, next);
            // handle dot
            const QPointF d = next - middle;
            const float reach = static_cast<float>(d.x() * d.x() + d.y() * d.y());
            if (front && reach > farthest + 0.5f)
            {
                farthest = reach;
                dot = next;
            }
            previous = next;
        }
        painter.setPen(QPen(QColor(0, 0, 0, 160), 1.0));
        painter.setBrush(colour);
        painter.drawEllipse(dot, lit ? 7.0 : 5.5, lit ? 7.0 : 5.5);
    }
}

void StudioModelView::setOrthographic(bool orthographic)
{
    m_orthographic = orthographic;
    redraw();
}

void StudioModelView::setBackground(const QColor& colour)
{
    m_background = colour;
    redraw();
}

void StudioModelView::drawModel(Model& model, bool additivePass)
{
    const StudioModel& data = *model.data;
    const bool textured = m_display.mode == Display::Textured;
    glBindVertexArray(model.vao);
    for (const Mesh& mesh : model.meshes)
    {
        const int flags = mesh.texture >= 0 ? data.textures[mesh.texture].flags : 0;
        const bool additive = textured && (flags & STUDIO_NF_ADDITIVE) != 0;
        if (additive != additivePass || mesh.count == 0)
            continue;
        if (textured)
        {
            int index = mesh.texture;
            if (model.skin > 0 && model.skin < static_cast<int>(data.skinFamilies.size()) && mesh.skinRef >= 0 &&
                mesh.skinRef < static_cast<int>(data.skinFamilies[model.skin].size()))
                index = data.skinFamilies[model.skin][mesh.skinRef];
            glBindTexture(GL_TEXTURE_2D, index >= 0 && index < static_cast<int>(model.textures.size()) ? model.textures[index] : 0);
            m_program->setUniformValue("mode", additive ? 2 : ((flags & STUDIO_NF_MASKED) ? 1 : 0));
        }
        glDrawArrays(GL_TRIANGLES, mesh.first, mesh.count);
    }
    glBindVertexArray(0);
}

void StudioModelView::paintGL()
{
    const QColor background = m_background.isValid() ? m_background : palette().color(QPalette::Base);
    glClearColor(background.redF(), background.greenF(), background.blueF(), 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    if (!m_glReady || m_models.empty())
    {
        QPainter painter(m_paintDevice.get());
        painter.setPen(palette().color(QPalette::PlaceholderText));
        painter.drawText(rect(), Qt::AlignCenter, placeholder);
        return;
    }

    const float elapsed = m_clock.restart() / 1000.0f;
    if (m_playing)
        m_time += elapsed * m_speed;
    updateBones();
    if (m_fitPending)
    {
        m_fitPending = false;
        QVector3D low, high;
        if (bounds(low, high))
        {
            m_target = (low + high) * 0.5f;
            m_distance = std::max(4.0f, (high - low).length() * 1.1f);
        }
    }

    const float yaw = qDegreesToRadians(m_yaw);
    const float pitch = qDegreesToRadians(m_pitch);
    const QVector3D away(std::cos(pitch) * std::cos(yaw), std::cos(pitch) * std::sin(yaw), std::sin(pitch));
    QVector3D eye = m_target + m_distance * away, look = m_target;
    if (m_camera == FreeLook)
    {
        eye = m_freeEye;
        look = eye - away;
    }
    else if (m_camera == FirstPerson)
    {
        eye = QVector3D();
        look = QVector3D(1, 0, 0);
    }
    QMatrix4x4 view;
    view.lookAt(eye, look, QVector3D(0, 0, 1));
    QMatrix4x4 projection;
    const float aspect = width() / static_cast<float>(std::max(1, height()));
    const float nearest = m_camera == ArcBall ? m_distance * 0.01f : 1.0f, farthest = m_camera == ArcBall ? m_distance * 50.0f : 20000.0f;
    if (m_orthographic)
    {
        const float half = m_distance * std::tan(qDegreesToRadians(m_fieldOfView * 0.5f));
        projection.ortho(-half * aspect, half * aspect, -half, half, nearest, farthest);
    }
    else
        projection.perspective(m_fieldOfView, aspect, nearest, farthest);

    const float mirrorX = m_display.mirrorX ? -1.0f : 1.0f, mirrorY = m_display.mirrorY ? -1.0f : 1.0f, mirrorZ = m_display.mirrorZ ? -1.0f : 1.0f;
    QMatrix4x4 mirror;
    mirror.scale(mirrorX, mirrorY, mirrorZ);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    if (m_display.backfaceCulling)
        glEnable(GL_CULL_FACE);
    else
        glDisable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(mirrorX * mirrorY * mirrorZ < 0.0f ? GL_CCW : GL_CW);
    m_program->bind();
    m_viewProjection = projection * view * mirror;
    m_eye = eye;
    m_program->setUniformValue("viewProjection", m_viewProjection);
    m_program->setUniformValue("tex", 0);
    m_program->setUniformValue("lightDirection", QVector3D(-0.4f, 0.3f, -1.0f).normalized());
    m_program->setUniformValue("opacity", m_display.opacity);
    glActiveTexture(GL_TEXTURE0);
    drawBackground();

    std::vector<float> vertices;
    m_triangles = 0;
    for (Model& model : m_models)
    {
        if (!model.uploaded)
            upload(model);
        if (!model.textured)
            uploadTextures(model);
        if (!model.arranged)
            arrange(model);
        buildVertices(model, vertices);
        m_triangles += static_cast<int>(vertices.size()) / kFloatsPerVertex / 3;
        glBindBuffer(GL_ARRAY_BUFFER, model.vbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)), vertices.data(), GL_STREAM_DRAW);
    }

    const QVector4D solid(0.78f, 0.78f, 0.78f, m_display.opacity);
    auto modelMode = [this, &solid]() {
        if (m_display.mode == Display::Smooth || m_display.mode == Display::Flat)
        {
            m_program->setUniformValue("mode", m_display.mode == Display::Smooth ? 3 : 4);
            m_program->setUniformValue("tint", solid);
        }
        else if (m_display.mode == Display::Wireframe)
        {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            m_program->setUniformValue("mode", 5);
            m_program->setUniformValue("tint", solid);
        }
    };
    modelMode();
    drawScenery(modelMode);
    const bool see = m_display.opacity < 1.0f;
    if (see)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    else
        glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    for (Model& model : m_models)
        drawModel(model, false);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glDepthMask(GL_FALSE);
    for (Model& model : m_models)
        drawModel(model, true);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    if (m_display.wireframeOverlay)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        const int kept = m_display.mode;
        m_display.mode = Display::Wireframe;
        m_program->setUniformValue("mode", 5);
        m_program->setUniformValue("tint", QVector4D(0.0f, 0.0f, 0.0f, 1.0f));
        for (Model& model : m_models)
            drawModel(model, false);
        m_display.mode = kept;
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    drawOverlays();
    if (!m_guides.empty())
        drawGuides();
    m_program->release();
    if (m_turnHandles)
    {
        QVector3D low, high;
        if (bounds(low, high))
        {
            // QPainter needs no culling or depth test
            glDisable(GL_CULL_FACE);
            glDisable(GL_DEPTH_TEST);
            m_handleMiddle = (low + high) * 0.5f;
            m_handleRadius = 0.8f * m_distance * std::tan(qDegreesToRadians(22.5f)) * std::min(aspect, 1.0f);
            drawHandles();
        }
    }
    drawGuidelines();
    if (m_display.crosshair)
    {
        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        QPainter painter(m_paintDevice.get());
        painter.setPen(QColor(255, 60, 60));
        const QPoint middle = rect().center();
        painter.drawLine(middle.x() - 10, middle.y(), middle.x() + 10, middle.y());
        painter.drawLine(middle.x(), middle.y() - 10, middle.x(), middle.y() + 10);
    }
    // FPS over each second
    ++m_framesCounted;
    const qint64 spent = m_frameClock.elapsed();
    if (spent >= 1000)
    {
        m_fps = m_framesCounted * 1000.0f / spent;
        m_framesCounted = 0;
        m_frameClock.restart();
    }
}

void StudioModelView::mousePressEvent(QMouseEvent* event)
{
    m_lastMouse = event->pos();
    m_pressedAt = event->pos();
    m_dragButton = event->button();
    // shift+drag pans
    if (event->button() == Qt::LeftButton && (event->modifiers() & Qt::ShiftModifier))
        m_dragButton = Qt::MiddleButton;
    if (m_dragButton == Qt::LeftButton)
    {
        m_handleAxis = handleAt(event->position(), &m_handleAt);
        if (m_handleAxis >= 0)
            redraw();
    }
}

void StudioModelView::mouseMoveEvent(QMouseEvent* event)
{
    const QPoint previous = m_lastMouse;
    const QPoint delta = event->pos() - m_lastMouse;
    m_lastMouse = event->pos();
    if (m_handleAxis >= 0)
    {
        const float degrees = handleTurn(previous, event->pos());
        if (degrees != 0.0f)
            emit turnedByHandle(m_handleAxis, degrees);
        return;
    }
    if (m_dragButton == Qt::NoButton)
    {
        // hover highlight
        const int hover = handleAt(event->position(), nullptr);
        if (hover != m_hoverAxis)
        {
            m_hoverAxis = hover;
            redraw();
        }
        return;
    }
    if (m_camera == FirstPerson)
        return;
    if (m_dragButton == Qt::LeftButton)
    {
        m_yaw -= delta.x() * 0.5f;
        m_pitch = std::clamp(m_pitch + delta.y() * 0.5f, -89.0f, 89.0f);
    }
    else if (m_dragButton == Qt::RightButton || m_dragButton == Qt::MiddleButton)
    {
        const float yaw = qDegreesToRadians(m_yaw);
        const float pitch = qDegreesToRadians(m_pitch);
        const QVector3D forward(-std::cos(pitch) * std::cos(yaw), -std::cos(pitch) * std::sin(yaw), -std::sin(pitch));
        const QVector3D right = QVector3D::crossProduct(forward, QVector3D(0, 0, 1)).normalized();
        const QVector3D up = QVector3D::crossProduct(right, forward).normalized();
        const float scale = m_distance / std::max(1, height());
        (m_camera == FreeLook ? m_freeEye : m_target) += (-right * delta.x() + up * delta.y()) * scale;
    }
    redraw();
}

void StudioModelView::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_surfacePicking && m_dragButton == Qt::LeftButton && m_handleAxis < 0 && (event->pos() - m_pressedAt).manhattanLength() < 4)
        pickSurface(event->position());
    m_dragButton = Qt::NoButton;
    if (m_handleAxis >= 0)
    {
        m_handleAxis = -1;
        redraw();
    }
}

void StudioModelView::setSurfacePicking(bool on)
{
    m_surfacePicking = on;
    const QCursor cursor(on ? Qt::CrossCursor : Qt::ArrowCursor);
    setCursor(cursor);
    if (m_surface)
        m_surface->setCursor(cursor);
}

void StudioModelView::pickSurface(const QPointF& at)
{
    if (m_models.empty() || !m_models[0].data)
        return;
    const Model& model = m_models[0];
    // nearest face under the point, as last drawn
    float nearest = 2.0f;
    int bestBone = -1;
    aiVector3D bestPoint;
    for (size_t part = 0; part < model.data->bodyParts.size(); ++part)
    {
        const StudioSubModel* sub = shownSubModel(model, part);
        if (!sub)
            continue;
        for (const StudioMesh& mesh : sub->meshes)
            for (size_t i = 0; i + 2 < mesh.triangles.size(); i += 3)
            {
                aiVector3D world[3];
                QPointF screen[3];
                float depth[3], w[3];
                bool behind = false;
                for (int c = 0; c < 3; ++c)
                {
                    const int vertex = mesh.triangles[i + c].vertex;
                    const int bone = sub->vertexBones[vertex];
                    if (bone < 0 || bone >= static_cast<int>(model.bones.size()))
                    {
                        behind = true;
                        break;
                    }
                    world[c] = model.bones[bone] * sub->vertices[vertex];
                    const QVector4D clip = m_viewProjection * QVector4D(world[c].x, world[c].y, world[c].z, 1.0f);
                    if (clip.w() <= 1e-6f)
                    {
                        behind = true;
                        break;
                    }
                    w[c] = clip.w();
                    depth[c] = clip.z() / clip.w();
                    screen[c] = QPointF((clip.x() / w[c] + 1.0f) * 0.5f * width(), (1.0f - clip.y() / w[c]) * 0.5f * height());
                }
                if (behind)
                    continue;
                // barycentric coordinates on screen
                const QPointF e1 = screen[1] - screen[0], e2 = screen[2] - screen[0], p = at - screen[0];
                const double area = e1.x() * e2.y() - e1.y() * e2.x();
                if (std::abs(area) < 1e-9)
                    continue;
                const double u = (p.x() * e2.y() - p.y() * e2.x()) / area;
                const double v = (e1.x() * p.y() - e1.y() * p.x()) / area;
                if (u < 0.0 || v < 0.0 || u + v > 1.0)
                    continue;
                const float weights[3] = { static_cast<float>(1.0 - u - v), static_cast<float>(u), static_cast<float>(v) };
                const float z = weights[0] * depth[0] + weights[1] * depth[1] + weights[2] * depth[2];
                if (z < -1.0f || z >= nearest)
                    continue;
                nearest = z;
                // perspective-correct
                float sum = 0.0f;
                aiVector3D point;
                for (int c = 0; c < 3; ++c)
                {
                    point += world[c] * (weights[c] / w[c]);
                    sum += weights[c] / w[c];
                }
                bestPoint = point / sum;
                const int closest = static_cast<int>(std::max_element(weights, weights + 3) - weights);
                bestBone = sub->vertexBones[mesh.triangles[i + closest].vertex];
            }
    }
    if (bestBone < 0)
        return;
    aiMatrix4x4 toBone = model.bones[bestBone];
    toBone.Inverse();
    const aiVector3D local = toBone * bestPoint;
    emit surfacePicked(bestBone, QVector3D(local.x, local.y, local.z));
}

void StudioModelView::mouseDoubleClickEvent(QMouseEvent*)
{
    fitView();
}

void StudioModelView::wheelEvent(QWheelEvent* event)
{
    if (m_camera == FreeLook)
    {
        const float yaw = qDegreesToRadians(m_yaw), pitch = qDegreesToRadians(m_pitch);
        const QVector3D away(std::cos(pitch) * std::cos(yaw), std::cos(pitch) * std::sin(yaw), std::sin(pitch));
        m_freeEye += away * (event->angleDelta().y() > 0 ? -kFreeStep * 3.0f : kFreeStep * 3.0f);
        redraw();
        return;
    }
    m_distance = std::clamp(m_distance * (event->angleDelta().y() > 0 ? 0.85f : 1.18f), 1.0f, 20000.0f);
    redraw();
}

void StudioModelView::keyPressEvent(QKeyEvent* event)
{
    // F: fit view
    if (event->key() == Qt::Key_F && event->modifiers() == Qt::NoModifier)
    {
        fitView();
        return;
    }
    if (m_camera != FreeLook)
    {
        QWidget::keyPressEvent(event);
        return;
    }
    const float yaw = qDegreesToRadians(m_yaw), pitch = qDegreesToRadians(m_pitch);
    const QVector3D forward = -QVector3D(std::cos(pitch) * std::cos(yaw), std::cos(pitch) * std::sin(yaw), std::sin(pitch));
    const QVector3D right = QVector3D::crossProduct(forward, QVector3D(0, 0, 1)).normalized();
    switch (event->key())
    {
    case Qt::Key_W: m_freeEye += forward * kFreeStep; break;
    case Qt::Key_S: m_freeEye -= forward * kFreeStep; break;
    case Qt::Key_D: m_freeEye += right * kFreeStep; break;
    case Qt::Key_A: m_freeEye -= right * kFreeStep; break;
    case Qt::Key_E: m_freeEye.setZ(m_freeEye.z() + kFreeStep); break;
    case Qt::Key_Q: m_freeEye.setZ(m_freeEye.z() - kFreeStep); break;
    default: QWidget::keyPressEvent(event); return;
    }
    redraw();
}
