// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#include "IConverter.h"
#include "utils/FileIO.h"

#include <algorithm>

ConvertAssets ConvertAll(IConverter& converter, const std::string& inputPath, const std::string& outputDir, const ConvertProgress& progress)
{
    namespace fs = std::filesystem;

    std::vector<std::pair<fs::path, fs::path>> jobs;
    const fs::path start = FileIO::toPath(inputPath);
    const fs::path outRoot = FileIO::toPath(outputDir);
    std::error_code ec;
    if (fs::is_regular_file(start, ec))
    {
        jobs.emplace_back(start, outRoot);
    }
    else if (fs::is_directory(start, ec))
    {
        for (auto it = fs::recursive_directory_iterator(start, fs::directory_options::skip_permission_denied, ec);
             it != fs::recursive_directory_iterator(); it.increment(ec))
        {
            if (ec) break;
            if (it->is_regular_file(ec) && converter.CanConvert(FileIO::fromPath(it->path())))
            {
                jobs.emplace_back(it->path(), outRoot / it->path().parent_path().lexically_relative(start));
            }
        }
        std::sort(jobs.begin(), jobs.end());
    }

    ConvertAssets summary;
    summary.total = static_cast<int>(jobs.size());
    for (size_t i = 0; i < jobs.size(); ++i)
    {
        const std::string input = FileIO::fromPath(jobs[i].first);
        fs::create_directories(jobs[i].second, ec);
        const ConvertResult result = converter.ConvertFile(input, FileIO::fromPath(jobs[i].second.lexically_normal()));
        if (result.ok)
        {
            summary.converted++;
        }
        else
        {
            summary.failed.emplace_back(input, result.error.empty() ? std::string("failed") : result.error);
        }
        if (progress && !progress(static_cast<int>(i), summary.total, input, result))
        {
            summary.cancelled = true;
            break;
        }
    }
    return summary;
}
