#ifdef QT_CORE_LIB

#include "ExtractArchive.h"
#include <QFileInfo>
#include <QDebug>

ExtractArchive& ExtractArchive::Instance()
{
    static ExtractArchive instance;
    return instance;
}

void ExtractArchive::Register(std::shared_ptr<IExtractor> extractor)
{
    if (extractor)
    {
        m_extractors.push_back(extractor);
    }
}

void ExtractArchive::Init()
{
    for (auto& extractor : m_extractors)
    {
        extractor->Initialize();
    }
}

void ExtractArchive::ShutdownAll()
{
    // Shutdown in reverse order? Not usually necessary for extractors but for now it is enough
    for (auto it = m_extractors.rbegin(); it != m_extractors.rend(); ++it)
    {
        (*it)->Shutdown();
    }
    m_extractors.clear();
}

std::shared_ptr<IExtractor> ExtractArchive::GetExtractorFor(const QString& filePath) const
{
    for (const auto& extractor : m_extractors)
    {
        if (extractor->CanHandle(filePath))
        {
            return extractor;
        }
    }
    return nullptr;
}

std::vector<std::shared_ptr<IExtractor>> ExtractArchive::GetAllExtractors() const
{
    return m_extractors;
}

#endif // QT_CORE_LIB
