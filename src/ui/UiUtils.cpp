#include "UiUtils.h"

#include <QAbstractItemView>
#include <QGuiApplication>
#include <QScreen>
#include <QTableWidget>
#include <QWidget>

namespace UiUtils
{

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

void resizeToScreen(QWidget* widget, double widthFactor, double heightFactor)
{
    if (!widget)
        return;

    QScreen* screen = QGuiApplication::primaryScreen();
    QSize screenSize = screen ? screen->availableGeometry().size() : QSize(1920, 1080);
    widget->resize(screenSize.width() * widthFactor, screenSize.height() * heightFactor);
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
