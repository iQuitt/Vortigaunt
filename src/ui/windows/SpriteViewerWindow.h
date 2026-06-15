#pragma once

#include <QDialog>
#include <QTabWidget>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QScrollArea>
#include <QTimer>
#include <QImage>
#include <QPixmap>
#include <QSlider>
#include <QProgressBar>
#include <QMenu>
#include <QTreeView>
#include <QFileSystemModel>
#include <QSplitter>
#include <stack>

#include "SpriteLoader.h"

// Custom widget for zoomable preview with background toggle
class ZoomablePreviewWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ZoomablePreviewWidget(QWidget* parent = nullptr);
    
    void setImage(const QImage& image);
    void clearImage();
    void resetZoom();
    double zoomFactor() const { return m_zoomFactor; }
    
signals:
    void zoomChanged(double factor);
    void backgroundChanged(int bgType);
    void coordinatesChanged(int x, int y, bool valid);
    void selectionChanged(int x, int y, int width, int height, bool valid);
    
protected:
    void paintEvent(QPaintEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    
private:
    void drawCheckeredBackground(QPainter& painter, const QRect& rect);
    
    QPoint widgetToImage(const QPoint& widgetPos) const;
    
    QImage m_image;
    double m_zoomFactor = 1.0;
    int m_backgroundType = 0; // 0=checkered, 1=black, 2=white, 3=gray
    
    // Rectangle selection
    bool m_isSelecting = false;
    QPoint m_selectionStart;  // Image coordinates
    QPoint m_selectionEnd;    // Image coordinates
    QRect m_selectionRect;    // Normalized selection in image coordinates
};

// Widget for displaying color palette
class PaletteWidget : public QWidget
{
    Q_OBJECT
public:
    explicit PaletteWidget(QWidget* parent = nullptr);
    
    void setPalette(const QVector<QRgb>& colors);
    void clearPalette();
    
signals:
    void colorClicked(int index, QRgb color);
    void colorDoubleClicked(int index, QRgb color);
    
protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    
private:
    QVector<QRgb> m_colors;
    int m_cellSize = 16;
    int m_hoveredIndex = -1;
};

class SpriteViewerWindow : public QDialog
{
    Q_OBJECT

public:
    explicit SpriteViewerWindow(QWidget* parent = nullptr);
    ~SpriteViewerWindow() override = default;
    
    // Public method to load sprite (for command-line support)
    void openSprite(const QString& filePath);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    // File browser
    void onFileBrowserCurrentChanged(const QModelIndex& current, const QModelIndex& previous);
    void onFileBrowserDoubleClicked(const QModelIndex& index);
    void onNavigateBack();
    void onNavigateForward();
    void onNavigateUp();
    void onPathEditReturnPressed();
    
    // File tab
    void onOpenSprite();
    void onQuickSave();      // Ctrl+S - save to current file
    void onSaveAsSprite();   // Save As - choose new location
    void onExtractFrame();
    void onExtractAllFrames();
    void onFixSprite();
    
    // Create tab
    void onBrowseFrames();
    void onCreateSprite();
    
    // Sprite Fix tab
    void onAddSpritesToFix();
    void onRemoveSelectedSprites();
    void onFixAllSprites();
    
    // Animation
    void onAnimationTimer();
    void onFileFrameListSelectionChanged();
    void onFilePlayAnimation();
    void onFileStopAnimation();
    void onNextSprite();
    void onPrevSprite();
    
    // Sprite properties
    void onSpriteTypeChanged(int index);
    void onTextureFormatChanged(int index);
    
    // Preview
    void onZoomChanged(double factor);
    void onBackgroundChanged(int bgType);
    void onCoordinatesChanged(int x, int y, bool valid);
    void onSelectionChanged(int x, int y, int width, int height, bool valid);
    void onPaletteColorClicked(int index, QRgb color);
    void onPaletteColorDoubleClicked(int index, QRgb color);
    
    // Lithtech-specific
    void onBrowseLithtechTextureDir();
    void onLoadLithtechTextures();
    void onExportLithtechToGoldSrc();
    void onExportLithtechFrames();
    
    // Palette manipulation
    void onPaletteBrightnessUp();
    void onPaletteBrightnessDown();
    void onPaletteGrayscale();
    void onPaletteSepia();
    void onPaletteContrast();
    void onPaletteInvert();
    void onPaletteSwapRB();
    void onPaletteSwapRG();
    void onPaletteSwapBG();
    void onPaletteUndo();
    void onPaletteRedo();
    void onPaletteReset();
    void onHueChanged(int value);
    void onSaturationChanged(int value);

private:
    void setupUI();
    QWidget* createFileBrowserWidget();
    QWidget* createFileTabWidget();
    QWidget* createCreateTabWidget();
    QWidget* createFixTabWidget();
    
    void loadSprite(const QString& filePath);
    void updateFileFrameList();
    void updateFileFrameDisplay();
    void updateFileInfo();
    void updatePaletteDisplay();
    void addSpritesToFixList(const QStringList& files);
    void navigateToPath(const QString& path);
    void applyHueSaturation();
    void loadNextPrevSprite(bool next);
    QString getSpriteTypeName(int type) const;
    QString getTextureFormatName(int format) const;
    
    // File Browser
    QFileSystemModel* m_fileBrowserModel;
    QTreeView* m_fileBrowser;
    QLineEdit* m_pathEdit;
    QPushButton* m_backButton;
    QPushButton* m_forwardButton;
    QPushButton* m_upButton;
    QStringList m_navigationHistory;
    int m_historyIndex = -1;
    
    // UI Elements - File Tab
    QLineEdit* m_fileSpritePathEdit;
    QPushButton* m_fileOpenButton;
    QPushButton* m_saveButton;        // Quick save (Ctrl+S)
    QPushButton* m_saveAsButton;      // Save As
    QPushButton* m_extractFrameButton;
    QPushButton* m_extractAllFramesButton;
    QPushButton* m_fixSpriteButton;
    QLabel* m_fileInfoLabel;
    QListWidget* m_fileFrameListWidget;
    ZoomablePreviewWidget* m_previewWidget;
    QScrollArea* m_fileFrameScrollArea;
    QPushButton* m_filePlayButton;
    QPushButton* m_fileStopButton;
    QPushButton* m_prevSpriteButton;
    QPushButton* m_nextSpriteButton;
    QSpinBox* m_fileAnimationSpeedSpin;
    QLabel* m_zoomLabel;
    QComboBox* m_spriteTypeEditCombo;
    QComboBox* m_textureFormatEditCombo;
    QLabel* m_spriteTypeLabel;
    QLabel* m_textureFormatLabel;
    QPushButton* m_resetZoomButton;
    QLabel* m_bgLabel;
    QLabel* m_coordLabel;  // X, Y coordinates display
    QLabel* m_selectionLabel;  // Selection rectangle info (x, y, w, h)
    PaletteWidget* m_paletteWidget;
    QLabel* m_paletteInfoLabel;
    QPushButton* m_paletteBrightnessUp;
    QPushButton* m_paletteBrightnessDown;
    QPushButton* m_paletteGrayscale;
    QPushButton* m_paletteSepia;
    QPushButton* m_paletteContrast;  // Opens contrast slider popup
    QPushButton* m_paletteInvert;
    QPushButton* m_paletteSwap;
    QMenu* m_swapMenu;
    QPushButton* m_paletteUndo;
    QPushButton* m_paletteRedo;
    QPushButton* m_paletteReset;
    QSlider* m_hueSlider;
    QSlider* m_saturationSlider;
    QLabel* m_hueLabel;
    QLabel* m_satLabel;
    // Undo/redo state (palette + properties)
    struct SpriteUndoState {
        std::vector<uint8_t> palette;
        int32_t type;
        int32_t texture_format;
    };
    std::stack<SpriteUndoState> m_undoStack;
    std::stack<SpriteUndoState> m_redoStack;
    std::vector<uint8_t> m_originalPalette; // For reset and H/S calculations
    int32_t m_originalType = 0;
    int32_t m_originalTextureFormat = 0;
    void saveUndoState();
    void updateUndoRedoButtons();
    bool isOriginalState() const;
    
    // UI Elements - Create Tab
    QListWidget* m_createFrameList;
    QPushButton* m_browseFramesButton;
    QPushButton* m_createSpriteButton;
    QComboBox* m_spriteTypeCombo;
    QComboBox* m_textureFormatCombo;
    QLineEdit* m_createOutputPathEdit;
    QPushButton* m_browseOutputButton;
    QPushButton* m_createTransColorButton;
    QLabel* m_createTransColorPreview;
    QColor m_createTransColor;
    QSlider* m_createContrastSlider;
    QLabel* m_createContrastLabel;
    
    // UI Elements - Fix Tab
    QListWidget* m_fixSpriteList;
    QPushButton* m_addSpritesButton;
    QPushButton* m_removeSpritesButton;
    QPushButton* m_fixAllButton;
    QComboBox* m_fixSpriteTypeCombo;
    QComboBox* m_fixTextureFormatCombo;
    QSlider* m_alphaThresholdSlider;
    QLabel* m_alphaThresholdLabel;
    QPushButton* m_fixTransColorButton;
    QLabel* m_fixTransColorPreview;
    QColor m_fixTransColor;
    QSlider* m_fixContrastSlider;
    QLabel* m_fixContrastLabel;
    QLineEdit* m_fixOutputDirEdit;
    QPushButton* m_browseFixOutputButton;
    QProgressBar* m_fixProgressBar;
    
    // Animation
    QTimer* m_animationTimer;
    int m_fileCurrentFrameIndex;
    bool m_fileIsPlaying;
    
    // Data
    SpriteLoader m_spriteLoader;
    QString m_currentSpritePath;
    bool m_modified = false;
    
    void setModified(bool modified);
    void updateWindowTitle();
    
    // Lithtech-specific UI
    QWidget* m_lithtechPanel = nullptr;
    QLineEdit* m_lithtechTextureDirEdit = nullptr;
    QPushButton* m_lithtechBrowseDirButton = nullptr;
    QPushButton* m_lithtechLoadTexturesButton = nullptr;
    QPushButton* m_lithtechExportGoldSrcButton = nullptr;
    QPushButton* m_lithtechExportFramesButton = nullptr;
    
    // For legacy label 
    QLabel* m_fileFrameDisplayLabel;
};
