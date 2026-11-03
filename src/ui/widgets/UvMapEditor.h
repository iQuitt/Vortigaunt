// Copyright (C) 2026 iQuit (https://github.com/iQuitt)
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#pragma once

#include <QImage>
#include <QPointF>
#include <QWidget>

#include <functional>
#include <vector>

class StudioModel;

// edits the UVs of one texture's meshes over the texture; positions are in texture pixels
class UvMapEditor : public QWidget
{
    Q_OBJECT
public:
    enum SelectMode
    {
        SelectPoints,
        SelectIslands
    };

    explicit UvMapEditor(QWidget* parent = nullptr);

    // onlyMesh -1 = every mesh of the texture; the selection stays while the faces are the same
    void setMeshes(const StudioModel& model, int texture, int onlyMesh);
    void setPicture(const QImage& picture);
    // writes the points back into the model's corners
    void apply(StudioModel& model) const;

    void setSelectMode(SelectMode mode) { m_selectMode = mode; }
    void selectAll();
    void selectNone();
    int selectedCount() const;

    // these act on the selection, or on every point when none is selected
    void move(const QPointF& pixels);
    void rotate(double degrees);
    void scale(double x, double y);
    void flip(bool horizontal);
    void fitToTexture();
    void snapToPixels();
    // the whole texture in view
    void frame();

signals:
    // finished: the mouse was let go, or a one-off edit
    void edited(bool finished);
    void selectionChanged(int count);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    struct MeshRef
    {
        int part, sub, mesh;
        size_t firstCorner;
        size_t corners;
    };

    QPointF toScreen(const QPointF& pixel) const;
    QPointF toPixel(const QPointF& screen) const;
    int pointAt(const QPointF& screen) const;
    int faceAt(const QPointF& screen) const;
    // the points of the faces linked to a face
    std::vector<int> island(int face) const;
    std::vector<int> targets() const;
    QPointF centre(const std::vector<int>& points) const;
    void transform(const std::function<QPointF(const QPointF&)>& change);
    void changeSelection(const std::vector<int>& points, Qt::KeyboardModifiers modifiers);

    std::vector<MeshRef> m_meshes;
    std::vector<int> m_cornerPoint;   // per corner
    std::vector<QPointF> m_points;    // pixels
    std::vector<size_t> m_pointCorner;  // a corner of each point, to read it back
    std::vector<char> m_selected;
    std::vector<std::vector<int>> m_pointFaces;
    int m_texture = -1, m_onlyMesh = -2;
    QSize m_size;
    QImage m_picture;
    SelectMode m_selectMode = SelectIslands;

    double m_zoom = 1.0;
    QPointF m_offset;
    bool m_viewMoved = false;  // zoomed or panned by hand: kept on resize
    enum class Drag { None, Pan, Move, Box } m_drag = Drag::None;
    QPointF m_pressAt, m_lastAt;
    Qt::KeyboardModifiers m_pressModifiers;
};
