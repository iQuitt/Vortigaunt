// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#pragma once

#include "ui/HudIcon.h"

#include <QDialog>

#include <assimp/matrix4x4.h>

#include <memory>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTimer;
class StudioModel;
class StudioModelView;

// Makes a weapon's 640hud sprite and hud .txt from its model.
class HudSpriteDialog : public QDialog
{
    Q_OBJECT
public:
    explicit HudSpriteDialog(QWidget* parent = nullptr);
    ~HudSpriteDialog() override;

private:
    void openModel(const QString& path);
    void scheduleRender();
    void render();
    QImage drawWeapon(float yaw, float pitch);
    // turn, tilt and roll as one matrix
    aiMatrix4x4 turning() const;
    // a ring dragged in the view
    void turnByHandle(int axis, float degrees);
    void openSheet(const QString& path);
    void showSheetCells();
    // the sheet to write
    QImage sheetToWrite() const;
    int sheetRow() const;
    void updateDrawn();
    void save();
    HudIconLook look() const;
    QString outputFolder() const;

    QLineEdit* m_modelEdit = nullptr;
    QComboBox* m_viewCombo = nullptr;
    QDoubleSpinBox* m_turnSpin = nullptr;
    QDoubleSpinBox* m_tiltSpin = nullptr;
    QDoubleSpinBox* m_rollSpin = nullptr;
    QCheckBox* m_mirrorCheck = nullptr;
    QCheckBox* m_levelCheck = nullptr;
    QComboBox* m_labelCombo = nullptr;
    QSpinBox* m_brightnessSpin = nullptr;
    QDoubleSpinBox* m_contrastSpin = nullptr;
    QCheckBox* m_rightCheck = nullptr;
    QCheckBox* m_overCheck = nullptr;
    QCheckBox* m_handsCheck = nullptr;
    QLineEdit* m_sheetEdit = nullptr;
    QComboBox* m_cellsCombo = nullptr;
    QLabel* m_sheetPreview = nullptr;
    QLineEdit* m_spriteEdit = nullptr;
    QLineEdit* m_weaponEdit = nullptr;
    QLineEdit* m_outputEdit = nullptr;
    QPushButton* m_saveButton = nullptr;
    QLabel* m_normalPreview = nullptr;
    QLabel* m_selectedPreview = nullptr;
    QLabel* m_status = nullptr;
    StudioModelView* m_view = nullptr;
    // off-screen render for the icon
    std::unique_ptr<StudioModelView> m_render;
    QTimer* m_renderTimer = nullptr;
    std::shared_ptr<StudioModel> m_model;
    std::shared_ptr<StudioModel> m_drawn;  // m_model, or a copy without the hands
    std::vector<std::vector<std::vector<bool>>> m_hands;
    QImage m_weapon;
    QImage m_sheet;          // an existing sheet the weapon goes into
    int m_shownLook = -1;    // last view the camera was set to
};
