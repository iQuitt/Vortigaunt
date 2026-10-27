// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#pragma once

#include <QWidget>

#include <assimp/matrix4x4.h>

#include <memory>

class QAction;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QSpinBox;
class StudioModel;
class StudioModelView;

// P model tab: a p_ model in the hands of a player model, moved and turned there, and saved where it is shown
class WeaponFitWorkspace : public QWidget
{
    Q_OBJECT
public:
    explicit WeaponFitWorkspace(QWidget* parent = nullptr);

    bool openWeapon(const QString& path);
    // remember: kept for the next start
    bool setPlayerModel(const QString& path, bool remember);
    bool isModified() const;

    QAction* openAction() const { return m_openAction; }
    QAction* playerAction() const { return m_playerAction; }
    QAction* saveAction() const { return m_saveAction; }
    QAction* saveAsAction() const { return m_saveAsAction; }
    QAction* resetAction() const { return m_resetAction; }

signals:
    void modifiedChanged(bool modified);

private:
    // fit: the camera is put in front of it
    bool loadWeapon(const QString& path, bool fit);
    void openWeaponDialog();
    void choosePlayerDialog();
    void save(const QString& path);
    // the player and the weapon in the view again, the weapon merged into the player
    void rebuildScene();
    void fillSequences();
    // the sequence shown, with legs that stand as in the game while the upper body aims
    void showSequence();
    void fillBones();
    void applyPlacement();
    void resetPlacement();
    aiMatrix4x4 placement() const;
    void updateState();

    std::shared_ptr<StudioModel> m_player;
    std::shared_ptr<StudioModel> m_weapon;
    QString m_playerPath;
    QString m_weaponPath;
    int m_weaponIndex = -1;  // in the view
    bool m_changingPlacement = false;

    StudioModelView* m_view = nullptr;
    QLabel* m_playerLabel = nullptr;
    QComboBox* m_sequenceCombo = nullptr;
    QSpinBox* m_aimSpin = nullptr;
    QCheckBox* m_playCheck = nullptr;
    QLabel* m_weaponLabel = nullptr;
    QComboBox* m_boneCombo = nullptr;
    QDoubleSpinBox* m_move[3] = {};
    QDoubleSpinBox* m_turn[3] = {};
    QDoubleSpinBox* m_scale = nullptr;
    QLabel* m_stateLabel = nullptr;

    QAction* m_openAction = nullptr;
    QAction* m_playerAction = nullptr;
    QAction* m_saveAction = nullptr;
    QAction* m_saveAsAction = nullptr;
    QAction* m_resetAction = nullptr;
};
