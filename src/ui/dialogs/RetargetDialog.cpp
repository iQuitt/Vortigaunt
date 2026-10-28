// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#include "RetargetDialog.h"
#include "SettingsDialog.h"
#include "core/QCFile.h"
#include "core/VortigauntLog.h"
#include "core/autorig/Retarget.h"
#include "ui/widgets/QcPreview.h"

#include <QCheckBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QSettings>
#include <QSplitter>
#include <QUrl>
#include <QVBoxLayout>
#include <thread>

namespace
{
const QString kCharacterKey = QStringLiteral("Retarget/CharacterPath");
const QString kQcKey = QStringLiteral("Retarget/QcPath");

QSettings settings()
{
    return QSettings(QStringLiteral("Vortigaunt"), QStringLiteral("VortigauntTool"));
}

QHBoxLayout* browseRow(QLineEdit* edit, QPushButton* button)
{
    auto* row = new QHBoxLayout();
    row->addWidget(edit, 1);
    row->addWidget(button);
    return row;
}
}

RetargetDialog::RetargetDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Animation Retargeting"));
    auto* outer = new QHBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    outer->addWidget(splitter);
    auto* leftScroll = new QScrollArea(splitter);
    leftScroll->setWidgetResizable(true);
    leftScroll->setFrameShape(QFrame::NoFrame);
    leftScroll->setMinimumWidth(420);
    auto* left = new QWidget();
    leftScroll->setWidget(left);
    auto* layout = new QVBoxLayout(left);

    auto* intro = new QLabel(tr("Plays motions on the skeleton of a reference model: each motion becomes an SMD beside the "
                                "model's QC and a sequence in it. The bones keep the model's lengths and the walk is sized by the legs. "
                                "Sources can be .gr2, .fbx, .glb, .smd or .mdl."));
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto* sourceGroup = new QGroupBox(tr("Source"));
    auto* sourceForm = new QFormLayout(sourceGroup);
    m_characterEdit = new QLineEdit(QDir::toNativeSeparators(settings().value(kCharacterKey).toString()));
    m_characterEdit->setPlaceholderText(tr("The character the motions are of, with its skeleton: .gr2, .fbx, .glb, .smd or .mdl"));
    auto* characterBrowse = new QPushButton(tr("Browse..."));
    sourceForm->addRow(tr("Character:"), browseRow(m_characterEdit, characterBrowse));
    m_motionList = new QListWidget();
    m_motionList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_motionList->setToolTip(tr("Left empty, the character's own animations are used"));
    auto* motionButtons = new QVBoxLayout();
    auto* addMotions = new QPushButton(tr("Add..."));
    auto* removeMotions = new QPushButton(tr("Remove"));
    motionButtons->addWidget(addMotions);
    motionButtons->addWidget(removeMotions);
    motionButtons->addStretch();
    auto* motionRow = new QHBoxLayout();
    motionRow->addWidget(m_motionList, 1);
    motionRow->addLayout(motionButtons);
    sourceForm->addRow(tr("Motions:"), motionRow);
    layout->addWidget(sourceGroup);

    auto* targetGroup = new QGroupBox(tr("Reference model"));
    auto* targetForm = new QFormLayout(targetGroup);
    m_qcEdit = new QLineEdit(QDir::toNativeSeparators(settings().value(kQcKey).toString()));
    m_qcEdit->setPlaceholderText(tr("The decompiled QC of the model; its first body is the skeleton"));
    auto* qcBrowse = new QPushButton(tr("Browse..."));
    targetForm->addRow(tr("QC:"), browseRow(m_qcEdit, qcBrowse));
    m_animsEdit = new QLineEdit();
    m_animsEdit->setPlaceholderText(tr("<QC name>_anims"));
    m_animsEdit->setToolTip(tr("The folder beside the QC the SMDs of the motions go to"));
    targetForm->addRow(tr("Animations folder:"), m_animsEdit);
    m_loopCheck = new QCheckBox(tr("Sequences loop"));
    m_loopCheck->setChecked(true);
    targetForm->addRow(m_loopCheck);
    layout->addWidget(targetGroup);

    m_compileGroup = new QGroupBox(tr("Compile the model"));
    m_compileGroup->setCheckable(true);
    m_compileGroup->setChecked(!SettingsDialog::getStudioMdlPath().isEmpty());
    auto* compileForm = new QFormLayout(m_compileGroup);
    m_studiomdlEdit = new QLineEdit(QDir::toNativeSeparators(SettingsDialog::getStudioMdlPath()));
    m_studiomdlEdit->setPlaceholderText(QStringLiteral("studiomdl.exe"));
    auto* studiomdlBrowse = new QPushButton(tr("Browse..."));
    compileForm->addRow(tr("studiomdl:"), browseRow(m_studiomdlEdit, studiomdlBrowse));
    layout->addWidget(m_compileGroup);

    m_report = new QPlainTextEdit();
    m_report->setReadOnly(true);
    m_report->setPlaceholderText(tr("What the retargeting made shows here"));
    layout->addWidget(m_report, 1);

    auto* buttons = new QHBoxLayout();
    buttons->addStretch();
    auto* openButton = new QPushButton(tr("Open folder"));
    m_retargetButton = new QPushButton(tr("Retarget"));
    buttons->addWidget(openButton);
    buttons->addWidget(m_retargetButton);
    layout->addLayout(buttons);

    // right: preview
    m_preview = new QcPreview(splitter);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    connect(m_preview, &QcPreview::report, m_report, &QPlainTextEdit::appendPlainText);
    connect(m_preview, &QcPreview::previewRequested, this, [this]() {
        m_preview->build(m_qcEdit->text().trimmed(), m_studiomdlEdit->text().trimmed(), QString(), m_preview->current());
    });

    auto browseFile = [this](QLineEdit* edit, const QString& title, const QString& filter) {
        const QString path = QFileDialog::getOpenFileName(this, title, QFileInfo(edit->text()).absolutePath(), filter);
        if (!path.isEmpty()) edit->setText(QDir::toNativeSeparators(path));
    };
    connect(characterBrowse, &QPushButton::clicked, this, [this, browseFile]() {
        browseFile(m_characterEdit, tr("Select the character"), tr("Models (*.gr2 *.fbx *.glb *.gltf *.smd *.mdl);;All files (*.*)"));
    });
    connect(addMotions, &QPushButton::clicked, this, [this]() {
        const QString from = m_motionList->count() ? QFileInfo(m_motionList->item(m_motionList->count() - 1)->text()).absolutePath()
                                                   : QFileInfo(m_characterEdit->text()).absolutePath();
        for (const QString& path : QFileDialog::getOpenFileNames(this, tr("Select the motions"), from,
                                                                 tr("Motions (*.gr2 *.fbx *.glb *.gltf *.smd *.mdl);;All files (*.*)")))
            if (m_motionList->findItems(QDir::toNativeSeparators(path), Qt::MatchExactly).isEmpty())
                m_motionList->addItem(QDir::toNativeSeparators(path));
    });
    connect(removeMotions, &QPushButton::clicked, this, [this]() { qDeleteAll(m_motionList->selectedItems()); });
    connect(qcBrowse, &QPushButton::clicked, this, [this, browseFile]() {
        browseFile(m_qcEdit, tr("Select the QC"), tr("QC files (*.qc);;All files (*.*)"));
    });
    connect(studiomdlBrowse, &QPushButton::clicked, this, [this, browseFile]() {
        browseFile(m_studiomdlEdit, tr("Select studiomdl"), tr("Programs (*.exe);;All files (*.*)"));
    });
    connect(openButton, &QPushButton::clicked, this, [this]() {
        const QFileInfo qc(m_qcEdit->text().trimmed());
        if (qc.exists()) QDesktopServices::openUrl(QUrl::fromLocalFile(qc.absolutePath()));
    });
    connect(m_retargetButton, &QPushButton::clicked, this, &RetargetDialog::onRetarget);
}

bool RetargetDialog::isBusy() const
{
    return m_busy || m_preview->isBusy();
}

void RetargetDialog::reject()
{
    if (!isBusy()) QDialog::reject();
}

void RetargetDialog::setBusy(bool busy)
{
    m_busy = busy;
    setProperty("vortigauntBusy", busy);
    m_retargetButton->setEnabled(!busy);
    m_retargetButton->setText(busy ? tr("Retargeting...") : tr("Retarget"));
}

void RetargetDialog::onRetarget()
{
    if (m_busy) return;
    const QString character = m_characterEdit->text().trimmed();
    const QString qc = m_qcEdit->text().trimmed();
    QStringList motions;
    for (int i = 0; i < m_motionList->count(); ++i) motions << m_motionList->item(i)->text();
    if (!QFileInfo::exists(character) || !QFileInfo::exists(qc))
    {
        m_report->setPlainText(tr("Give the character and the QC of the reference model."));
        return;
    }
    // the character's own animations
    if (motions.isEmpty()) motions << character;
    const QString body = QString::fromStdString(retarget::QcBody(qc.toStdString()));
    if (body.isEmpty() || !QFileInfo::exists(body))
    {
        m_report->setPlainText(tr("The QC has no body SMD to take the skeleton from."));
        return;
    }
    settings().setValue(kCharacterKey, QDir::fromNativeSeparators(character));
    settings().setValue(kQcKey, QDir::fromNativeSeparators(qc));
    QString anims = m_animsEdit->text().trimmed();
    if (anims.isEmpty()) anims = QFileInfo(qc).completeBaseName() + QStringLiteral("_anims");
    anims = QDir::fromNativeSeparators(anims);
    const QString animsFolder = QFileInfo(qc).dir().filePath(anims);
    QDir().mkpath(animsFolder);
    const bool loop = m_loopCheck->isChecked();
    const bool compile = m_compileGroup->isChecked();
    const QString studiomdl = m_studiomdlEdit->text().trimmed();
    if (compile && !studiomdl.isEmpty()) SettingsDialog::setStudioMdlPath(QDir::fromNativeSeparators(studiomdl));
    const QString work = QDir(QDir::tempPath()).filePath(QStringLiteral("vortigaunt_retarget"));

    m_report->setPlainText(tr("Retargeting the motions of %1 files...").arg(motions.size()));
    setBusy(true);
    std::thread([this, character, qc, body, motions, anims, animsFolder, loop, compile, studiomdl, work]() {
        // report line, and English in the log
        auto report = [this](const QString& shown, const QString& logged, const QString& colour = QString()) {
            QMetaObject::invokeMethod(this, [this, shown]() { m_report->appendPlainText(shown); }, Qt::QueuedConnection);
            VortigauntLog::Vortigaunt_Printf(colour.isEmpty() ? logged : colour + logged + QStringLiteral("^7"));
        };
        int retargeted = 0;
        QString first;
        std::string error;
        const std::string rest = retarget::SourceSkeleton(character.toStdString(), work.toStdString(), error);
        if (rest.empty())
        {
            const QString line = QString::fromStdString(error);
            report(line, line, QStringLiteral("^1"));
        }
        QSet<QString> taken;
        for (const QString& file : rest.empty() ? QStringList() : motions)
        {
            const std::vector<retarget::QcSequence> sequences = retarget::SourceMotions(file.toStdString(), character.toStdString(), work.toStdString(), error);
            if (sequences.empty())
            {
                const QString line = QStringLiteral("%1: %2").arg(QFileInfo(file).fileName(), QString::fromStdString(error));
                report(line, line, QStringLiteral("^1"));
            }
            for (const retarget::QcSequence& sequence : sequences)
            {
                // a name not used yet in this run
                QString name = QString::fromStdString(sequence.name);
                for (int n = 2; taken.contains(name.toLower()); ++n) name = QString::fromStdString(sequence.name) + QStringLiteral("_%1").arg(n);
                taken.insert(name.toLower());
                const retarget::RetargetResult r = retarget::Retarget(rest, sequence.path, body.toStdString(),
                                                                      QDir(animsFolder).filePath(name + QStringLiteral(".smd")).toStdString(), sequence.fps);
                if (!r.ok)
                {
                    const QString line = QStringLiteral("%1: %2").arg(name, QString::fromStdString(r.error));
                    report(line, line, QStringLiteral("^1"));
                    continue;
                }
                if (!retarget::AddSequence(qc.toStdString(), name.toStdString(), (anims + QStringLiteral("/") + name).toStdString(), r.fps,
                                           loop || sequence.loop, error, sequence.activity))
                {
                    const QString line = QStringLiteral("%1: %2").arg(name, QString::fromStdString(error));
                    report(line, line, QStringLiteral("^1"));
                    continue;
                }
                QString shown = tr("%1: %2 frames at %3 fps").arg(name).arg(r.frames).arg(r.fps);
                QString logged = QStringLiteral("%1: %2 frames at %3 fps").arg(name).arg(r.frames).arg(r.fps);
                if (!r.unmatched.empty())
                {
                    QStringList bones;
                    for (const std::string& bone : r.unmatched) bones << QString::fromStdString(bone);
                    shown += tr(", kept at rest: %1").arg(bones.join(QStringLiteral(", ")));
                    logged += QStringLiteral(", kept at rest: %1").arg(bones.join(QStringLiteral(", ")));
                }
                report(shown, logged);
                if (!retargeted) first = name;
                retargeted++;
            }
        }
        if (retargeted && compile)
        {
            report(tr("Compiling %1...").arg(QFileInfo(qc).fileName()), QStringLiteral("Compiling %1...").arg(QFileInfo(qc).fileName()));
            const QCCompileResult built = QCFile::Compile(studiomdl.toStdString(), qc.toStdString());
            const QString model = QDir::toNativeSeparators(QString::fromStdString(built.ModelPath));
            const QString error = QString::fromStdString(built.Error);
            if (built.Ok) report(tr("Model written: %1").arg(model), QStringLiteral("Model written: %1").arg(model), QStringLiteral("^2"));
            else report(tr("studiomdl failed: %1").arg(error), QStringLiteral("studiomdl failed: %1").arg(error), QStringLiteral("^1"));
        }
        QMetaObject::invokeMethod(this, [this, retargeted, first, qc, studiomdl]() {
            setBusy(false);
            m_report->appendPlainText(tr("%1 sequences in the QC.").arg(retargeted));
            if (retargeted && !studiomdl.isEmpty()) m_preview->build(qc, studiomdl, QString(), first);
        }, Qt::QueuedConnection);
    }).detach();
}
