// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#pragma once

#include <QVector>
#include <QWidget>
#include <QRgb>

// the 256 colours of a palette, 16 by 16; a click tells which one
class PaletteWidget : public QWidget
{
    Q_OBJECT
public:
    explicit PaletteWidget(QWidget* parent = nullptr);
    
    void setPalette(const QVector<QRgb>& colors);
    void clearPalette();
    
signals:
    void colorClicked(int index, QRgb color);
    void colorDoubleClicked(int index, QRgb color);
    
protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    
private:
    QVector<QRgb> m_colors;
    int m_cellSize = 16;
    int m_hoveredIndex = -1;
};
