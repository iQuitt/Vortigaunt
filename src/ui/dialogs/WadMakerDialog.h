#pragma once

#include <QDialog>
#include <QListWidget>
#include <QString>
#include <QStringList>

class QLineEdit;
class QPushButton;
class QLabel;
class QSplitter;
class WadArchive;

/**
 * @brief Enhanced WAD Editor dialog with thumbnail grid view
 * 
 * Features:
 * - Open/Edit existing WAD files
 * - Create new WAD from images
 * - Thumbnail grid view like Hammer Editor
 * - Preview panel for selected texture
 * - Rename textures inline
 * - GoldSrc limit compliance (15 char names, 512 max size, 4096 textures)
 */
class WadMakerDialog : public QDialog
{
    Q_OBJECT
public:
    explicit WadMakerDialog(QWidget* parent = nullptr);
    ~WadMakerDialog();
    
    // Load WAD file directly (for command-line file association)
    bool loadWad(const QString& filePath);

private slots:
    // File operations
    void onNewWad();
    void onOpenWad();
    void onSaveWad();
    void onSaveWadAs();

    // Texture operations  
    void onAddImages();
    void onRemoveSelected();
    void onExtractSelected();
    void onRenameSelected();

    // UI updates
    void onSelectionChanged();
    void onFilterChanged(const QString& text);
    void onItemDoubleClicked(QListWidgetItem* item);

private:
    void setupUi();
    void refreshTextureList();
    void updateTextureCount();
    void updatePreview(int index);
    void populateThumbnails();
    bool maybeSave(); // Check if user wants to save before closing

    // UI elements
    QListWidget* m_textureGrid;
    QLabel* m_previewLabel;
    QLabel* m_previewInfo;
    QLabel* m_statusLabel;
    QLineEdit* m_filterEdit;
    
    // Toolbar buttons
    QPushButton* m_newButton;
    QPushButton* m_openButton;
    QPushButton* m_saveButton;
    QPushButton* m_saveAsButton;
    
    // Texture buttons
    QPushButton* m_addButton;
    QPushButton* m_removeButton;
    QPushButton* m_extractButton;
    QPushButton* m_renameButton;
    
    // WAD data
    WadArchive* m_wadMaker;
    QString m_currentFilePath;
    bool m_modified;
    
    // Constants
    static constexpr int THUMBNAIL_SIZE = 64;
    static constexpr int MAX_TEXTURES = 4096;
    static constexpr int MAX_NAME_LENGTH = 15;
};
