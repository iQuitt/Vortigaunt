// Copyright (C) 2026 iQuit (https://github.com/iQuitt)
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#include "ui/widgets/UvMapEditor.h"

#include "core/mdl/StudioModel.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <map>
#include <tuple>

namespace
{
constexpr double kPickRadius = 6.0;
constexpr double kClickDistance = 3.0;

bool insideTriangle(const QPointF& p, const QPointF& a, const QPointF& b, const QPointF& c)
{
    auto side = [](const QPointF& p, const QPointF& a, const QPointF& b) { return (b.x() - a.x()) * (p.y() - a.y()) - (b.y() - a.y()) * (p.x() - a.x()); };
    const double d1 = side(p, a, b), d2 = side(p, b, c), d3 = side(p, c, a);
    return !(((d1 < 0) || (d2 < 0) || (d3 < 0)) && ((d1 > 0) || (d2 > 0) || (d3 > 0)));
}
}

UvMapEditor::UvMapEditor(QWidget* parent) : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(false);
    setMinimumSize(200, 200);
}

void UvMapEditor::setMeshes(const StudioModel& model, int texture, int onlyMesh)
{
    std::vector<MeshRef> meshes;
    size_t total = 0;
    int number = -1;
    for (size_t b = 0; b < model.bodyParts.size(); ++b)
        for (size_t s = 0; s < model.bodyParts[b].models.size(); ++s)
            for (size_t k = 0; k < model.bodyParts[b].models[s].meshes.size(); ++k)
            {
                const StudioMesh& mesh = model.bodyParts[b].models[s].meshes[k];
                if (texture < 0 || mesh.texture != texture) continue;
                ++number;
                if (onlyMesh >= 0 && number != onlyMesh) continue;
                meshes.push_back({ static_cast<int>(b), static_cast<int>(s), static_cast<int>(k), total, mesh.triangles.size() });
                total += mesh.triangles.size();
            }
    const bool known = texture >= 0 && texture < static_cast<int>(model.textures.size());
    const QSize previousSize = m_size;
    m_size = known ? QSize(std::max(1, model.textures[texture].width), std::max(1, model.textures[texture].height)) : QSize(1, 1);

    std::vector<QPointF> corners(total);
    std::vector<int> vertexOf(total);
    for (const MeshRef& ref : meshes)
    {
        const StudioMesh& mesh = model.bodyParts[ref.part].models[ref.sub].meshes[ref.mesh];
        for (size_t i = 0; i < ref.corners; ++i)
        {
            corners[ref.firstCorner + i] = QPointF(mesh.triangles[i].u * m_size.width(), mesh.triangles[i].v * m_size.height());
            vertexOf[ref.firstCorner + i] = mesh.triangles[i].vertex;
        }
    }

    auto sameRef = [](const MeshRef& a, const MeshRef& b) { return a.part == b.part && a.sub == b.sub && a.mesh == b.mesh && a.corners == b.corners; };
    const bool same = texture == m_texture && onlyMesh == m_onlyMesh && meshes.size() == m_meshes.size() &&
                      std::equal(meshes.begin(), meshes.end(), m_meshes.begin(), sameRef);
    if (same)
    {
        // an edit or an undo: only the positions moved
        for (size_t p = 0; p < m_points.size(); ++p) m_points[p] = corners[m_pointCorner[p]];
        update();
        return;
    }

    // corners on one vertex with one UV are one point, so faces stay joined
    m_meshes = std::move(meshes);
    m_texture = texture;
    m_onlyMesh = onlyMesh;
    m_cornerPoint.assign(total, -1);
    m_points.clear();
    m_pointCorner.clear();
    std::map<std::tuple<int, int, int, long, long>, int> welded;
    for (const MeshRef& ref : m_meshes)
        for (size_t i = 0; i < ref.corners; ++i)
        {
            const size_t c = ref.firstCorner + i;
            const auto key = std::make_tuple(ref.part, ref.sub, vertexOf[c], std::lround(corners[c].x() * 8.0), std::lround(corners[c].y() * 8.0));
            auto [at, added] = welded.emplace(key, static_cast<int>(m_points.size()));
            if (added)
            {
                m_points.push_back(corners[c]);
                m_pointCorner.push_back(c);
            }
            m_cornerPoint[c] = at->second;
        }
    m_selected.assign(m_points.size(), 0);
    m_pointFaces.assign(m_points.size(), {});
    for (size_t f = 0; f * 3 + 2 < total; ++f)
        for (int k = 0; k < 3; ++k) m_pointFaces[m_cornerPoint[f * 3 + k]].push_back(static_cast<int>(f));
    emit selectionChanged(0);
    if (m_size != previousSize) frame();
    update();
}

void UvMapEditor::setPicture(const QImage& picture)
{
    m_picture = picture;
    update();
}

void UvMapEditor::apply(StudioModel& model) const
{
    for (const MeshRef& ref : m_meshes)
    {
        if (ref.part >= static_cast<int>(model.bodyParts.size()) || ref.sub >= static_cast<int>(model.bodyParts[ref.part].models.size())) continue;
        StudioSubModel& sub = model.bodyParts[ref.part].models[ref.sub];
        if (ref.mesh >= static_cast<int>(sub.meshes.size()) || sub.meshes[ref.mesh].triangles.size() != ref.corners) continue;
        StudioMesh& mesh = sub.meshes[ref.mesh];
        for (size_t i = 0; i < ref.corners; ++i)
        {
            const QPointF& point = m_points[m_cornerPoint[ref.firstCorner + i]];
            mesh.triangles[i].u = static_cast<float>(point.x() / m_size.width());
            mesh.triangles[i].v = static_cast<float>(point.y() / m_size.height());
        }
    }
}

void UvMapEditor::selectAll()
{
    std::fill(m_selected.begin(), m_selected.end(), 1);
    emit selectionChanged(selectedCount());
    update();
}

void UvMapEditor::selectNone()
{
    std::fill(m_selected.begin(), m_selected.end(), 0);
    emit selectionChanged(0);
    update();
}

int UvMapEditor::selectedCount() const
{
    return static_cast<int>(std::count(m_selected.begin(), m_selected.end(), 1));
}

std::vector<int> UvMapEditor::targets() const
{
    std::vector<int> out;
    for (size_t p = 0; p < m_selected.size(); ++p)
        if (m_selected[p]) out.push_back(static_cast<int>(p));
    if (out.empty())
        for (size_t p = 0; p < m_points.size(); ++p) out.push_back(static_cast<int>(p));
    return out;
}

QPointF UvMapEditor::centre(const std::vector<int>& points) const
{
    if (points.empty()) return QPointF();
    double minX = m_points[points[0]].x(), maxX = minX, minY = m_points[points[0]].y(), maxY = minY;
    for (int p : points)
    {
        minX = std::min(minX, m_points[p].x()), maxX = std::max(maxX, m_points[p].x());
        minY = std::min(minY, m_points[p].y()), maxY = std::max(maxY, m_points[p].y());
    }
    return QPointF((minX + maxX) / 2.0, (minY + maxY) / 2.0);
}

void UvMapEditor::transform(const std::function<QPointF(const QPointF&)>& change)
{
    if (m_points.empty()) return;
    for (int p : targets()) m_points[p] = change(m_points[p]);
    update();
    emit edited(true);
}

void UvMapEditor::move(const QPointF& pixels)
{
    transform([pixels](const QPointF& p) { return p + pixels; });
}

void UvMapEditor::rotate(double degrees)
{
    const QPointF pivot = centre(targets());
    const double radians = degrees * 3.14159265358979 / 180.0, c = std::cos(radians), s = std::sin(radians);
    transform([=](const QPointF& p) {
        const QPointF d = p - pivot;
        return pivot + QPointF(d.x() * c - d.y() * s, d.x() * s + d.y() * c);
    });
}

void UvMapEditor::scale(double x, double y)
{
    const QPointF pivot = centre(targets());
    transform([=](const QPointF& p) { return pivot + QPointF((p.x() - pivot.x()) * x, (p.y() - pivot.y()) * y); });
}

void UvMapEditor::flip(bool horizontal)
{
    scale(horizontal ? -1.0 : 1.0, horizontal ? 1.0 : -1.0);
}

void UvMapEditor::fitToTexture()
{
    const std::vector<int> points = targets();
    if (points.empty()) return;
    double minX = m_points[points[0]].x(), maxX = minX, minY = m_points[points[0]].y(), maxY = minY;
    for (int p : points)
    {
        minX = std::min(minX, m_points[p].x()), maxX = std::max(maxX, m_points[p].x());
        minY = std::min(minY, m_points[p].y()), maxY = std::max(maxY, m_points[p].y());
    }
    const double w = std::max(maxX - minX, 1e-6), h = std::max(maxY - minY, 1e-6);
    const double width = m_size.width() - 1, height = m_size.height() - 1;
    transform([=](const QPointF& p) { return QPointF((p.x() - minX) / w * width, (p.y() - minY) / h * height); });
}

void UvMapEditor::snapToPixels()
{
    transform([](const QPointF& p) { return QPointF(std::round(p.x()), std::round(p.y())); });
}

void UvMapEditor::frame()
{
    const double margin = 16.0;
    m_zoom = std::max(0.01, std::min((width() - 2 * margin) / m_size.width(), (height() - 2 * margin) / m_size.height()));
    m_offset = QPointF((width() - m_size.width() * m_zoom) / 2.0, (height() - m_size.height() * m_zoom) / 2.0);
    m_viewMoved = false;
    update();
}

QPointF UvMapEditor::toScreen(const QPointF& pixel) const
{
    return pixel * m_zoom + m_offset;
}

QPointF UvMapEditor::toPixel(const QPointF& screen) const
{
    return (screen - m_offset) / m_zoom;
}

int UvMapEditor::pointAt(const QPointF& screen) const
{
    int best = -1;
    double bestDistance = kPickRadius * kPickRadius;
    for (size_t p = 0; p < m_points.size(); ++p)
    {
        const QPointF d = toScreen(m_points[p]) - screen;
        const double distance = d.x() * d.x() + d.y() * d.y();
        if (distance <= bestDistance) best = static_cast<int>(p), bestDistance = distance;
    }
    return best;
}

int UvMapEditor::faceAt(const QPointF& screen) const
{
    const QPointF at = toPixel(screen);
    for (size_t f = m_cornerPoint.size() / 3; f-- > 0;)
        if (insideTriangle(at, m_points[m_cornerPoint[f * 3]], m_points[m_cornerPoint[f * 3 + 1]], m_points[m_cornerPoint[f * 3 + 2]]))
            return static_cast<int>(f);
    return -1;
}

std::vector<int> UvMapEditor::island(int face) const
{
    std::vector<char> seenFace(m_cornerPoint.size() / 3, 0), seenPoint(m_points.size(), 0);
    std::vector<int> faces{ face }, points;
    seenFace[face] = 1;
    while (!faces.empty())
    {
        const int f = faces.back();
        faces.pop_back();
        for (int k = 0; k < 3; ++k)
        {
            const int p = m_cornerPoint[f * 3 + k];
            if (seenPoint[p]) continue;
            seenPoint[p] = 1;
            points.push_back(p);
            for (int next : m_pointFaces[p])
                if (!seenFace[next]) seenFace[next] = 1, faces.push_back(next);
        }
    }
    return points;
}

void UvMapEditor::changeSelection(const std::vector<int>& points, Qt::KeyboardModifiers modifiers)
{
    if (!(modifiers & (Qt::ShiftModifier | Qt::ControlModifier))) std::fill(m_selected.begin(), m_selected.end(), 0);
    const char value = (modifiers & Qt::ControlModifier) ? 0 : 1;
    for (int p : points) m_selected[p] = value;
    emit selectionChanged(selectedCount());
    update();
}

void UvMapEditor::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor(40, 40, 44));
    const QRectF area(toScreen(QPointF(0, 0)), toScreen(QPointF(m_size.width(), m_size.height())));
    if (!m_picture.isNull()) painter.drawImage(area, m_picture);
    painter.setPen(QPen(QColor(120, 120, 130), 0));
    painter.drawRect(area);

    // faces with every corner selected are filled
    const QColor accent(255, 152, 0);
    QColor fill = accent;
    fill.setAlpha(70);
    painter.setPen(QPen(QColor(255, 255, 255, 170), 0));
    for (size_t f = 0; f * 3 + 2 < m_cornerPoint.size(); ++f)
    {
        const int a = m_cornerPoint[f * 3], b = m_cornerPoint[f * 3 + 1], c = m_cornerPoint[f * 3 + 2];
        const QPointF corner[3] = { toScreen(m_points[a]), toScreen(m_points[b]), toScreen(m_points[c]) };
        const bool chosen = m_selected[a] && m_selected[b] && m_selected[c];
        painter.setBrush(chosen ? QBrush(fill) : Qt::NoBrush);
        painter.drawPolygon(corner, 3);
    }
    painter.setPen(Qt::NoPen);
    painter.setBrush(accent);
    for (size_t p = 0; p < m_points.size(); ++p)
        if (m_selected[p])
        {
            const QPointF at = toScreen(m_points[p]);
            painter.drawRect(QRectF(at.x() - 2.0, at.y() - 2.0, 4.0, 4.0));
        }
    if (m_drag == Drag::Box)
    {
        painter.setPen(QPen(accent, 0, Qt::DashLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(QRectF(m_pressAt, m_lastAt).normalized());
    }
}

void UvMapEditor::mousePressEvent(QMouseEvent* event)
{
    setFocus();
    m_pressAt = m_lastAt = event->position();
    m_pressModifiers = event->modifiers();
    if (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton)
    {
        m_drag = Drag::Pan;
        return;
    }
    if (event->button() != Qt::LeftButton) return;
    m_drag = Drag::Box;
    if (m_pressModifiers & (Qt::ShiftModifier | Qt::ControlModifier)) return;
    // pressed on a point or face: it is taken and dragged
    const int point = pointAt(m_pressAt);
    const int face = point < 0 && m_selectMode == SelectIslands ? faceAt(m_pressAt) : -1;
    if (point < 0 && face < 0) return;
    bool chosen = point >= 0 ? m_selected[point] != 0 : true;
    if (face >= 0)
        for (int k = 0; k < 3; ++k) chosen = chosen && m_selected[m_cornerPoint[face * 3 + k]];
    if (!chosen)
    {
        if (m_selectMode == SelectIslands)
            changeSelection(island(face >= 0 ? face : m_pointFaces[point].front()), Qt::NoModifier);
        else
            changeSelection({ point }, Qt::NoModifier);
    }
    m_drag = Drag::Move;
}

void UvMapEditor::mouseMoveEvent(QMouseEvent* event)
{
    const QPointF at = event->position();
    const QPointF delta = at - m_lastAt;
    m_lastAt = at;
    if (m_drag == Drag::Pan)
        m_offset += delta, m_viewMoved = true;
    else if (m_drag == Drag::Move)
    {
        for (size_t p = 0; p < m_points.size(); ++p)
            if (m_selected[p]) m_points[p] += delta / m_zoom;
        emit edited(false);
    }
    update();
}

void UvMapEditor::mouseReleaseEvent(QMouseEvent* event)
{
    const QPointF moved = event->position() - m_pressAt;
    const bool click = std::hypot(moved.x(), moved.y()) < kClickDistance;
    if (m_drag == Drag::Move && !click)
        emit edited(true);
    else if (m_drag == Drag::Box)
    {
        std::vector<int> points;
        if (click)
        {
            const int point = pointAt(m_pressAt);
            const int face = point < 0 && m_selectMode == SelectIslands ? faceAt(m_pressAt) : -1;
            if (m_selectMode == SelectIslands && (point >= 0 || face >= 0))
                points = island(face >= 0 ? face : m_pointFaces[point].front());
            else if (point >= 0)
                points.push_back(point);
        }
        else
        {
            const QRectF box(toPixel(m_pressAt), toPixel(event->position()));
            const QRectF area = box.normalized();
            std::vector<char> taken(m_points.size(), 0);
            for (size_t p = 0; p < m_points.size(); ++p)
                if (area.contains(m_points[p]) && !taken[p])
                {
                    if (m_selectMode == SelectIslands)
                        for (int q : island(m_pointFaces[p].front())) taken[q] = 1;
                    else
                        taken[p] = 1;
                }
            for (size_t p = 0; p < taken.size(); ++p)
                if (taken[p]) points.push_back(static_cast<int>(p));
        }
        changeSelection(points, m_pressModifiers);
    }
    m_drag = Drag::None;
    update();
}

void UvMapEditor::wheelEvent(QWheelEvent* event)
{
    const QPointF at = event->position();
    const QPointF pixel = toPixel(at);
    m_zoom = std::clamp(m_zoom * std::pow(1.15, event->angleDelta().y() / 120.0), 0.01, 256.0);
    m_offset = at - pixel * m_zoom;
    m_viewMoved = true;
    update();
}

void UvMapEditor::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_A && (event->modifiers() & Qt::ControlModifier))
        selectAll();
    else if (event->key() == Qt::Key_Escape)
        selectNone();
    else if (event->key() == Qt::Key_F)
        frame();
    else
        QWidget::keyPressEvent(event);
}

void UvMapEditor::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (!m_viewMoved) frame();
}
