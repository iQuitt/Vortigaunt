// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#pragma once

#include <QImage>
#include <QList>
#include <QPair>
#include <QWidget>

#include <functional>
#include <vector>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QProgressBar;
class QPushButton;
class QSpinBox;
class TextureView;

// Textures tab: DTX, VTF, DDS and image files side by side, one of them large, saved for GoldSrc
class TextureWorkspace : public QWidget
{
    Q_OBJECT
public:
    // a texture and the layers it has; a VTF can hold more than one picture
    struct Decoded
    {
        QImage image;
        QString format;
        int frames = 1;
        int faces = 1;
        int mipmaps = 1;
        bool alphaIsGloss = false;  // DTX alpha is a gloss mask, not transparency
    };

    explicit TextureWorkspace(std::function<QString()> outputDir, QWidget* parent = nullptr);
    ~TextureWorkspace() override;

    static bool isTexture(const QString& path);
    // ext without the dot, lower case
    static Decoded decode(const QString& ext, const QByteArray& data, int frame = 0, int face = 0, int mipmap = 0);

    void addFiles(const QStringList& paths);
    // a texture that is not a file of its own, from an archive
    void addFromMemory(const QString& name, const QByteArray& data);
    bool isBusy() const { return m_busy; }

public slots:
    void openFilesDialog();
    void openFolderDialog();
    void removeSelected();
    void clear();
    // every texture the search shows
    void exportBmp();
    void exportPng();
    void exportSelectedBmp();
    // the selected textures, or the one shown, for the WAD tab
    void sendSelectedToWad();

signals:
    void sendToWad(const QList<QPair<QString, QImage>>& images);

private:
    struct Texture
    {
        QString name;
        QString path;       // empty for a texture from memory
        QString subfolder;  // kept in the output when a folder was added
        QByteArray data;    // only for a texture from memory
    };

    void addTexture(Texture texture);
    QByteArray read(const Texture& texture) const;
    void loadThumbnails();
    void applyFilter();
    void showSelected();
    void showLayer();
    void updateInfo();
    // the textures the search shows, or only the selected ones of them
    std::vector<int> shownTextures(bool selectedOnly) const;
    // to the output folder in the background
    void exportTextures(const std::vector<int>& indices, bool bmp);
    void setBusy(bool busy);

    std::function<QString()> m_outputDir;
    std::vector<Texture> m_textures;
    int m_current = -1;
    Decoded m_decoded;
    bool m_busy = false;
    int m_thumbnailGeneration = 0;

    QLineEdit*    m_searchEdit = nullptr;
    QListWidget*  m_list = nullptr;
    QLabel*       m_countLabel = nullptr;
    QProgressBar* m_progress = nullptr;
    TextureView*  m_view = nullptr;
    QComboBox*    m_channelCombo = nullptr;
    QLabel*       m_zoomLabel = nullptr;
    QSpinBox*     m_frameSpin = nullptr;
    QSpinBox*     m_faceSpin = nullptr;
    QSpinBox*     m_mipSpin = nullptr;
    QLabel*       m_frameLabel = nullptr;
    QLabel*       m_faceLabel = nullptr;
    QLabel*       m_mipLabel = nullptr;
    QLabel*       m_infoLabel = nullptr;
    QCheckBox*    m_maskedCheck = nullptr;
    QCheckBox*    m_powerOfTwoCheck = nullptr;
    QPushButton*  m_saveBmpButton = nullptr;
    QPushButton*  m_savePngButton = nullptr;
    QPushButton*  m_exportButton = nullptr;
    QPushButton*  m_wadButton = nullptr;
};
