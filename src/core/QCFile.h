#pragma once

#include <string>
#include <vector>
#include <fstream>

struct QCFileSettings
{
    std::string ModelName;         
    std::string MeshName;          
    float Scale = 1.0f;           
    int Fps = 30;               
};


struct QCSequence
{
    std::string Name;               
    std::string SmdPath;           
    int Fps = 30;              
};

/**
 * @brief Generates Valve QC (StudioMdl Compile) files for model compilation
 */
class QCFile
{
public:
    QCFile() = default;
    ~QCFile() = default;

    // Set QC file settings
    void SetSettings(const QCFileSettings& settings) { m_settings = settings; }

    /*
	*   @brief Add an animation sequence
    */
    void AddSequence(const std::string& name, const std::string& smdPath, int fps = 30);

    /*
	*   @brief Get number of sequences added
    */
    size_t GetSequenceCount() const { return m_sequences.size(); }

    /*
	*   @brief Write QC file to disk
    */
    bool Write(const std::string& outputPath);

    const std::string& GetError() const { return m_error; }

private:
    QCFileSettings m_settings;
    std::vector<QCSequence> m_sequences;
    std::string m_error;

};
