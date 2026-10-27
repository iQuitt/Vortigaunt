// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#include "WeaponFitWorkspace.h"

#include "core/VortigauntLog.h"
#include "core/mdl/StudioModel.h"
#include "ui/widgets/StudioModelView.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSpinBox>
#include <QSplitter>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <map>

namespace {

const char* kPlayerSetting = "weaponFit/playerModel";
constexpr double kRadiansPerDegree = 3.14159265358979323846 / 180.0;

QDoubleSpinBox* spinBox(QWidget* parent, double low, double high, double step, int decimals, const QString& suffix)
{
    auto* spin = new QDoubleSpinBox(parent);
    spin->setRange(low, high);
    spin->setSingleStep(step);
    spin->setDecimals(decimals);
    spin->setSuffix(suffix);
    spin->setAccelerated(true);
    return spin;
}

} // namespace

WeaponFitWorkspace::WeaponFitWorkspace(QWidget* parent)
    : QWidget(parent)
{
    auto action = [this](const QString& text, const QString& tip) {
        auto* created = new QAction(text, this);
        created->setToolTip(tip);
        addAction(created);
        return created;
    };
    m_openAction = action(tr("Open p_ model"), tr("Open the p_ model to place in the player's hands"));
    m_playerAction = action(tr("Player model"), tr("Choose the player model the p_ model is shown on; it is remembered"));
    m_saveAction = action(tr("Save"), tr("Write the p_ model with the weapon where it is shown"));
    m_saveAsAction = action(tr("Save as"), tr("Write the placed p_ model to a new file"));
    m_resetAction = action(tr("Reset"), tr("Put the weapon back where the file has it"));
    m_saveAction->setShortcut(QKeySequence::Save);
    m_saveAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    layout->addWidget(splitter);

    // left: the player and the weapon
    auto* left = new QWidget(splitter);
    auto* leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(4, 4, 4, 4);
    auto* playerGroup = new QGroupBox(tr("Player"), left);
    auto* playerLayout = new QVBoxLayout(playerGroup);
    m_playerLabel = new QLabel(playerGroup);
    m_playerLabel->setWordWrap(true);
    auto* playerButton = new QPushButton(tr("Choose player model..."), playerGroup);
    m_sequenceCombo = new QComboBox(playerGroup);
    m_sequenceCombo->setToolTip(tr("The player's animation; the ref_aim one of the weapon fits best"));
    m_aimSpin = new QSpinBox(playerGroup);
    m_aimSpin->setRange(-90, 90);
    m_aimSpin->setSuffix(QStringLiteral("°"));
    m_aimSpin->setToolTip(tr("Where the player looks: 0 straight ahead, -90 up, 90 down"));
    m_playCheck = new QCheckBox(tr("Play"), playerGroup);
    m_playCheck->setChecked(true);
    playerLayout->addWidget(m_playerLabel);
    playerLayout->addWidget(playerButton);
    playerLayout->addWidget(new QLabel(tr("Animation:"), playerGroup));
    playerLayout->addWidget(m_sequenceCombo);
    playerLayout->addWidget(new QLabel(tr("Looking:"), playerGroup));
    playerLayout->addWidget(m_aimSpin);
    playerLayout->addWidget(m_playCheck);
    leftLayout->addWidget(playerGroup);

    auto* weaponGroup = new QGroupBox(tr("Weapon"), left);
    auto* weaponLayout = new QVBoxLayout(weaponGroup);
    m_weaponLabel = new QLabel(weaponGroup);
    m_weaponLabel->setWordWrap(true);
    auto* weaponButton = new QPushButton(tr("Open p_ model..."), weaponGroup);
    m_boneCombo = new QComboBox(weaponGroup);
    m_boneCombo->setToolTip(tr("The bone whose vertices are moved; a p_ model hangs on the right hand, a dual one on both"));
    weaponLayout->addWidget(m_weaponLabel);
    weaponLayout->addWidget(weaponButton);
    weaponLayout->addWidget(new QLabel(tr("Bone:"), weaponGroup));
    weaponLayout->addWidget(m_boneCombo);
    leftLayout->addWidget(weaponGroup);
    leftLayout->addStretch();

    // centre: the player holding the weapon
    m_view = new StudioModelView(splitter);
    m_view->placeholder = tr("Open a p_ model from the ribbon, and choose a player model to hold it.");

    // right: where the weapon sits, in the axes of the bone
    auto* rightScroll = new QScrollArea(splitter);
    rightScroll->setWidgetResizable(true);
    rightScroll->setFrameShape(QFrame::NoFrame);
    rightScroll->setMinimumWidth(260);
    auto* right = new QWidget();
    rightScroll->setWidget(right);
    auto* rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(4, 4, 4, 4);
    auto* placeGroup = new QGroupBox(tr("Placement"), right);
    auto* placeLayout = new QFormLayout(placeGroup);
    const char* axes[] = { "X", "Y", "Z" };
    for (int i = 0; i < 3; ++i)
    {
        m_move[i] = spinBox(placeGroup, -128.0, 128.0, 0.25, 2, QString());
        placeLayout->addRow(tr("Move %1:").arg(axes[i]), m_move[i]);
    }
    for (int i = 0; i < 3; ++i)
    {
        m_turn[i] = spinBox(placeGroup, -180.0, 180.0, 1.0, 1, QStringLiteral("°"));
        m_turn[i]->setWrapping(true);
        placeLayout->addRow(tr("Turn %1:").arg(axes[i]), m_turn[i]);
    }
    m_scale = spinBox(placeGroup, 10.0, 400.0, 1.0, 1, QStringLiteral("%"));
    m_scale->setValue(100.0);
    placeLayout->addRow(tr("Size:"), m_scale);
    auto* resetButton = new QPushButton(tr("Reset"), placeGroup);
    placeLayout->addRow(resetButton);
    auto* focusButton = new QPushButton(tr("Zoom to the weapon"), placeGroup);
    focusButton->setToolTip(tr("Put the camera close to the weapon; double-click the view to see all again"));
    placeLayout->addRow(focusButton);
    connect(focusButton, &QPushButton::clicked, this, [this]() {
        if (m_weaponIndex >= 0)
            m_view->fitView(m_weaponIndex);
    });
    rightLayout->addWidget(placeGroup);
    m_stateLabel = new QLabel(right);
    m_stateLabel->setWordWrap(true);
    rightLayout->addWidget(m_stateLabel);
    auto* saveButton = new QPushButton(tr("Save"), right);
    rightLayout->addWidget(saveButton);
    rightLayout->addStretch();

    splitter->addWidget(left);
    splitter->addWidget(m_view);
    splitter->addWidget(rightScroll);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({ 260, 800, 280 });

    connect(m_openAction, &QAction::triggered, this, &WeaponFitWorkspace::openWeaponDialog);
    connect(weaponButton, &QPushButton::clicked, this, &WeaponFitWorkspace::openWeaponDialog);
    connect(m_playerAction, &QAction::triggered, this, &WeaponFitWorkspace::choosePlayerDialog);
    connect(playerButton, &QPushButton::clicked, this, &WeaponFitWorkspace::choosePlayerDialog);
    connect(m_saveAction, &QAction::triggered, this, [this]() { save(m_weaponPath); });
    connect(saveButton, &QPushButton::clicked, this, [this]() { save(m_weaponPath); });
    connect(m_saveAsAction, &QAction::triggered, this, [this]() {
        const QString path = QFileDialog::getSaveFileName(this, tr("Save p_ model"), m_weaponPath, tr("GoldSrc model (*.mdl)"));
        if (!path.isEmpty())
            save(path);
    });
    connect(m_resetAction, &QAction::triggered, this, &WeaponFitWorkspace::resetPlacement);
    connect(resetButton, &QPushButton::clicked, this, &WeaponFitWorkspace::resetPlacement);
    connect(m_sequenceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &WeaponFitWorkspace::showSequence);
    connect(m_aimSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int degrees) {
        if (m_player)
            m_view->setAimPitch(0, static_cast<float>(degrees));
    });
    connect(m_playCheck, &QCheckBox::toggled, m_view, &StudioModelView::setPlaying);
    connect(m_boneCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &WeaponFitWorkspace::applyPlacement);
    for (QDoubleSpinBox* spin : { m_move[0], m_move[1], m_move[2], m_turn[0], m_turn[1], m_turn[2], m_scale })
        connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &WeaponFitWorkspace::applyPlacement);

    // the player model of last time
    const QString remembered = QSettings("Vortigaunt", "VortigauntTool").value(kPlayerSetting).toString();
    if (!remembered.isEmpty() && QFileInfo::exists(remembered))
        setPlayerModel(remembered, false);
    updateState();
}

bool WeaponFitWorkspace::setPlayerModel(const QString& path, bool remember)
{
    auto player = std::make_shared<StudioModel>();
    if (!player->Load(path.toStdString()))
    {
        QMessageBox::warning(this, tr("Player model"), QString::fromStdString(player->GetError()));
        return false;
    }
    m_player = player;
    m_playerPath = path;
    if (remember)
        QSettings("Vortigaunt", "VortigauntTool").setValue(kPlayerSetting, path);
    m_playerLabel->setText(QFileInfo(path).fileName());
    m_playerLabel->setToolTip(QDir::toNativeSeparators(path));
    rebuildScene();
    fillSequences();
    return true;
}

bool WeaponFitWorkspace::openWeapon(const QString& path)
{
    return loadWeapon(path, true);
}

bool WeaponFitWorkspace::loadWeapon(const QString& path, bool fit)
{
    auto weapon = std::make_shared<StudioModel>();
    if (!weapon->Load(path.toStdString()))
    {
        QMessageBox::warning(this, tr("p_ model"), QString::fromStdString(weapon->GetError()));
        return false;
    }
    m_weapon = weapon;
    m_weaponPath = path;
    m_weaponLabel->setText(QFileInfo(path).fileName());
    m_weaponLabel->setToolTip(QDir::toNativeSeparators(path));
    m_changingPlacement = true;
    fillBones();
    m_changingPlacement = false;
    resetPlacement();
    rebuildScene();
    fillSequences();
    if (fit)
        m_view->fitView();
    return true;
}

void WeaponFitWorkspace::openWeaponDialog()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Open p_ model"), QFileInfo(m_weaponPath).absolutePath(),
                                                      tr("GoldSrc model (*.mdl)"));
    if (!path.isEmpty())
        openWeapon(path);
}

void WeaponFitWorkspace::choosePlayerDialog()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Choose player model"), QFileInfo(m_playerPath).absolutePath(),
                                                      tr("GoldSrc model (*.mdl)"));
    if (!path.isEmpty())
        setPlayerModel(path, true);
}

void WeaponFitWorkspace::rebuildScene()
{
    const int sequence = m_sequenceCombo->currentData().isValid() ? m_sequenceCombo->currentData().toInt() : 0;
    m_view->clearModels();
    m_weaponIndex = -1;
    int player = -1;
    if (m_player)
    {
        player = m_view->addModel(m_player);
        m_view->setSequence(player, sequence);
        m_view->setAimPitch(player, static_cast<float>(m_aimSpin->value()));
    }
    if (m_weapon)
        m_weaponIndex = m_view->addModel(m_weapon, player);
    m_view->setPlaying(m_playCheck->isChecked());
    showSequence();
    applyPlacement();
}

void WeaponFitWorkspace::showSequence()
{
    if (!m_player)
        return;
    const int sequence = m_sequenceCombo->currentData().isValid() ? m_sequenceCombo->currentData().toInt() : 0;
    m_view->setSequence(0, sequence);

    // the game gives an aiming player's legs their own sequence: standing, crouched for a crouched aim
    const QString name = m_sequenceCombo->currentText().toLower();
    const char* stand = name.startsWith(QStringLiteral("crouch")) ? "crouch_idle" : name.startsWith(QStringLiteral("ref_")) ? "idle1" : nullptr;
    int gait = -1;
    for (size_t i = 0; stand && i < m_player->sequences.size(); ++i)
    {
        if (QString::fromStdString(m_player->sequences[i].name).compare(QLatin1String(stand), Qt::CaseInsensitive) == 0)
            gait = static_cast<int>(i);
    }
    m_view->setGaitSequence(0, gait);
}

void WeaponFitWorkspace::fillSequences()
{
    m_sequenceCombo->blockSignals(true);
    m_sequenceCombo->clear();
    if (m_player)
    {
        for (size_t i = 0; i < m_player->sequences.size(); ++i)
            m_sequenceCombo->addItem(QString::fromStdString(m_player->sequences[i].name), static_cast<int>(i));
        // the aim of the weapon: p_ak47 -> ref_aim_ak47, else any aim
        QString weapon = QFileInfo(m_weaponPath).completeBaseName().toLower();
        if (weapon.startsWith(QStringLiteral("p_")))
            weapon = weapon.mid(2);
        int choice = m_sequenceCombo->findText(QStringLiteral("ref_aim_") + weapon, Qt::MatchFixedString);
        if (choice < 0)
            choice = m_sequenceCombo->findText(QStringLiteral("ref_aim_"), Qt::MatchStartsWith);
        m_sequenceCombo->setCurrentIndex(std::max(0, choice));
    }
    m_sequenceCombo->blockSignals(false);
    showSequence();
}

void WeaponFitWorkspace::fillBones()
{
    m_boneCombo->clear();
    if (!m_weapon)
        return;
    // only the bones that carry vertices, the busiest first
    std::map<int, int> counts;
    for (const StudioBodyPart& part : m_weapon->bodyParts)
    {
        for (const StudioSubModel& sub : part.models)
        {
            for (int bone : sub.vertexBones)
                counts[bone]++;
        }
    }
    std::vector<std::pair<int, int>> order(counts.begin(), counts.end());
    std::sort(order.begin(), order.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    for (const auto& [bone, count] : order)
        m_boneCombo->addItem(tr("%1 (%2 vertices)").arg(QString::fromStdString(m_weapon->bones[bone].name)).arg(count), bone);
}

aiMatrix4x4 WeaponFitWorkspace::placement() const
{
    // scaled, turned, then moved, all in the bone's axes
    const double radians = kRadiansPerDegree;
    aiMatrix4x4 turn;
    turn.FromEulerAnglesXYZ(static_cast<float>(m_turn[0]->value() * radians), static_cast<float>(m_turn[1]->value() * radians),
                            static_cast<float>(m_turn[2]->value() * radians));
    aiMatrix4x4 scale;
    aiMatrix4x4::Scaling(aiVector3D(static_cast<float>(m_scale->value() / 100.0)), scale);
    aiMatrix4x4 move;
    aiMatrix4x4::Translation(aiVector3D(static_cast<float>(m_move[0]->value()), static_cast<float>(m_move[1]->value()),
                                        static_cast<float>(m_move[2]->value())), move);
    return move * turn * scale;
}

bool WeaponFitWorkspace::isModified() const
{
    for (int i = 0; i < 3; ++i)
    {
        if (m_move[i]->value() != 0.0 || m_turn[i]->value() != 0.0)
            return true;
    }
    return m_scale->value() != 100.0;
}

void WeaponFitWorkspace::applyPlacement()
{
    if (m_changingPlacement)
        return;
    if (m_weaponIndex >= 0)
        m_view->setBoneOffset(m_weaponIndex, m_boneCombo->currentData().isValid() ? m_boneCombo->currentData().toInt() : -1, placement());
    updateState();
}

void WeaponFitWorkspace::resetPlacement()
{
    m_changingPlacement = true;
    for (int i = 0; i < 3; ++i)
    {
        m_move[i]->setValue(0.0);
        m_turn[i]->setValue(0.0);
    }
    m_scale->setValue(100.0);
    m_changingPlacement = false;
    applyPlacement();
}

void WeaponFitWorkspace::updateState()
{
    const bool weapon = static_cast<bool>(m_weapon);
    const bool modified = weapon && isModified();
    m_saveAction->setEnabled(modified);
    m_saveAsAction->setEnabled(weapon);
    m_resetAction->setEnabled(modified);
    m_boneCombo->setEnabled(weapon);
    for (QDoubleSpinBox* spin : { m_move[0], m_move[1], m_move[2], m_turn[0], m_turn[1], m_turn[2], m_scale })
        spin->setEnabled(weapon);
    if (!m_player)
        m_stateLabel->setText(tr("No player model yet: the weapon is shown on its own. Choose one from a game folder, "
                                 "for example cstrike/models/player/gign/gign.mdl."));
    else
        m_stateLabel->clear();
    if (m_playerLabel->text().isEmpty())
        m_playerLabel->setText(tr("None"));
    if (m_weaponLabel->text().isEmpty())
        m_weaponLabel->setText(tr("None"));
    emit modifiedChanged(modified);
}

void WeaponFitWorkspace::save(const QString& path)
{
    if (!m_weapon || path.isEmpty())
        return;
    const int bone = m_boneCombo->currentData().isValid() ? m_boneCombo->currentData().toInt() : -1;
    std::string error;
    if (!StudioModel::TransformBoneVertices(m_weaponPath.toStdString(), path.toStdString(), bone, placement(), error))
    {
        QMessageBox::warning(this, tr("Save error"), QString::fromStdString(error));
        return;
    }
    VortigauntLog::Vortigaunt_Printf(QStringLiteral("^2Saved^7 %1").arg(QDir::toNativeSeparators(path)));
    // the placement is in the file now; it starts again from zero
    const int keepBone = m_boneCombo->currentIndex();
    loadWeapon(path, false);
    m_boneCombo->setCurrentIndex(keepBone);
}
