#pragma once

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>

#include <memory>
#include <vector>

class VpkFile;
class GmaFile;

// Viewer for Source Engine archives: VPK and GMA (Garry's Mod Addon)
class VpkViewerWindow : public QDialog
{
    Q_OBJECT

public:
    explicit VpkViewerWindow(QWidget* parent = nullptr);
    ~VpkViewerWindow() override;

    bool loadVpk(const QString& filePath); // handles both .vpk and .gma

private slots:
    void onOpenVpk();
    void onExtractSelected();
    void onExtractAll();
    void onSelectionChanged();
    void onSearchTextChanged(const QString& text);
    void onSelectAllMatches();

private:
    void setupUI();
    bool loadVpkFile(const QString& filePath);
    bool loadGmaFile(const QString& filePath);
    void populateFileList();
    void extractEntries(const std::vector<size_t>& indices, const QString& outputDir);
    void filterTable(const QString& searchText);
    bool hasArchive() const;
    QString getFileTypeInfo(const QString& path) const;
    QString formatSize(quint64 size) const;

    QLineEdit*      m_vpkPathEdit;
    QPushButton*    m_openButton;
    QLineEdit*      m_searchEdit;
    QPushButton*    m_selectAllMatchesButton;
    QTableWidget*   m_fileTable;
    QPushButton*    m_extractSelectedButton;
    QPushButton*    m_extractAllButton;
    QLabel*         m_statusLabel;
    QProgressBar*   m_progressBar;

    std::unique_ptr<VpkFile> m_vpkFile;
    std::unique_ptr<GmaFile> m_gmaFile;
    QString m_currentVpkPath;
};
