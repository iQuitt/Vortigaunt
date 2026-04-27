#pragma once

#ifdef QT_CORE_LIB

#include "core/extractors/IExtractor.h"
#include <vector>
#include <memory>
#include <QMap>

class ExtractArchive
{
public:
    static ExtractArchive& Instance();

    void Register(std::shared_ptr<IExtractor> extractor);

    void Init();
    void ShutdownAll();

    std::shared_ptr<IExtractor> GetExtractorFor(const QString& filePath) const;
    std::vector<std::shared_ptr<IExtractor>> GetAllExtractors() const;

private:
    ExtractArchive() = default;
    std::vector<std::shared_ptr<IExtractor>> m_extractors;
};

#endif // QT_CORE_LIB
