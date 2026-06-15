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

class VpkViewerWindow : public QDialog
{
    Q_OBJECT

public:
    explicit VpkViewerWindow(QWidget* parent = nullptr);
    ~VpkViewerWindow() override;

    bool loadVpk(const QString& filePath);

private slots:
    void onOpenVpk();
    void onExtractSelected();
    void onExtractAll();
    void onSelectionChanged();
    void onSearchTextChanged(const QString& text);
    void onSelectAllMatches();

private:
    void setupUI();
    void populateFileList();
    void extractEntries(const std::vector<size_t>& indices, const QString& outputDir);
    void filterTable(const QString& searchText);
    QString getFileTypeInfo(const QString& path) const;
    QString formatSize(uint32_t size) const;

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
    QString m_currentVpkPath;
};
