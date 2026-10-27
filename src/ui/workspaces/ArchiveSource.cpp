// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#include "ArchiveSource.h"

#include "core/extractors/gma/GmaFile.h"
#include "core/extractors/pak/PakFile.h"
#include "core/extractors/rez/RezExtractor.h"
#include "core/extractors/vpk/VpkFile.h"
#include "core/extractors/xfs/XfsExtractor.h"
#include "utils/FileIO.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>


namespace
{

QString slashed(QString path)
{
    path.replace('\\', '/');
    return path;
}

class RezSource : public ArchiveSource
{
public:
    bool load(const QString& path)
    {
        if (!m_rez.Load(path.toStdString()))
            return false;
        for (const RezEntry& e : m_rez.GetEntries())
            m_entries.push_back({ slashed(QString::fromStdString(e.filename)), e.size });
        return true;
    }
    QString typeName() const override { return QCoreApplication::translate("ArchiveSource", "REZ (LithTech)"); }
    bool read(size_t index, QByteArray& data) const override
    {
        std::vector<char> bytes;
        if (index >= m_rez.GetEntries().size() || !m_rez.ExtractEntryToMemory(m_rez.GetEntries()[index], bytes))
            return false;
        data = QByteArray(bytes.data(), static_cast<qsizetype>(bytes.size()));
        return true;
    }

private:
    RezExtractor m_rez;
};

class PakSource : public ArchiveSource
{
public:
    bool load(const QString& path)
    {
        // mapped, not read: only the parts looked at take memory, however many paks are open
        auto file = std::make_shared<QFile>(path);
        if (!file->open(QIODevice::ReadOnly) || file->size() <= 0)
            return false;
        const uchar* bytes = file->map(0, file->size());
        if (!bytes)
            return false;
        const std::span<const uint8_t> data(bytes, static_cast<size_t>(file->size()));
        m_pak = std::make_unique<PakFile>(data, file, FileIO::toPath(path.toStdString()).filename().generic_u16string());
        if (!m_pak->ParseHeader() || !m_pak->ParseEntries())
            return false;
        for (const PakEntry_t& e : m_pak->GetEntries())
            m_entries.push_back({ slashed(QString::fromStdU16String(e.FilePath)), e.RealSize });
        return true;
    }
    QString typeName() const override { return QCoreApplication::translate("ArchiveSource", "PAK (Counter-Strike Online)"); }
    bool read(size_t index, QByteArray& data) const override
    {
        if (index >= m_pak->GetEntries().size())
            return false;
        auto [ok, bytes] = m_pak->UnpackEntry(m_pak->GetEntries()[index]);
        if (!ok)
            return false;
        data = QByteArray(reinterpret_cast<const char*>(bytes.data()), static_cast<qsizetype>(bytes.size()));
        return true;
    }

private:
    std::unique_ptr<PakFile> m_pak;
};

class XfsSource : public ArchiveSource
{
public:
    bool load(const QString& path)
    {
        if (!m_xfs.Load(path.toStdString()))
            return false;
        for (const XfsEntry& e : m_xfs.GetEntries())
            m_entries.push_back({ slashed(QString::fromStdString(e.filename)), e.unpackedSize });
        return true;
    }
    QString typeName() const override { return QCoreApplication::translate("ArchiveSource", "XFS (Xenesis)"); }
    bool read(size_t index, QByteArray& data) const override
    {
        std::vector<uint8_t> bytes;
        if (index >= m_xfs.GetEntries().size() || !m_xfs.ExtractEntryToMemory(m_xfs.GetEntries()[index], bytes))
            return false;
        data = QByteArray(reinterpret_cast<const char*>(bytes.data()), static_cast<qsizetype>(bytes.size()));
        return true;
    }

private:
    XfsExtractor m_xfs;
};

class VpkSource : public ArchiveSource
{
public:
    bool load(const QString& path)
    {
        if (!m_vpk.Load(path.toStdString()))
            return false;
        for (const VpkEntry& e : m_vpk.GetEntries())
            m_entries.push_back({ slashed(QString::fromStdString(e.fullPath)), static_cast<quint64>(e.preloadBytes) + e.entryLength });
        return true;
    }
    QString typeName() const override { return QCoreApplication::translate("ArchiveSource", "VPK (Source)"); }
    bool read(size_t index, QByteArray& data) const override
    {
        if (index >= m_vpk.GetEntries().size())
            return false;
        const std::vector<uint8_t> bytes = m_vpk.ExtractEntry(m_vpk.GetEntries()[index]);
        if (bytes.empty() && m_entries[index].size > 0)
            return false;
        data = QByteArray(reinterpret_cast<const char*>(bytes.data()), static_cast<qsizetype>(bytes.size()));
        return true;
    }

private:
    VpkFile m_vpk;
};

class GmaSource : public ArchiveSource
{
public:
    bool load(const QString& path)
    {
        if (!m_gma.Load(path.toStdString()))
            return false;
        for (const GmaEntry& e : m_gma.GetEntries())
            m_entries.push_back({ slashed(QString::fromStdString(e.fullPath)), e.size });
        return true;
    }
    QString typeName() const override { return QCoreApplication::translate("ArchiveSource", "GMA (Garry's Mod)"); }
    bool read(size_t index, QByteArray& data) const override
    {
        if (index >= m_gma.GetEntries().size())
            return false;
        const std::vector<uint8_t> bytes = m_gma.ExtractEntry(m_gma.GetEntries()[index]);
        if (bytes.empty() && m_entries[index].size > 0)
            return false;
        data = QByteArray(reinterpret_cast<const char*>(bytes.data()), static_cast<qsizetype>(bytes.size()));
        return true;
    }

private:
    GmaFile m_gma;
};

template <typename Source>
std::unique_ptr<ArchiveSource> load(const QString& path)
{
    auto source = std::make_unique<Source>();
    if (!source->load(path))
        return nullptr;
    return source;
}

} // namespace

bool ArchiveSource::isArchive(const QString& path)
{
    const QString ext = QFileInfo(path).suffix().toLower();
    return ext == "rez" || ext == "pak" || ext == "xfs" || ext == "vpk" || ext == "gma";
}

std::unique_ptr<ArchiveSource> ArchiveSource::open(const QString& path, QString& error)
{
    const QFileInfo info(path);
    const QString ext = info.suffix().toLower();
    std::unique_ptr<ArchiveSource> source;
    if (ext == "rez")
    {
        source = load<RezSource>(path);
    }
    else if (ext == "pak")
    {
        source = load<PakSource>(path);
    }
    else if (ext == "xfs")
    {
        source = load<XfsSource>(path);
    }
    else if (ext == "gma")
    {
        source = load<GmaSource>(path);
    }
    else if (ext == "vpk")
    {
        // the numbered parts hold data only; their _dir.vpk has the file list
        if (QRegularExpression("_\\d{3}$").match(info.completeBaseName()).hasMatch())
        {
            error = QCoreApplication::translate("ArchiveSource", "This is a data part of a VPK. Open the matching _dir.vpk instead.");
            return nullptr;
        }
        source = load<VpkSource>(path);
    }
    else
    {
        error = QCoreApplication::translate("ArchiveSource", "Not an archive Vortigaunt can read.");
        return nullptr;
    }

    if (!source)
    {
        error = QCoreApplication::translate("ArchiveSource", "The archive could not be read. It may be damaged or of a version Vortigaunt does not know.");
        return nullptr;
    }
    source->m_filePath = info.absoluteFilePath();
    return source;
}

QString ArchiveSource::safeRelativePath(const QString& entryPath)
{
    return slashed(QString::fromStdString(FileIO::fromPath(FileIO::safeRelativePath(entryPath.toStdString()))));
}
