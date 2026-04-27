#pragma once

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QGroupBox>

#include <memory>
#include <vector>

class RezExtractor;

class RezViewerWindow : public QDialog
{
    Q_OBJECT

public:
    explicit RezViewerWindow(QWidget* parent = nullptr);
    ~RezViewerWindow() override;
    
    // Load REZ file directly (for command-line file association)
    bool loadRez(const QString& filePath);

private slots:
    void onOpenRez();
    void onExtractSelected();
    void onExtractAll();
    void onViewDtx();
    void onViewSpr();
    void onSelectionChanged();
    void onDoubleClick(int row, int column);
    void onSearchTextChanged(const QString& text);
    void onSelectAllMatches();

private:
    void setupUI();
    void populateFileList();
    void extractEntries(const std::vector<size_t>& indices, const QString& outputDir);
    void viewDtxEntry(size_t index);
    void viewSprEntry(size_t index);
    void filterTable(const QString& searchText);
    QString getFileTypeInfo(const QString& path) const;
    QString formatSize(uint32_t size) const;
    bool canViewAsDtx(const QString& path) const;
    bool canViewAsSpr(const QString& path) const;

    // UI Elements
    QLineEdit*      m_rezPathEdit;
    QPushButton*    m_openButton;
    QLineEdit*      m_searchEdit;
    QPushButton*    m_selectAllMatchesButton;
    QTableWidget*   m_fileTable;
    QPushButton*    m_extractSelectedButton;
    QPushButton*    m_extractAllButton;
    QPushButton*    m_viewDtxButton;
    QPushButton*    m_viewSprButton;
    QLabel*         m_statusLabel;
    QProgressBar*   m_progressBar;
    
    // Preview panel
    QGroupBox*      m_previewGroup;
    QLabel*         m_previewLabel;
    QLabel*         m_previewInfoLabel;

    // Data
    std::unique_ptr<RezExtractor> m_rezExtractor;
    QString m_currentRezPath;
};
