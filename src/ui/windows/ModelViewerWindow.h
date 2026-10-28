// Copyright (C) 2026 iQuit (https://github.com/iQuitt)
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#pragma once

#include <QWidget>

class QCheckBox;
class QComboBox;
class StudioModelView;

// A GoldSrc model on its own, playing its sequences
class ModelViewerWindow : public QWidget
{
    Q_OBJECT
public:
    explicit ModelViewerWindow(QWidget* parent = nullptr);

    bool load(const QString& path);

private:
    StudioModelView* m_view = nullptr;
    QComboBox* m_sequenceCombo = nullptr;
    QCheckBox* m_playCheck = nullptr;
};
