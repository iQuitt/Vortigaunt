#pragma once

#include <QString>

class QTableWidget;
class QWidget;

// Small helpers shared by the archive viewer windows and the tool dialogs.
namespace UiUtils
{
    // Human readable byte count: "512 B", "1.5 KB", "12.34 MB", "1.20 GB"
    QString formatSize(quint64 size);

    // Resize a window to a fraction of the primary screen (falls back to 1920x1080)
    void resizeToScreen(QWidget* widget, double widthFactor, double heightFactor);
    inline void resizeToScreen(QWidget* widget, double factor)
    {
        resizeToScreen(widget, factor, factor);
    }

    // Hide every row whose first column does not contain searchText.
    // Returns the number of rows left visible.
    int filterTableRows(QTableWidget* table, const QString& searchText);

    // Select all rows that are currently visible (i.e. survived the filter).
    void selectVisibleRows(QTableWidget* table);
}
