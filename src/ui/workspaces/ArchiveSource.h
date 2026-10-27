// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#pragma once

#include <QByteArray>
#include <QString>

#include <memory>
#include <vector>

// One opened REZ, PAK, XFS, VPK or GMA archive, read the same way whatever its kind
class ArchiveSource
{
public:
    struct Entry
    {
        QString path;  // '/' separated, as stored in the archive
        quint64 size = 0;
    };

    virtual ~ArchiveSource() = default;

    static bool isArchive(const QString& path);
    static std::unique_ptr<ArchiveSource> open(const QString& path, QString& error);

    virtual QString typeName() const = 0;
    virtual bool read(size_t index, QByteArray& data) const = 0;

    const QString& filePath() const { return m_filePath; }
    const std::vector<Entry>& entries() const { return m_entries; }

    // the entry's path made safe to write below a folder: no "..", no drive letters, no illegal characters
    static QString safeRelativePath(const QString& entryPath);

protected:
    QString m_filePath;
    std::vector<Entry> m_entries;
};
