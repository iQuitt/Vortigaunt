// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#include "UiUtils.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QScreen>
#include <QTableWidget>
#include <QWidget>

namespace
{

QPainterPath parseIconPath(const QString& d)
{
    QPainterPath path;
    QPointF current, start;
    QChar command;
    int i = 0;
    auto skipSeparators = [&]() {
        while (i < d.size() && (d[i].isSpace() || d[i] == ','))
            ++i;
    };
    auto number = [&]() {
        skipSeparators();
        const int from = i;
        if (i < d.size() && (d[i] == '-' || d[i] == '+'))
            ++i;
        while (i < d.size() && (d[i].isDigit() || d[i] == '.'))
            ++i;
        return d.mid(from, i - from).toDouble();
    };

    while (true)
    {
        skipSeparators();
        if (i >= d.size())
            break;
        const int before = i;
        if (d[i].isLetter())
            command = d[i++];
        const bool relative = command.isLower();
        const QChar c = command.toUpper();
        if (c == 'Z')
        {
            path.closeSubpath();
            current = start;
            skipSeparators();
            if (i < d.size() && !d[i].isLetter())
                break;
            continue;
        }
        if (c == 'M' || c == 'L')
        {
            const double x = number();
            const double y = number();
            const QPointF p = relative ? current + QPointF(x, y) : QPointF(x, y);
            if (c == 'M')
            {
                path.moveTo(p);
                start = p;
                command = relative ? 'l' : 'L';  // more pairs after a move are lines
            }
            else
            {
                path.lineTo(p);
            }
            current = p;
        }
        else if (c == 'H')
        {
            const double x = number();
            current.setX(relative ? current.x() + x : x);
            path.lineTo(current);
        }
        else if (c == 'V')
        {
            const double y = number();
            current.setY(relative ? current.y() + y : y);
            path.lineTo(current);
        }
        else
        {
            break;
        }
        if (i == before)
            break;
    }
    return path;
}

QPixmap drawIcon(const QPainterPath& path, int size, const QColor& color)
{
    const int scale = 2;
    QPixmap pixmap(size * scale, size * scale);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(size * scale / 24.0, size * scale / 24.0);
    painter.setPen(QPen(color, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);
    painter.end();
    pixmap.setDevicePixelRatio(scale);
    return pixmap;
}

} // namespace

namespace UiUtils
{

QIcon lineIcon(const QString& pathData, int size)
{
    const QPainterPath path = parseIconPath(pathData);
    const QPalette palette = QApplication::palette();
    QIcon icon;
    icon.addPixmap(drawIcon(path, size, palette.color(QPalette::Active, QPalette::ButtonText)), QIcon::Normal);
    icon.addPixmap(drawIcon(path, size, palette.color(QPalette::Disabled, QPalette::ButtonText)), QIcon::Disabled);
    return icon;
}

QString formatSize(quint64 size)
{
    if (size < 1024ull)
        return QString::number(size) + " B";
    else if (size < 1024ull * 1024)
        return QString::number(size / 1024.0, 'f', 1) + " KB";
    else if (size < 1024ull * 1024 * 1024)
        return QString::number(size / (1024.0 * 1024.0), 'f', 2) + " MB";
    else
        return QString::number(size / (1024.0 * 1024.0 * 1024.0), 'f', 2) + " GB";
}

void resizeToScreen(QWidget* widget, double width, double height)
{
    if (!widget)
        return;

    QScreen* screen = QGuiApplication::primaryScreen();
    QSize screenSize = screen ? screen->availableGeometry().size() : QSize(1920, 1080);
    widget->resize(screenSize.width() * width, screenSize.height() * height);
}

int filterTableRows(QTableWidget* table, const QString& searchText)
{
    if (!table)
        return 0;

    const QString searchLower = searchText.toLower();
    int visibleCount = 0;

    for (int row = 0; row < table->rowCount(); ++row)
    {
        auto* pathItem = table->item(row, 0);
        if (!pathItem)
        {
            table->setRowHidden(row, true);
            continue;
        }

        const bool matches = searchText.isEmpty() || pathItem->text().toLower().contains(searchLower);

        table->setRowHidden(row, !matches);
        if (matches)
            ++visibleCount;
    }

    return visibleCount;
}

void selectVisibleRows(QTableWidget* table)
{
    if (!table)
        return;

    table->setSelectionMode(QAbstractItemView::MultiSelection);
    table->clearSelection();

    for (int row = 0; row < table->rowCount(); ++row)
    {
        if (!table->isRowHidden(row))
            table->selectRow(row);
    }

    table->setSelectionMode(QAbstractItemView::ExtendedSelection);
}

} // namespace UiUtils
