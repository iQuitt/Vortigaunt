#include "QCFile.h"
#include <filesystem>
#include "VortigauntVersion.h"
#include "utils/FileIO.h"

void QCFile::AddSequence(const std::string& name, const std::string& smdPath, int fps)
{
    QCSequence seq;
    seq.Name = name;
    seq.SmdPath = smdPath;
    seq.Fps = fps;
    m_sequences.push_back(seq);
}

bool QCFile::Write(const std::string& outputPath)
{
    std::ofstream file(FileIO::toPath(outputPath));
    if (!file.is_open())
    {
        m_error = "Failed to create QC file: " + outputPath;
        return false;
    }


    file << "// Created by VortigauntTool ver " << VORTIGAUNT_VERSION_STRING << "\n\n";

    file << "$modelname \"" << m_settings.ModelName << "\"\n";
    
    file << "$cd \".\"\n";
    file << "$cdtexture \".\"\n";
    file << "$cliptotextures\n";
    
    file << "$scale " << m_settings.Scale << "\n\n";

    file << "$bodygroup \"studio\"\n";
    file << "{\n";
    file << "\tstudio \"" << m_settings.MeshName << "\"\n";
    file << "}\n\n";


    //maybe useless  to write, but who cares
    file << "$flags 0\n\n";
    file << "$cbox 0 0 0 0 0 0\n";
    file << "$bbox 0 0 0 0 0 0\n";

    if (m_sequences.empty())
    {
        // If no animations, create a default "idle" 
        file << "\n$sequence \"idle\" {\n";
        file << "\t\"" << m_settings.MeshName << "\"\n";
        file << "\tfps " << m_settings.Fps << "\n";
        file << "}\n";
    }
    else
    {
        for (const auto& seq : m_sequences)
        {
            file << "$sequence \"" << seq.Name << "\" {\n";
            file << "\t\"" << seq.SmdPath << "\"\n";
            file << "\tfps " << seq.Fps << "\n";
            file << "}\n";
        }
    }

    file.close();
    return true;
}
