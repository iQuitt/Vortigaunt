// Copyright (C) 2026 iQuit (https://github.com/iQuitt)
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#pragma once

#include <QDialog>
#include <QStringList>

class QListWidget;
class QListWidgetItem;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;

// The Explorer context menu's work on the files it was given: decompile, compile or convert, with the outcome
class ShellActionDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ShellActionDialog(const QString& action, QWidget* parent = nullptr);

    void addFiles(const QStringList& files);
    bool isBusy() const { return m_busy; }

    // runs a context menu command; files of the same command from other Explorer launches join the first window
    static int run(const QString& action, const QStringList& files);

#ifdef Q_OS_WIN
    // the VortigauntTool submenu on .mdl, .qc, .fbx and .glb files, for this user
    static bool registerContextMenu();
    static void unregisterContextMenu();
    static bool isContextMenuRegistered();
#endif

public slots:
    void reject() override;

private:
    struct Outcome
    {
        bool ok = false;
        QString summary, details, folder, model;
    };

    void processNext();
    void finish(QListWidgetItem* item, const Outcome& outcome);
    void showItem();
    Outcome work(const QString& file, const QString& studiomdl) const;

    QString m_action;
    QStringList m_queue;
    bool m_busy = false;
    int m_overwrite = -1;  // the answer to an existing output folder, once asked

    QListWidget* m_list = nullptr;
    QPlainTextEdit* m_details = nullptr;
    QProgressBar* m_progress = nullptr;
    QPushButton* m_openButton = nullptr;
    QPushButton* m_viewButton = nullptr;
};
