// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#include "QCFile.h"
#include <filesystem>
#include "VortigauntVersion.h"
#include "utils/FileIO.h"

#ifdef QT_CORE_LIB
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QTextStream>
#endif

void QCFile::AddSequence(const std::string& name, const std::string& smdPath, int fps)
{
    QCSequence seq;
    seq.Name = name;
    seq.SmdPath = smdPath;
    seq.Fps = fps;
    m_sequences.push_back(seq);
}

QCCompileResult QCFile::Compile(const std::string& studiomdlPath, const std::string& qcPath)
{
    QCCompileResult result;
#ifdef QT_CORE_LIB
    const QFileInfo compiler(QString::fromStdString(studiomdlPath));
    const QFileInfo qc(QString::fromStdString(qcPath));
    if (studiomdlPath.empty() || !compiler.isFile())
    {
        result.Error = "studiomdl was not found. Set its location in Settings.";
        return result;
    }
    if (!qc.isFile())
    {
        result.Error = "The QC file does not exist: " + qcPath;
        return result;
    }

    // Which model the QC makes, to check afterwards that it really was made.
    QString modelName;
    {
        QFile file(qc.absoluteFilePath());
        if (file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            static const QRegularExpression pattern(QStringLiteral("^\\s*\\$modelname\\s+\"?([^\"\\r\\n]+)\"?"),
                                                    QRegularExpression::CaseInsensitiveOption | QRegularExpression::MultilineOption);
            const QRegularExpressionMatch match = pattern.match(QString::fromUtf8(file.readAll()));
            if (match.hasMatch())
            {
                modelName = match.captured(1).trimmed();
            }
        }
    }
    const QDateTime started = QDateTime::currentDateTime().addSecs(-2);

    QProcess process;
    process.setWorkingDirectory(qc.absolutePath());
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(compiler.absoluteFilePath(), { qc.fileName() });
    if (!process.waitForStarted(10000))
    {
        result.Error = "studiomdl could not be started: " + process.errorString().toStdString();
        return result;
    }
    process.closeWriteChannel();
    if (!process.waitForFinished(10 * 60 * 1000))
    {
        process.kill();
        process.waitForFinished(3000);
        result.Error = "studiomdl did not finish within ten minutes and was stopped.";
        return result;
    }
    const QString output = QString::fromLocal8Bit(process.readAll());
    result.Output = output.toStdString();

    QString modelPath;
    if (!modelName.isEmpty())
    {
        modelPath = QDir(qc.absolutePath()).absoluteFilePath(modelName);
    }
    const QFileInfo model(modelPath);
    const bool written = !modelPath.isEmpty() && model.isFile() && model.lastModified() >= started;

    // studiomdl announces a failure in its output; older builds still exit with 0.
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0 ||
        output.contains(QStringLiteral("ERROR")) || (!modelPath.isEmpty() && !written))
    {
        // the reason is what follows the error banner, or failing that the last thing said
        QString reason;
        const QStringList lines = output.split(QRegularExpression(QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts);
        for (int i = 0; i < lines.size(); i++)
        {
            if (lines[i].contains(QStringLiteral("ERROR")) && i + 1 < lines.size())
            {
                reason = lines[i + 1].trimmed();
                break;
            }
        }
        if (reason.isEmpty() && !lines.isEmpty())
        {
            reason = lines.last().trimmed();
        }
        result.Error = reason.isEmpty() ? std::string("studiomdl failed without saying why.") : reason.toStdString();
        return result;
    }

    result.Ok = true;
    result.ModelPath = QDir::toNativeSeparators(modelPath).toStdString();
#else
    (void)studiomdlPath;
    (void)qcPath;
    result.Error = "Compiling is only available in the GUI build.";
#endif
    return result;
}

void QCFile::AddBody(const std::string& name, const std::string& smdPath)
{
    m_bodies.push_back({ name, smdPath });
}

void QCFile::AddAttachment(int index, const std::string& bone, float x, float y, float z)
{
    QCAttachment attachment;
    attachment.Index = index;
    attachment.Bone = bone;
    attachment.X = x;
    attachment.Y = y;
    attachment.Z = z;
    m_attachments.push_back(attachment);
}

void QCFile::AddTextureRenderMode(const std::string& texture, const std::string& mode)
{
    m_renderModes.push_back({ texture, mode });
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

    if (m_bodies.empty())
    {
        file << "$bodygroup \"studio\"\n";
        file << "{\n";
        file << "\tstudio \"" << m_settings.MeshName << "\"\n";
        file << "}\n\n";
    }
    else
    {
        for (const auto& body : m_bodies)
        {
            file << "$body \"" << body.Name << "\" \"" << body.SmdPath << "\"\n";
        }
        file << "\n";
    }

    for (const auto& mode : m_renderModes)
    {
        file << "$texrendermode \"" << mode.Texture << "\" " << mode.Mode << "\n";
    }
    for (const auto& attachment : m_attachments)
    {
        file << "$attachment " << attachment.Index << " \"" << attachment.Bone << "\" "
             << attachment.X << " " << attachment.Y << " " << attachment.Z << "\n";
    }
    if (!m_renderModes.empty() || !m_attachments.empty())
    {
        file << "\n";
    }


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
