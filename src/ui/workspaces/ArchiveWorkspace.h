// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#pragma once

#include <QImage>
#include <QList>
#include <QPair>
#include <QWidget>

#include <functional>
#include <memory>
#include <vector>

class ArchiveSource;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QStackedWidget;
class QTreeWidget;
class QTreeWidgetItem;

// Archives tab: the opened archives and their folders, the files in them, and a preview of the selected one
class ArchiveWorkspace : public QWidget
{
    Q_OBJECT
public:
    explicit ArchiveWorkspace(std::function<QString()> outputDir, QWidget* parent = nullptr);
    ~ArchiveWorkspace() override;

    bool openArchive(const QString& path);
    bool isBusy() const { return m_busy; }

public slots:
    void openArchiveDialog();
    void closeCurrentArchive();
    void extractSelected();
    void extractAll();
    // extracts the selected files and hands them to the Convert list
    void sendSelectedToConvert();
    // the selected textures, decoded, for the WAD tab
    void sendSelectedToWad();

signals:
    void addToConvert(const QStringList& paths);
    void openTexture(const QString& name, const QByteArray& data);
    void openSprite(const QString& path);
    void sendToWad(const QList<QPair<QString, QImage>>& images);

private:
    void showArchive(int index);
    void buildFolderTree();
    void applyFilter();
    void updatePreview();
    void openSelectedInViewer();
    std::vector<size_t> selectedEntries() const;
    // in the background; done gets the written files, on the UI thread
    void extract(const std::vector<size_t>& indices, const std::function<void(const QStringList&)>& done);
    void setBusy(bool busy);
    ArchiveSource* current() const;

    std::function<QString()> m_outputDir;
    std::vector<std::unique_ptr<ArchiveSource>> m_archives;
    bool m_busy = false;

    QListWidget*    m_archiveList = nullptr;
    QTreeWidget*    m_folderTree = nullptr;
    QLineEdit*      m_searchEdit = nullptr;
    QComboBox*      m_typeFilter = nullptr;
    QTreeWidget*    m_entryList = nullptr;
    QLabel*         m_countLabel = nullptr;
    QProgressBar*   m_progress = nullptr;
    QStackedWidget* m_previewStack = nullptr;
    QLabel*         m_previewImage = nullptr;
    QPlainTextEdit* m_previewText = nullptr;
    QLabel*         m_previewNote = nullptr;
    QLabel*         m_infoLabel = nullptr;
    QPushButton*    m_openButton = nullptr;
    QPushButton*    m_extractButton = nullptr;
    QPushButton*    m_convertButton = nullptr;
    QPushButton*    m_wadButton = nullptr;
};
