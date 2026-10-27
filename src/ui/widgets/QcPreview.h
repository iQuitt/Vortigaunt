// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#pragma once

#include "StudioModelView.h"

#include <QWidget>

#include <functional>
#include <memory>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
class QTimer;
class StudioModel;

// Compiles a QC to a temporary model and plays its sequences, optionally on a player model.
class QcPreview : public QWidget
{
    Q_OBJECT
public:
    explicit QcPreview(QWidget* parent = nullptr);
    ~QcPreview() override;

    // compiles the QC plus extra QC text and shows the named sequence
    void build(const QString& qc, const QString& studiomdl, const QString& extra, const QString& show);
    bool isBusy() const { return m_busy; }
    QString current() const;

    // optional per-sequence hooks
    std::function<QString(const QString&)> markOf, sourceOf, noteOf;
    std::function<std::vector<StudioModelView::Guide>(const QString&)> guidesOf;

signals:
    void previewRequested();
    void busyChanged(bool busy);
    void report(const QString& line);

private:
    void showModels();
    void loadPlayer();
    void tick();
    void setBusy(bool busy);

    QLineEdit* m_playerEdit = nullptr;
    QComboBox* m_sequenceCombo = nullptr;
    QCheckBox* m_compareCheck = nullptr;
    QCheckBox* m_playCheck = nullptr;
    QPushButton* m_previewButton = nullptr;
    QSlider* m_timeSlider = nullptr;
    QLabel* m_timeLabel = nullptr;
    StudioModelView* m_view = nullptr;
    QTimer* m_clock = nullptr;
    bool m_sliderHeld = false;
    bool m_busy = false;
    std::shared_ptr<StudioModel> m_scene, m_player;
};
