#include "QcPreview.h"
#include "StudioModelView.h"
#include "core/QCFile.h"
#include "core/mdl/StudioModel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <thread>


namespace
{
const QString kPlayerKey = QStringLiteral("Retarget/PlayerModel");
const QString kPreviewName = QStringLiteral("vortigaunt_preview");

QString plain(const QString& name)
{
    return name.section(QStringLiteral("  ("), 0, 0);
}
}

QcPreview::QcPreview(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    auto* playerRow = new QHBoxLayout();
    m_playerEdit = new QLineEdit(QDir::toNativeSeparators(QSettings(QStringLiteral("Vortigaunt"), QStringLiteral("VortigauntTool")).value(kPlayerKey).toString()));
    m_playerEdit->setPlaceholderText(tr("A player model to wear a reference model's sequences; none for a model of its own"));
    auto* playerBrowse = new QPushButton(tr("Player..."));
    playerRow->addWidget(m_playerEdit, 1);
    playerRow->addWidget(playerBrowse);
    layout->addLayout(playerRow);
    auto* sequenceRow = new QHBoxLayout();
    m_sequenceCombo = new QComboBox();
    m_compareCheck = new QCheckBox(tr("Beside its source"));
    m_compareCheck->setChecked(true);
    m_compareCheck->setToolTip(tr("A take plays beside the sequence it came of"));
    m_previewButton = new QPushButton(tr("Preview QC"));
    m_previewButton->setToolTip(tr("Compiles the QC to a model of its own and plays it"));
    sequenceRow->addWidget(m_sequenceCombo, 1);
    sequenceRow->addWidget(m_compareCheck);
    m_compareCheck->hide();
    sequenceRow->addWidget(m_previewButton);
    layout->addLayout(sequenceRow);
    m_view = new StudioModelView(this);
    m_view->placeholder = tr("Preview the QC to see its sequences.");
    m_view->setHoldUnlooped(true);
    layout->addWidget(m_view, 1);
    auto* timeRow = new QHBoxLayout();
    m_playCheck = new QCheckBox(tr("Play"));
    m_playCheck->setChecked(true);
    m_timeSlider = new QSlider(Qt::Horizontal);
    m_timeSlider->setRange(0, 1000);
    m_timeLabel = new QLabel();
    m_timeLabel->setMinimumWidth(260);
    timeRow->addWidget(m_playCheck);
    timeRow->addWidget(m_timeSlider, 1);
    timeRow->addWidget(m_timeLabel);
    layout->addLayout(timeRow);

    m_clock = new QTimer(this);
    m_clock->setInterval(40);
    connect(m_clock, &QTimer::timeout, this, &QcPreview::tick);
    m_clock->start();

    connect(playerBrowse, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(this, tr("Select a player model"), QFileInfo(m_playerEdit->text()).absolutePath(),
                                                          tr("Studio models (*.mdl);;All files (*.*)"));
        if (path.isEmpty()) return;
        m_playerEdit->setText(QDir::toNativeSeparators(path));
        loadPlayer();
    });
    connect(m_playerEdit, &QLineEdit::editingFinished, this, &QcPreview::loadPlayer);
    connect(m_previewButton, &QPushButton::clicked, this, &QcPreview::previewRequested);
    connect(m_sequenceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { showModels(); });
    connect(m_compareCheck, &QCheckBox::toggled, this, [this]() { showModels(); });
    connect(m_playCheck, &QCheckBox::toggled, m_view, &StudioModelView::setPlaying);
    connect(m_timeSlider, &QSlider::sliderPressed, this, [this]() {
        m_sliderHeld = true;
        m_playCheck->setChecked(false);
    });
    connect(m_timeSlider, &QSlider::sliderReleased, this, [this]() { m_sliderHeld = false; });
    connect(m_timeSlider, &QSlider::sliderMoved, this, [this](int value) {
        const int index = m_sequenceCombo->currentData().toInt();
        if (!m_scene || index < 0 || index >= static_cast<int>(m_scene->sequences.size())) return;
        const StudioSequence& sequence = m_scene->sequences[index];
        m_view->setTime(value / 1000.0f * sequence.frames / std::max(1.0f, sequence.fps));
    });

    if (!m_playerEdit->text().isEmpty()) loadPlayer();
}

QcPreview::~QcPreview() = default;

QString QcPreview::current() const
{
    return plain(m_sequenceCombo->currentText());
}

void QcPreview::setBusy(bool busy)
{
    m_busy = busy;
    m_previewButton->setEnabled(!busy);
    emit busyChanged(busy);
}

void QcPreview::loadPlayer()
{
    const QString path = m_playerEdit->text().trimmed();
    QSettings settings(QStringLiteral("Vortigaunt"), QStringLiteral("VortigauntTool"));
    if (path.isEmpty())
    {
        m_player.reset();
        settings.setValue(kPlayerKey, QString());
        showModels();
        return;
    }
    auto player = std::make_shared<StudioModel>();
    if (!player->Load(path.toStdString()))
    {
        emit report(tr("Could not load the player model %1").arg(path));
        return;
    }
    settings.setValue(kPlayerKey, QDir::fromNativeSeparators(path));
    m_player = player;
    showModels();
}

void QcPreview::build(const QString& qc, const QString& studiomdl, const QString& extra, const QString& show)
{
    if (m_busy) return;
    m_compareCheck->setVisible(static_cast<bool>(sourceOf));
    if (!QFileInfo::exists(qc) || studiomdl.isEmpty())
    {
        emit report(tr("The preview needs the QC and studiomdl."));
        return;
    }
    // copy of the QC under the preview's model name
    QFile source(qc);
    QString text = source.open(QIODevice::ReadOnly) ? QString::fromUtf8(source.readAll()) : QString();
    source.close();
    text.replace(QRegularExpression(QStringLiteral("\\$modelname\\s+\"?[^\"\\r\\n]+\"?")), QStringLiteral("$modelname \"%1.mdl\"").arg(kPreviewName));
    if (!text.endsWith(QLatin1Char('\n'))) text += QLatin1Char('\n');
    text += extra;
    const QString previewQc = QFileInfo(qc).dir().filePath(kPreviewName + QStringLiteral(".qc"));
    QFile file(previewQc);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        emit report(tr("Could not write %1").arg(previewQc));
        return;
    }
    file.write(text.toUtf8());
    file.close();

    setBusy(true);
    std::thread([this, studiomdl, previewQc, show]() {
        const QCCompileResult built = QCFile::Compile(studiomdl.toStdString(), previewQc.toStdString());
        auto scene = std::make_shared<StudioModel>();
        const bool loaded = built.Ok && scene->Load(built.ModelPath);
        // the preview is in memory; remove its files
        QFile::remove(previewQc);
        if (!built.ModelPath.empty()) QFile::remove(QString::fromStdString(built.ModelPath));
        QMetaObject::invokeMethod(this, [this, built, scene, loaded, show]() {
            setBusy(false);
            if (!loaded)
            {
                emit report(tr("The preview did not compile: %1").arg(QString::fromStdString(built.Error)));
                return;
            }
            const bool first = !m_scene;
            m_scene = scene;
            const QSignalBlocker block(m_sequenceCombo);
            m_sequenceCombo->clear();
            int chosen = 0;
            for (size_t i = 0; i < m_scene->sequences.size(); ++i)
            {
                const QString name = QString::fromStdString(m_scene->sequences[i].name);
                const QString mark = markOf ? markOf(name) : QString();
                m_sequenceCombo->addItem(mark.isEmpty() ? name : name + QStringLiteral("  (") + mark + QStringLiteral(")"), static_cast<int>(i));
                if (name == show) chosen = m_sequenceCombo->count() - 1;
            }
            m_sequenceCombo->setCurrentIndex(chosen);
            showModels();
            if (first) m_view->fitView();
        }, Qt::QueuedConnection);
    }).detach();
}

void QcPreview::showModels()
{
    if (!m_scene) return;
    const int index = m_sequenceCombo->currentData().toInt();
    m_view->clearModels();
    const int scene = m_view->addModel(m_scene);
    m_view->setSequence(scene, index);
    if (m_player) m_view->addModel(m_player, scene);

    // the source sequence beside it
    const QString source = sourceOf ? sourceOf(current()) : QString();
    if (m_compareCheck->isChecked() && !source.isEmpty())
    {
        int other = -1;
        for (size_t i = 0; i < m_scene->sequences.size(); ++i)
            if (QString::fromStdString(m_scene->sequences[i].name) == source) other = static_cast<int>(i);
        if (other >= 0)
        {
            const std::vector<std::array<float, 6>> rest = m_scene->AnimationValues(-1, 0, 0);
            float fx = 1.0f, fy = 0.0f;
            if (!rest.empty())
            {
                fx = std::cos(rest[0][5]) * std::cos(rest[0][4]);
                fy = std::sin(rest[0][5]) * std::cos(rest[0][4]);
            }
            const float length = std::max(std::hypot(fx, fy), 1e-6f);
            aiMatrix4x4 placement;
            placement.a4 = -fy / length * 48.0f;
            placement.b4 = fx / length * 48.0f;
            const int beside = m_view->addModel(m_scene);
            m_view->setPlacement(beside, placement);
            m_view->setSequence(beside, other);
            if (m_player) m_view->addModel(m_player, beside);
        }
    }
    m_view->setGuides(guidesOf ? guidesOf(current()) : std::vector<StudioModelView::Guide>());
    m_view->setTime(0.0f);
}

void QcPreview::tick()
{
    const int index = m_sequenceCombo->currentData().toInt();
    if (!m_scene || index < 0 || index >= static_cast<int>(m_scene->sequences.size()))
    {
        m_timeLabel->clear();
        return;
    }
    const StudioSequence& sequence = m_scene->sequences[index];
    const float length = std::max(1, sequence.frames) / std::max(1.0f, sequence.fps);
    float at = m_view->time();
    at = (sequence.flags & 1) ? std::fmod(at, length) : std::min(at, length);
    if (!m_sliderHeld)
    {
        const QSignalBlocker block(m_timeSlider);
        m_timeSlider->setValue(static_cast<int>(at / length * 1000.0f));
    }
    QString text = tr("%1 / %2 s, frame %3").arg(at, 0, 'f', 2).arg(length, 0, 'f', 2).arg(static_cast<int>(at * sequence.fps));
    const QString note = noteOf ? noteOf(current()) : QString();
    if (!note.isEmpty()) text += QStringLiteral("\n") + note;
    m_timeLabel->setText(text);
}
