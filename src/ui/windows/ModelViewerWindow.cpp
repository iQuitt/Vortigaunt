// Copyright (C) 2026 iQuit (https://github.com/iQuitt)
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#include "ModelViewerWindow.h"

#include "core/mdl/StudioModel.h"
#include "ui/widgets/StudioModelView.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

ModelViewerWindow::ModelViewerWindow(QWidget* parent)
    : QWidget(parent, Qt::Window)
{
    resize(900, 700);
    auto* layout = new QVBoxLayout(this);
    auto* bar = new QHBoxLayout();
    bar->addWidget(new QLabel(tr("Sequence:")));
    m_sequenceCombo = new QComboBox();
    m_sequenceCombo->setObjectName(QStringLiteral("sequence"));
    bar->addWidget(m_sequenceCombo, 1);
    m_playCheck = new QCheckBox(tr("Play"));
    m_playCheck->setChecked(true);
    bar->addWidget(m_playCheck);
    layout->addLayout(bar);
    m_view = new StudioModelView();
    layout->addWidget(m_view, 1);

    connect(m_sequenceCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index < 0 || !m_view->modelCount()) return;
        m_view->setSequence(0, index);
        m_view->setTime(0.0f);
    });
    connect(m_playCheck, &QCheckBox::toggled, this, [this](bool playing) { m_view->setPlaying(playing); });
}

bool ModelViewerWindow::load(const QString& path)
{
    setWindowTitle(QFileInfo(path).fileName());
    auto model = std::make_shared<StudioModel>();
    if (!model->Load(path.toStdString()))
    {
        m_view->placeholder = tr("Could not read %1").arg(QFileInfo(path).fileName());
        m_view->update();
        return false;
    }
    m_view->clearModels();
    m_view->addModel(model);
    {
        const QSignalBlocker block(m_sequenceCombo);
        m_sequenceCombo->clear();
        for (const StudioSequence& sequence : model->sequences) m_sequenceCombo->addItem(QString::fromStdString(sequence.name));
    }
    m_view->setSequence(0, 0);
    m_view->setPlaying(m_playCheck->isChecked());
    m_view->fitView();
    return true;
}
