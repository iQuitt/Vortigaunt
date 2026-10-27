// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#include "PaletteWidget.h"

#include <QMouseEvent>
#include <QPainter>

PaletteWidget::PaletteWidget(QWidget* parent)
    : QWidget(parent)
    , m_cellSize(16)
    , m_hoveredIndex(-1)
{
    setFixedSize(16 * m_cellSize + 2, 16 * m_cellSize + 2);
    setMouseTracking(true);
}

void PaletteWidget::setPalette(const QVector<QRgb>& colors)
{
    m_colors = colors;
    update();
}

void PaletteWidget::clearPalette()
{
    m_colors.clear();
    update();
}

void PaletteWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    
    // Draw border
    painter.setPen(Qt::gray);
    painter.drawRect(0, 0, width() - 1, height() - 1);
    
    // Draw color cells
    for (int i = 0; i < 256 && i < m_colors.size(); i++) {
        int x = (i % 16) * m_cellSize + 1;
        int y = (i / 16) * m_cellSize + 1;
        
        QColor color(m_colors[i]);
        painter.fillRect(x, y, m_cellSize, m_cellSize, color);
    }
    
    // Fill empty cells with black
    for (int i = m_colors.size(); i < 256; i++) {
        int x = (i % 16) * m_cellSize + 1;
        int y = (i / 16) * m_cellSize + 1;
        painter.fillRect(x, y, m_cellSize, m_cellSize, Qt::black);
    }
}

void PaletteWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && !m_colors.isEmpty()) {
        int x = (event->pos().x() - 1) / m_cellSize;
        int y = (event->pos().y() - 1) / m_cellSize;
        int index = y * 16 + x;
        
        if (index >= 0 && index < m_colors.size()) {
            emit colorClicked(index, m_colors[index]);
        }
    }
    QWidget::mousePressEvent(event);
}

void PaletteWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && !m_colors.isEmpty()) {
        int x = (event->pos().x() - 1) / m_cellSize;
        int y = (event->pos().y() - 1) / m_cellSize;
        int index = y * 16 + x;
        
        if (index >= 0 && index < m_colors.size()) {
            emit colorDoubleClicked(index, m_colors[index]);
        }
    }
    QWidget::mouseDoubleClickEvent(event);
}
