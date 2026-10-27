// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#pragma once

#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QWheelEvent>
#include <QWidget>

#include <algorithm>
#include <functional>

// a texture zoomed with the wheel and moved by dragging; double-click fits it
class TextureView : public QWidget
{
public:
    enum class Channels
    {
        ColorAndAlpha,
        Color,
        Alpha,
    };

    explicit TextureView(QWidget* parent)
        : QWidget(parent)
    {
        setMinimumSize(200, 200);
        setMouseTracking(false);
        setCursor(Qt::OpenHandCursor);
    }

    std::function<void()> zoomChanged;
    QString placeholder;

    void setImage(const QImage& image, bool keepView)
    {
        m_image = image;
        updatePixmap();
        if (!keepView || m_fit)
            fit();
        else
            update();
    }

    void setChannels(Channels channels)
    {
        m_channels = channels;
        updatePixmap();
        update();
    }

    double zoom() const { return m_zoom; }

    void fit()
    {
        m_fit = true;
        if (m_image.isNull())
        {
            update();
            return;
        }
        const double room = (std::min)((width() - 16) / double(m_image.width()), (height() - 16) / double(m_image.height()));
        setZoom((std::clamp)(room, 0.01, 8.0), QPointF());
        centre();
    }

    void actualSize()
    {
        m_fit = false;
        setZoom(1.0, QPointF());
        centre();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), palette().color(QPalette::Base));
        if (m_pixmap.isNull())
        {
            painter.setPen(palette().color(QPalette::PlaceholderText));
            painter.drawText(rect(), Qt::AlignCenter, placeholder);
            return;
        }
        const QRectF target(m_origin, QSizeF(m_image.size()) * m_zoom);
        if (m_channels == Channels::ColorAndAlpha)
            painter.fillRect(target, checkerBrush());
        painter.setRenderHint(QPainter::SmoothPixmapTransform, m_zoom < 1.0);
        painter.drawPixmap(target, m_pixmap, QRectF(m_pixmap.rect()));
    }

    void resizeEvent(QResizeEvent*) override
    {
        if (m_fit)
            fit();
    }

    void wheelEvent(QWheelEvent* event) override
    {
        if (m_image.isNull())
            return;
        const double factor = event->angleDelta().y() > 0 ? 1.25 : 0.8;
        m_fit = false;
        setZoom((std::clamp)(m_zoom * factor, 0.01, 64.0), event->position());
        event->accept();
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        m_dragFrom = event->position();
        setCursor(Qt::ClosedHandCursor);
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (!(event->buttons() & Qt::LeftButton))
            return;
        m_origin += event->position() - m_dragFrom;
        m_dragFrom = event->position();
        m_fit = false;
        update();
    }

    void mouseReleaseEvent(QMouseEvent*) override { setCursor(Qt::OpenHandCursor); }

    void mouseDoubleClickEvent(QMouseEvent*) override
    {
        if (m_fit)
            actualSize();
        else
            fit();
    }

private:
    void updatePixmap()
    {
        if (m_image.isNull())
        {
            m_pixmap = QPixmap();
            return;
        }
        if (m_channels == Channels::Color)
        {
            m_pixmap = QPixmap::fromImage(m_image.convertToFormat(QImage::Format_RGB32));
        }
        else if (m_channels == Channels::Alpha)
        {
            const QImage argb = m_image.convertToFormat(QImage::Format_ARGB32);
            QImage alpha(argb.size(), QImage::Format_Grayscale8);
            for (int y = 0; y < argb.height(); ++y)
            {
                const auto* in = reinterpret_cast<const QRgb*>(argb.constScanLine(y));
                uchar* out = alpha.scanLine(y);
                for (int x = 0; x < argb.width(); ++x)
                    out[x] = static_cast<uchar>(qAlpha(in[x]));
            }
            m_pixmap = QPixmap::fromImage(alpha);
        }
        else
        {
            m_pixmap = QPixmap::fromImage(m_image);
        }
    }

    // zooms keeping the point under anchor in place; a null anchor keeps the centre
    void setZoom(double zoom, QPointF anchor)
    {
        if (anchor.isNull())
            anchor = QPointF(width() / 2.0, height() / 2.0);
        const QPointF onImage = (anchor - m_origin) / m_zoom;
        m_zoom = zoom;
        m_origin = anchor - onImage * m_zoom;
        if (zoomChanged)
            zoomChanged();
        update();
    }

    void centre()
    {
        const QSizeF size = QSizeF(m_image.size()) * m_zoom;
        m_origin = QPointF((width() - size.width()) / 2.0, (height() - size.height()) / 2.0);
        update();
    }

    QBrush checkerBrush() const
    {
        QPixmap tile(16, 16);
        const QColor base = palette().color(QPalette::Base);
        const QColor other = base.lightness() < 128 ? base.lighter(160) : base.darker(115);
        tile.fill(base);
        QPainter painter(&tile);
        painter.fillRect(0, 0, 8, 8, other);
        painter.fillRect(8, 8, 8, 8, other);
        return QBrush(tile);
    }

    QImage m_image;
    QPixmap m_pixmap;
    Channels m_channels = Channels::ColorAndAlpha;
    double m_zoom = 1.0;
    bool m_fit = true;
    QPointF m_origin;
    QPointF m_dragFrom;
};
