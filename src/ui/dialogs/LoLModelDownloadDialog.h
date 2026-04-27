#pragma once

// Required includes (used directly in header)
#include <QDialog>
#include <QWidget>
#include <QPixmap>
#include <QSet>
#include <QStringList>
#include <QMap>
#include <QList>
#include <QHBoxLayout>
#include <QLabel>


class QListWidget;
class QPushButton;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPlainTextEdit;
class QComboBox;
class QScrollArea;
class QGridLayout;
class QHBoxLayout;
class QStackedWidget;

// for get the models from khada CDN
class QNetworkAccessManager;
class QNetworkReply;

// render parts of the model
class QEnterEvent;
class QMouseEvent;
class QPaintEvent;

class GLBViewer;

struct ChampionModel {
    QString id;
    QString name;
};

struct SkinModel {
    QString id;
    QString name;
};

class SkinCardWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SkinCardWidget(const QString& skinName, const QString& skinId, QWidget* parent = nullptr);
    void setSplashArt(const QPixmap& pixmap);
    void setSelected(bool selected);
    bool isSelected() const { return m_selected; }
    QString skinId() const { return m_skinId; }
    QString skinName() const { return m_skinName; }

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QString m_skinName;
    QString m_skinId;
    QPixmap m_splashArt;
    bool m_selected;
    bool m_hovered;
    QLabel* m_imageLabel;
    QLabel* m_nameLabel;
};

class LoLModelDownloadDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoLModelDownloadDialog(QWidget* parent = nullptr);
    ~LoLModelDownloadDialog() override;

private slots:
    void onChampionSelected();
    void onSkinSelected();
    void onSkinCardClicked();
    void onViewCharacterClicked();
    void onDownloadClicked();
    void onCancelClicked();
    void onSkinListLoaded();
    void onSplashArtLoaded();

private:
    void setupUI();
    void loadChampionList();
    void loadSkinList(const QString& championName);
    void loadSplashArt(const QString& skinId);
    void loadSplashArtWebP(const QString& skinId);
    void downloadModel(const QString& championName, const QString& modelId, const QString& modelName, bool autoFix = true, bool splitPrimitives = true, bool downloadChromas = true);
    void updateStatus(const QString& message);
    void logToDialog(const QString& message);
    QString formatSkinDisplayName(const QString& skinName, const QString& modelId);
    void updateDownloadButton();
    void openGLBViewer(const QString& skinId);
    void openGLBViewerWithFile(const QString& skinId, const QString& filePath);
    void startAsyncChromaDetection(const QString& skinId);
    
    // UI Elements
    QComboBox*          m_championCombo;
    QListWidget*        m_skinListWidget;  // Fallback için tutuluyor
    QScrollArea*        m_skinScrollArea;
    QWidget*            m_skinContainer;
    QGridLayout*        m_skinGridLayout;
    QStackedWidget*     m_contentStack;    // Stacked widget for skin cards and 3D viewer
    GLBViewer*    m_viewerWidget = nullptr;   // 3D viewer widget (lazy-created) 
    QPushButton*        m_backToSkinsButton; // Button to go back to skin cards
    QPushButton*        m_refreshSkinsButton;
    QPushButton*        m_viewCharacterButton;  // Button to view selected character
    QPushButton*        m_downloadButton;
    QPushButton*        m_cancelButton;
    QLineEdit*          m_outputPathEdit;
    QPushButton*        m_browseOutputButton;
    QPlainTextEdit*          m_logEdit;
    QProgressBar*       m_progressBar;
    QLabel*             m_statusLabel;
    
    // Lazy viewer creation support
    QHBoxLayout*        m_viewerHLayout = nullptr;
    QLabel*             m_viewerPlaceholder = nullptr;
    QComboBox*          m_animCombo = nullptr;
    QListWidget*        m_meshList = nullptr;
    QComboBox*          m_chromaCombo = nullptr;
    
    // Network
    QNetworkAccessManager* m_networkManager;
    QNetworkReply*         m_currentReply;
    QMap<QString, QNetworkReply*> m_splashArtReplies;  // Skin ID -> Reply mapping
    
    // Data
    QStringList           m_championList;
    QList<ChampionModel>  m_championModels;
    QList<SkinModel>      m_skinModels;
    QMap<QString, SkinCardWidget*> m_skinCards;  // Skin ID -> Card widget mapping
    QSet<QString>         m_selectedSkinIds;  // Selected skin IDs
    QString              m_currentChampion;
    QString              m_viewedSkinId;     // Currently viewed skin ID
    bool                 m_cancelled;
    QPlainTextEdit*      m_previousLogWidget;  // saved to restore on close
    
    // Async viewer download state
    QString              m_currentTempFile;    // Current temp file for cleanup
    QNetworkReply*       m_viewerDownloadReply = nullptr; // Active viewer download
    
    // Async chroma detection state
    QList<QNetworkReply*> m_chromaDetectionReplies;  // Active chroma HEAD requests
    QStringList          m_detectedChromaIds;        // Found chroma IDs
    int                  m_chromaDetectionPending = 0; // Pending chroma requests
};

