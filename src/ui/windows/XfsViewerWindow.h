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

class XfsExtractor;

class XfsViewerWindow : public QDialog
{
    Q_OBJECT

public:
    explicit XfsViewerWindow(QWidget* parent = nullptr);
    ~XfsViewerWindow() override;
    
    // Load XFS file directly (for command-line file association)
    bool loadXfs(const QString& filePath);

private slots:
    void onOpenXfs();
    void onExtractSelected();
    void onExtractAll();
    void onViewSpr();
    void onSelectionChanged();
    void onDoubleClick(int row, int column);
    void onSearchTextChanged(const QString& text);
    void onSelectAllMatches();

private:
    void setupUI();
    void populateFileList();
    void extractEntries(const std::vector<size_t>& indices, const QString& outputDir);
    void viewSprEntry(size_t index);
    void filterTable(const QString& searchText);
    QString getFileTypeInfo(const QString& path) const;
    bool canViewAsSpr(const QString& path) const;

    // UI Elements
    QLineEdit*      m_xfsPathEdit;
    QPushButton*    m_openButton;
    QLineEdit*      m_searchEdit;
    QPushButton*    m_selectAllMatchesButton;
    QTableWidget*   m_fileTable;
    QPushButton*    m_viewSprButton;
    QPushButton*    m_extractSelectedButton;
    QPushButton*    m_extractAllButton;
    QLabel*         m_statusLabel;
    QLabel*         m_archiveInfoLabel;
    QProgressBar*   m_progressBar;

    // Data
    std::unique_ptr<XfsExtractor> m_xfsExtractor;
    QString m_currentXfsPath;
};
