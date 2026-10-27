// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#pragma once

#include <QDialog>
#include <QImage>
#include <QList>
#include <QListWidget>
#include <QPair>
#include <QString>
#include <QStringList>

#include <vector>

class QAction;
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QSlider;
class QSpinBox;
class QTimer;
class QToolButton;
class PaletteWidget;
class TextureView;
class WadArchive;

// WAD tab: the textures as a grid, the selected one large, and what GoldSrc makes of its name and size
class WadMakerDialog : public QDialog
{
    Q_OBJECT
public:
    explicit WadMakerDialog(QWidget* parent = nullptr);
    ~WadMakerDialog();

    // Load WAD file directly (for command-line file association)
    bool loadWad(const QString& filePath);
    bool isModified() const { return m_modified; }

    // image files, or pictures from another tab with the name they should get; returns how many went in
    int addImages(const QStringList& paths);
    int addImages(const QList<QPair<QString, QImage>>& images);

    // the commands, for the ribbon; enabled while they can be used
    QAction* newAction() const { return m_newAction; }
    QAction* openAction() const { return m_openAction; }
    QAction* saveAction() const { return m_saveAction; }
    QAction* saveAsAction() const { return m_saveAsAction; }
    QAction* addImagesAction() const { return m_addAction; }
    QAction* removeAction() const { return m_removeAction; }
    QAction* extractAction() const { return m_extractAction; }
    QAction* renameAction() const { return m_renameAction; }
    QAction* replaceAction() const { return m_replaceAction; }

signals:
    void modifiedChanged(bool modified);

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
    void onReplaceSelected();

    // UI updates
    void onSelectionChanged();
    void onFilterChanged(const QString& text);
    void onItemDoubleClicked(QListWidgetItem* item);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    // what a GoldSrc texture name starts with
    enum class Kind
    {
        Normal,
        Masked,    // {
        Water,     // !
        Animated,  // +0 .. +9
        Toggle,    // +a .. +j
        Light,     // ~
    };

    // a problem GoldSrc would have with a texture
    struct Warning
    {
        int index;
        QString text;
    };

    void setupUi();
    // keeps the texture at index selected, -1 for none
    void refreshTextureList(int keepSelected = -1);
    void updateSummary();
    void updatePreview(int index);
    void populateThumbnails();
    bool maybeSave(); // Check if user wants to save before closing
    void setModified(bool modified);
    void updateTitle();
    int currentIndex() const;
    void selectTexture(int index);
    // renames to the name field with the type's prefix; says why when that name cannot be used
    void applyName();
    std::vector<Warning> findWarnings() const;
    // the result of an add: how many, which failed, which names were shortened or got the { prefix
    void reportAdded(int added, const QStringList& failed, const QStringList& renamed);
    void applyFilter();
    // the WAD's texture indices in the order the sort asks for
    std::vector<int> sortedIndices() const;
    // the textures of the same animation as index, in frame order
    std::vector<int> animationFrames(int index) const;

    static Kind kindOf(const QString& name, int* frame = nullptr);
    static QString prefixOf(Kind kind, int frame);
    static qint64 diskSizeOf(int width, int height);

    // UI elements
    QListWidget* m_textureGrid;
    QLineEdit* m_filterEdit;
    QButtonGroup* m_kindFilter = nullptr;
    QComboBox* m_sortCombo = nullptr;
    QComboBox* m_fitCombo = nullptr;
    QSlider* m_sizeSlider = nullptr;
    QToolButton* m_listViewButton = nullptr;
    int m_thumbnailSize = 64;
    TextureView* m_view = nullptr;
    QLabel* m_zoomLabel = nullptr;
    QCheckBox* m_animateCheck = nullptr;
    QTimer* m_animationTimer = nullptr;
    std::vector<int> m_animation;
    size_t m_animationFrame = 0;

    QLabel* m_titleLabel = nullptr;
    QLineEdit* m_nameEdit = nullptr;
    QLabel* m_nameCounter = nullptr;
    QLabel* m_nameProblem = nullptr;
    QComboBox* m_kindCombo = nullptr;
    QSpinBox* m_frameSpin = nullptr;
    QLabel* m_frameLabel = nullptr;
    QLabel* m_sizeLabel = nullptr;
    PaletteWidget* m_palette = nullptr;
    QLabel* m_paletteInfo = nullptr;
    QLabel* m_mipLabels[4] = {};
    QWidget* m_propertiesPanel = nullptr;
    QLabel* m_noSelectionLabel = nullptr;

    QLabel* m_statusLabel;
    QToolButton* m_warningsButton = nullptr;

    // Commands, shown in the ribbon
    QAction* m_newAction = nullptr;
    QAction* m_openAction = nullptr;
    QAction* m_saveAction = nullptr;
    QAction* m_saveAsAction = nullptr;
    QAction* m_addAction = nullptr;
    QAction* m_removeAction = nullptr;
    QAction* m_extractAction = nullptr;
    QAction* m_renameAction = nullptr;
    QAction* m_replaceAction = nullptr;

    // WAD data
    WadArchive* m_wadMaker;
    QString m_currentFilePath;
    bool m_modified;
    bool m_updatingProperties = false;

    // Constants
    static constexpr int THUMBNAIL_SIZE = 64;
    static constexpr int MAX_TEXTURES = 4096;
    static constexpr int MAX_NAME_LENGTH = 15;
};
