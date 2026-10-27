// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

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


struct QCBody
{
    std::string Name;
    std::string SmdPath;
};

struct QCAttachment
{
    int Index = 0;
    std::string Bone;
    float X = 0.0f, Y = 0.0f, Z = 0.0f;
};

struct QCTextureRenderMode
{
    std::string Texture;
    std::string Mode;  // masked, additive
};

struct QCCompileResult
{
    bool Ok = false;
    std::string ModelPath;  // the .mdl the QC names, when it was written
    std::string Output;     // everything the compiler printed
    std::string Error;      // why it failed, in a sentence
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
	*   @brief Add a body. With none added the model has the single body named by MeshName
    */
    void AddBody(const std::string& name, const std::string& smdPath);

    /*
	*   @brief Add an attachment point on a bone
    */
    void AddAttachment(int index, const std::string& bone, float x, float y, float z);

    /*
	*   @brief Set how a texture is drawn ($texrendermode): masked, additive
    */
    void AddTextureRenderMode(const std::string& texture, const std::string& mode);

    /*
	*   @brief Get number of sequences added
    */
    size_t GetSequenceCount() const { return m_sequences.size(); }

    /*
	*   @brief Write QC file to disk
    */
    bool Write(const std::string& outputPath);

    const std::string& GetError() const { return m_error; }

    /*
	*   @brief Compile a QC with studiomdl, run in the QC's own folder
	*   Only the GUI build can do this; the command line build reports that it cannot.
    */
    static QCCompileResult Compile(const std::string& studiomdlPath, const std::string& qcPath);

private:
    QCFileSettings m_settings;
    std::vector<QCSequence> m_sequences;
    std::vector<QCBody> m_bodies;
    std::vector<QCAttachment> m_attachments;
    std::vector<QCTextureRenderMode> m_renderModes;
    std::string m_error;

};
