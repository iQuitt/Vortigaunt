#include "StudioModelView.h"

#include "core/mdl/StudioModel.h"

#include <QMouseEvent>
#include <QOpenGLShaderProgram>
#include <QPainter>
#include <QTimer>
#include <QVector4D>
#include <QWheelEvent>

#include <algorithm>
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
out vec2 vTexCoord;
void main()
{
    vNormal = normal;
    vTexCoord = texCoord;
    gl_Position = viewProjection * vec4(position, 1.0);
}
)";

const char* kFragmentShader = R"(
#version 330 core
in vec3 vNormal;
in vec2 vTexCoord;
uniform sampler2D tex;
uniform int mode;
uniform vec3 lightDirection;
uniform vec4 tint;
out vec4 color;
void main()
{
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
        color = vec4(texel.rgb, 1.0);
        return;
    }
    color = vec4(texel.rgb * min(light, 1.2), 1.0);
}
)";

constexpr float kPi = 3.14159265358979f;
constexpr int kRingPoints = 96;
constexpr float kHandleReach = 7.0f;  // pixels from a ring that take it
const QColor kAxisColours[3] = { QColor(232, 62, 62), QColor(110, 205, 60), QColor(62, 120, 240) };

QVector3D toQt(const aiVector3D& v)
{
    return QVector3D(v.x, v.y, v.z);
}

bool sameName(const std::string& a, const std::string& b)
{
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
        return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
    });
}

} // namespace

StudioModelView::StudioModelView(QWidget* parent)
    : QOpenGLWidget(parent)
{
    setMinimumSize(200, 200);
    m_timer = new QTimer(this);
    m_timer->setTimerType(Qt::PreciseTimer);
    m_timer->setInterval(16);
    connect(m_timer, &QTimer::timeout, this, [this]() {
        const float elapsed = m_clock.restart() / 1000.0f;
        if (m_playing)
            m_time += elapsed;
        update();
    });
    m_clock.start();
}

StudioModelView::~StudioModelView()
{
    if (context() && context()->isValid())
    {
        makeCurrent();
        releaseAll();
        delete m_program;
        m_program = nullptr;
        doneCurrent();
    }
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
                if (sameName(bone.name, parent.bones[j].name))
                    match = static_cast<int>(j);
            }
            entry.mergeBones.push_back(match);
        }
    }
    m_models.push_back(std::move(entry));
    if (m_models.size() == 1)
        m_fitPending = true;
    if (!m_timer->isActive())
        m_timer->start();
    update();
    return modelCount() - 1;
}

void StudioModelView::clearModels()
{
    if (context() && context()->isValid())
    {
        makeCurrent();
        releaseAll();
        doneCurrent();
    }
    m_models.clear();
    m_timer->stop();
    update();
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
    update();
}

int StudioModelView::sequence(int model) const
{
    return model >= 0 && model < modelCount() ? m_models[model].sequence : -1;
}

void StudioModelView::setPlaying(bool playing)
{
    m_playing = playing;
    update();
}

void StudioModelView::setTime(float seconds)
{
    m_time = std::max(0.0f, seconds);
    update();
}

void StudioModelView::setHoldUnlooped(bool hold)
{
    m_holdUnlooped = hold;
    update();
}

void StudioModelView::setPlacement(int model, const aiMatrix4x4& placement)
{
    if (model < 0 || model >= modelCount())
        return;
    m_models[model].placement = placement;
    update();
}

void StudioModelView::setBoneOffset(int model, int bone, const aiMatrix4x4& offset)
{
    if (model < 0 || model >= modelCount())
        return;
    m_models[model].offsetBone = bone;
    m_models[model].offset = offset;
    update();
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
    update();
}

void StudioModelView::setAimPitch(int model, float degrees)
{
    if (model < 0 || model >= modelCount())
        return;
    m_models[model].aimPitch = degrees;
    update();
}

void StudioModelView::updateBones()
{
    auto playing = [this](const StudioModel& data, int index, float pitch, int& frame, float& blend) {
        frame = 0;
        blend = 0.0f;
        if (index < 0 || index >= static_cast<int>(data.sequences.size()))
            return;
        const StudioSequence& sequence = data.sequences[index];
        const int at = static_cast<int>(m_time * std::max(1.0f, sequence.fps));
        frame = m_holdUnlooped && !(sequence.flags & 1) ? std::min(at, std::max(1, sequence.frames) - 1) : at % std::max(1, sequence.frames);
        if (sequence.blends > 1 && sequence.blendEnd != sequence.blendStart)
            blend = std::clamp((pitch - sequence.blendStart) / (sequence.blendEnd - sequence.blendStart), 0.0f, 1.0f);
    };

    for (Model& model : m_models)
    {
        const StudioModel& data = *model.data;
        int frame = 0;
        float blend = 0.0f;
        playing(data, model.sequence, model.aimPitch, frame, blend);
        std::vector<aiMatrix4x4> local = data.LocalBoneTransforms(model.sequence, frame, blend);

        if (model.mergeInto < 0 && model.gaitSequence >= 0)
        {
            int gaitFrame = 0;
            float gaitBlend = 0.0f;
            playing(data, model.gaitSequence, model.aimPitch, gaitFrame, gaitBlend);
            const std::vector<aiMatrix4x4> gait = data.LocalBoneTransforms(model.gaitSequence, gaitFrame, gaitBlend);
            for (size_t i = 0; i < data.bones.size() && !sameName(data.bones[i].name, "Bip01 Spine"); ++i)
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

void StudioModelView::buildVertices(const Model& model, std::vector<float>& out) const
{
    out.clear();
    for (const StudioBodyPart& part : model.data->bodyParts)
    {
        if (part.models.empty())
            continue;
        const StudioSubModel& sub = part.models.front();
        for (const StudioMesh& mesh : sub.meshes)
        {
            for (const StudioTriVertex& corner : mesh.triangles)
            {
                const aiMatrix4x4& bone = model.bones[sub.vertexBones[corner.vertex]];
                const aiVector3D position = bone * sub.vertices[corner.vertex];
                const aiMatrix4x4& normalBone = model.bones[sub.normalBones[corner.normal]];
                const aiVector3D normal = (aiMatrix3x3(normalBone) * sub.normals[corner.normal]).NormalizeSafe();
                out.insert(out.end(), { position.x, position.y, position.z, normal.x, normal.y, normal.z, corner.u, corner.v });
            }
        }
    }
}

void StudioModelView::upload(Model& model)
{
    const StudioModel& data = *model.data;
    model.meshes.clear();
    int first = 0;
    for (const StudioBodyPart& part : data.bodyParts)
    {
        if (part.models.empty())
            continue;
        for (const StudioMesh& mesh : part.models.front().meshes)
        {
            const int count = static_cast<int>(mesh.triangles.size());
            model.meshes.push_back({ mesh.texture, first, count });
            first += count;
        }
    }

    model.textures.assign(data.textures.size(), 0);
    for (size_t i = 0; i < data.textures.size(); ++i)
    {
        const StudioTexture& texture = data.textures[i];
        if (texture.pixels.empty() || texture.palette.size() < 768)
            continue;
        const bool masked = (texture.flags & STUDIO_NF_MASKED) != 0;
        std::vector<uint8_t> rgba(texture.pixels.size() * 4);
        for (size_t p = 0; p < texture.pixels.size(); ++p)
        {
            const uint8_t index = texture.pixels[p];
            rgba[p * 4 + 0] = texture.palette[index * 3 + 0];
            rgba[p * 4 + 1] = texture.palette[index * 3 + 1];
            rgba[p * 4 + 2] = texture.palette[index * 3 + 2];
            rgba[p * 4 + 3] = masked && index == 255 ? 0 : 255;
        }
        glGenTextures(1, &model.textures[i]);
        glBindTexture(GL_TEXTURE_2D, model.textures[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texture.width, texture.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, masked ? GL_NEAREST : GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, masked ? GL_NEAREST : GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }

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
    for (unsigned int texture : model.textures)
    {
        if (texture)
            glDeleteTextures(1, &texture);
    }
    model.textures.clear();
    glDeleteBuffers(1, &model.vbo);
    glDeleteVertexArrays(1, &model.vao);
    model.vbo = model.vao = 0;
    model.uploaded = false;
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
}

void StudioModelView::setGuides(const std::vector<Guide>& guides)
{
    m_guides = guides;
    update();
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

void StudioModelView::initializeGL()
{
    if (!initializeOpenGLFunctions())
        return;
    m_program = new QOpenGLShaderProgram();
    m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, kVertexShader);
    m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, kFragmentShader);
    m_glReady = m_program->link();
}

void StudioModelView::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
}

bool StudioModelView::bounds(QVector3D& low, QVector3D& high, int onlyModel) const
{
    bool any = false;
    for (int index = 0; index < modelCount(); ++index)
    {
        if (onlyModel >= 0 && index != onlyModel)
            continue;
        const Model& model = m_models[index];
        for (const StudioBodyPart& part : model.data->bodyParts)
        {
            if (part.models.empty())
                continue;
            const StudioSubModel& sub = part.models.front();
            for (size_t v = 0; v < sub.vertices.size(); ++v)
            {
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
    update();
}

void StudioModelView::setCameraAngles(float yaw, float pitch)
{
    m_yaw = yaw;
    m_pitch = std::clamp(pitch, -89.0f, 89.0f);
    update();
}

void StudioModelView::setTurnHandles(bool shown)
{
    m_turnHandles = shown;
    setMouseTracking(shown);
    update();
}

void StudioModelView::recenter()
{
    updateBones();
    QVector3D low, high;
    if (!bounds(low, high))
        return;
    m_target = (low + high) * 0.5f;
    update();
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
    QPainter painter(this);
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
    update();
}

void StudioModelView::setBackground(const QColor& colour)
{
    m_background = colour;
    update();
}

void StudioModelView::drawModel(Model& model, bool additivePass)
{
    const StudioModel& data = *model.data;
    glBindVertexArray(model.vao);
    for (const Mesh& mesh : model.meshes)
    {
        const int flags = mesh.texture >= 0 ? data.textures[mesh.texture].flags : 0;
        const bool additive = (flags & STUDIO_NF_ADDITIVE) != 0;
        if (additive != additivePass || mesh.count == 0)
            continue;
        const unsigned int texture = mesh.texture >= 0 ? model.textures[mesh.texture] : 0;
        glBindTexture(GL_TEXTURE_2D, texture);
        m_program->setUniformValue("mode", additive ? 2 : ((flags & STUDIO_NF_MASKED) ? 1 : 0));
        glDrawArrays(GL_TRIANGLES, mesh.first, mesh.count);
    }
    glBindVertexArray(0);
}

void StudioModelView::paintGL()
{
    const QColor background = m_background.isValid() ? m_background : palette().color(QPalette::Base);
    glClearColor(background.redF(), background.greenF(), background.blueF(), 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!m_glReady || m_models.empty())
    {
        QPainter painter(this);
        painter.setPen(palette().color(QPalette::PlaceholderText));
        painter.drawText(rect(), Qt::AlignCenter, placeholder);
        return;
    }

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
    const QVector3D eye = m_target + m_distance * QVector3D(std::cos(pitch) * std::cos(yaw), std::cos(pitch) * std::sin(yaw), std::sin(pitch));
    QMatrix4x4 view;
    view.lookAt(eye, m_target, QVector3D(0, 0, 1));
    QMatrix4x4 projection;
    const float aspect = width() / static_cast<float>(std::max(1, height()));
    if (m_orthographic)
    {
        const float half = m_distance * std::tan(qDegreesToRadians(22.5f));
        projection.ortho(-half * aspect, half * aspect, -half, half, m_distance * 0.01f, m_distance * 50.0f);
    }
    else
        projection.perspective(45.0f, aspect, m_distance * 0.01f, m_distance * 50.0f);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CW);
    m_program->bind();
    m_viewProjection = projection * view;
    m_eye = eye;
    m_program->setUniformValue("viewProjection", m_viewProjection);
    m_program->setUniformValue("tex", 0);
    m_program->setUniformValue("lightDirection", QVector3D(-0.4f, 0.3f, -1.0f).normalized());
    glActiveTexture(GL_TEXTURE0);

    std::vector<float> vertices;
    for (Model& model : m_models)
    {
        if (!model.uploaded)
            upload(model);
        buildVertices(model, vertices);
        glBindBuffer(GL_ARRAY_BUFFER, model.vbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)), vertices.data(), GL_STREAM_DRAW);
    }

    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    for (Model& model : m_models)
        drawModel(model, false);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glDepthMask(GL_FALSE);
    for (Model& model : m_models)
        drawModel(model, true);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
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
}

void StudioModelView::mousePressEvent(QMouseEvent* event)
{
    m_lastMouse = event->pos();
    m_dragButton = event->button();
    if (event->button() == Qt::LeftButton)
    {
        m_handleAxis = handleAt(event->position(), &m_handleAt);
        if (m_handleAxis >= 0)
            update();
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
            update();
        }
        return;
    }
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
        m_target += (-right * delta.x() + up * delta.y()) * scale;
    }
    update();
}

void StudioModelView::mouseReleaseEvent(QMouseEvent*)
{
    m_dragButton = Qt::NoButton;
    if (m_handleAxis >= 0)
    {
        m_handleAxis = -1;
        update();
    }
}

void StudioModelView::mouseDoubleClickEvent(QMouseEvent*)
{
    fitView();
}

void StudioModelView::wheelEvent(QWheelEvent* event)
{
    m_distance = std::clamp(m_distance * (event->angleDelta().y() > 0 ? 0.85f : 1.18f), 1.0f, 20000.0f);
    update();
}
