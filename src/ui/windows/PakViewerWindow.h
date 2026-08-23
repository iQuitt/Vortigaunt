#pragma once

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>

#include <memory>
#include <vector>

class PakFile;

class PakViewerWindow : public QDialog
{
    Q_OBJECT

public:
    explicit PakViewerWindow(QWidget* parent = nullptr);
    ~PakViewerWindow() override;
    
    // Load PAK file directly (for command-line file association)
    bool loadPak(const QString& filePath);

private slots:
    void onOpenPak();
    void onExtractSelected();
    void onExtractAll();
    void onOpenInViewer();
    void onSelectionChanged();
    void onDoubleClick(int row, int column);
    void onSearchTextChanged(const QString& text);
    void onSelectAllMatches();

private:
    void setupUI();
    void populateFileList();
    void extractEntries(const std::vector<size_t>& indices, const QString& outputDir);
    void openEntryInViewer(size_t index);
    void filterTable(const QString& searchText);
    QString getFileTypeInfo(const QString& path) const;
    bool canOpenInViewer(const QString& path) const;

    // UI Elements
    QLineEdit*      m_pakPathEdit;
    QPushButton*    m_openButton;
    QLineEdit*      m_searchEdit;
    QPushButton*    m_selectAllMatchesButton;
    QTableWidget*   m_fileTable;
    QPushButton*    m_extractSelectedButton;
    QPushButton*    m_extractAllButton;
    QPushButton*    m_openInViewerButton;
    QLabel*         m_statusLabel;
    QProgressBar*   m_progressBar;

    // Data
    std::unique_ptr<PakFile> m_pakFile;
    std::vector<uint8_t> m_pakBuffer;
    QString m_currentPakPath;
};
