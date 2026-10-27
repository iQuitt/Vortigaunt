#include "HudSpriteDialog.h"
#include "SettingsDialog.h"
#include "core/SpriteLoader.h"
#include "core/VortigauntLog.h"
#include "core/converters/WeaponModelConverter.h"
#include "core/mdl/StudioModel.h"
#include "ui/widgets/StudioModelView.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QScrollArea>
#include <QScrollBar>
#include <QSplitter>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace
{
// off-screen render size
constexpr int kRenderWidth = 1600;
constexpr int kRenderHeight = 640;
// camera angles
struct Look
{
    const char* name;
    float yaw, pitch;
};
const Look kLooks[] = { { QT_TRANSLATE_NOOP("HudSpriteDialog", "Side"), 90.0f, 0.0f },
                        { QT_TRANSLATE_NOOP("HudSpriteDialog", "Front"), 0.0f, 0.0f },
                        { QT_TRANSLATE_NOOP("HudSpriteDialog", "Top"), 90.0f, 89.0f } };

QHBoxLayout* browseRow(QLineEdit* edit, QPushButton* button)
{
    auto* row = new QHBoxLayout();
    row->addWidget(edit, 1);
    row->addWidget(button);
    return row;
}

QDoubleSpinBox* angleSpin()
{
    auto* spin = new QDoubleSpinBox();
    spin->setRange(-180.0, 180.0);
    spin->setSingleStep(5.0);
    spin->setDecimals(1);
    spin->setSuffix(QStringLiteral("°"));
    spin->setWrapping(true);
    return spin;
}

// spin box with a synced slider
QHBoxLayout* angleRow(QDoubleSpinBox* spin)
{
    auto* slider = new QSlider(Qt::Horizontal);
    slider->setRange(-180, 180);
    slider->setPageStep(15);
    QObject::connect(slider, &QSlider::valueChanged, spin, [spin](int value) { spin->setValue(value); });
    QObject::connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), slider, [slider](double value) {
        const QSignalBlocker block(slider);
        slider->setValue(static_cast<int>(std::lround(value)));
    });
    auto* row = new QHBoxLayout();
    row->addWidget(slider, 1);
    row->addWidget(spin);
    return row;
}

// a cell at 2x
QPixmap shown(const QImage& cell)
{
    return QPixmap::fromImage(cell.scaled(cell.size() * 2, Qt::KeepAspectRatio, Qt::FastTransformation));
}
}

HudSpriteDialog::HudSpriteDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("HUD Sprite"));
    auto* outer = new QHBoxLayout(this);
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    outer->addWidget(splitter);

    // left: settings, scrollable
    auto* leftScroll = new QScrollArea(splitter);
    leftScroll->setWidgetResizable(true);
    leftScroll->setFrameShape(QFrame::NoFrame);
    auto* left = new QWidget();
    leftScroll->setWidget(left);
    auto* layout = new QVBoxLayout(left);

    auto* modelGroup = new QGroupBox(tr("Model"));
    auto* modelForm = new QFormLayout(modelGroup);
    m_modelEdit = new QLineEdit();
    m_modelEdit->setObjectName(QStringLiteral("model"));
    m_modelEdit->setPlaceholderText(tr("The weapon's model: a w_ or a p_ model shows it whole"));
    auto* modelBrowse = new QPushButton(tr("Browse..."));
    modelForm->addRow(tr("Model:"), browseRow(m_modelEdit, modelBrowse));
    m_handsCheck = new QCheckBox(tr("Hide the hands"));
    m_handsCheck->setObjectName(QStringLiteral("hands"));
    m_handsCheck->setToolTip(tr("Leaves a v_ model's hands out of the icon"));
    m_handsCheck->setEnabled(false);
    modelForm->addRow(m_handsCheck);
    layout->addWidget(modelGroup);
    layout->addStretch(1);

    auto* turnGroup = new QGroupBox(tr("Turn"));
    auto* turnForm = new QFormLayout(turnGroup);
    m_viewCombo = new QComboBox();
    for (const Look& view : kLooks) m_viewCombo->addItem(tr(view.name));
    m_viewCombo->setToolTip(tr("Where the model is seen from; on opening, the one it shows the most of. A w_ model lies on its side, "
                               "and is seen from the top"));
    turnForm->addRow(tr("Seen from:"), m_viewCombo);
    m_turnSpin = angleSpin();
    m_turnSpin->setToolTip(tr("About the up axis: 0 and 180 show the two sides"));
    turnForm->addRow(tr("Turn:"), angleRow(m_turnSpin));
    m_tiltSpin = angleSpin();
    turnForm->addRow(tr("Tilt:"), angleRow(m_tiltSpin));
    m_rollSpin = angleSpin();
    turnForm->addRow(tr("Roll:"), angleRow(m_rollSpin));
    m_levelCheck = new QCheckBox(tr("Lay it level"));
    m_levelCheck->setChecked(true);
    m_levelCheck->setToolTip(tr("Turns the picture so the weapon's length lies along the cell, as a w_ model lying askew on the floor "
                                "does not"));
    turnForm->addRow(m_levelCheck);
    m_mirrorCheck = new QCheckBox(tr("Mirror"));
    m_mirrorCheck->setToolTip(tr("The game's icons have the muzzle on the left; on opening, a model is mirrored when its muzzle is "
                                 "on the right"));
    turnForm->addRow(m_mirrorCheck);
    layout->addWidget(turnGroup);
    layout->addStretch(1);

    auto* lookGroup = new QGroupBox(tr("Look"));
    auto* lookForm = new QFormLayout(lookGroup);
    m_labelCombo = new QComboBox();
    m_labelCombo->setEditable(true);
    m_labelCombo->addItems({ QStringLiteral("PRIMARY"), QStringLiteral("SECONDARY"), QStringLiteral("MELEE"), QStringLiteral("GRENADE"), QStringLiteral("EQUIPMENT") });
    m_labelCombo->setToolTip(tr("The slot's name written under the weapon"));
    lookForm->addRow(tr("Name:"), m_labelCombo);
    m_brightnessSpin = new QSpinBox();
    m_brightnessSpin->setRange(-100, 100);
    m_brightnessSpin->setSingleStep(5);
    lookForm->addRow(tr("Brightness:"), m_brightnessSpin);
    m_contrastSpin = new QDoubleSpinBox();
    m_contrastSpin->setRange(0.3, 3.0);
    m_contrastSpin->setSingleStep(0.1);
    m_contrastSpin->setValue(1.0);
    lookForm->addRow(tr("Contrast:"), m_contrastSpin);
    m_rightCheck = new QCheckBox(tr("Against the right edge, as the game's are"));
    m_rightCheck->setChecked(true);
    lookForm->addRow(m_rightCheck);
    m_overCheck = new QCheckBox(tr("Name over the weapon, as CSO's are"));
    m_overCheck->setToolTip(tr("The weapon as large as the cell holds, the name written over it; else a weapon that would reach the "
                               "name is kept above it"));
    lookForm->addRow(m_overCheck);
    layout->addWidget(lookGroup);
    layout->addStretch(1);

    auto* outputGroup = new QGroupBox(tr("Output"));
    auto* outputForm = new QFormLayout(outputGroup);
    m_sheetEdit = new QLineEdit();
    m_sheetEdit->setObjectName(QStringLiteral("sheet"));
    m_sheetEdit->setPlaceholderText(tr("A sheet of its own"));
    m_sheetEdit->setToolTip(tr("An existing 640hud sheet to put the weapon in, in two of its cells; empty for a sheet of its own"));
    auto* sheetBrowse = new QPushButton(tr("Browse..."));
    outputForm->addRow(tr("Into sheet:"), browseRow(m_sheetEdit, sheetBrowse));
    m_cellsCombo = new QComboBox();
    m_cellsCombo->setObjectName(QStringLiteral("cells"));
    m_cellsCombo->setToolTip(tr("The sheet's cells the weapon takes: as it looks, and as it looks selected under it"));
    m_cellsCombo->setEnabled(false);
    outputForm->addRow(tr("Cells:"), m_cellsCombo);
    m_spriteEdit = new QLineEdit();
    m_spriteEdit->setPlaceholderText(QStringLiteral("640hud_weapon"));
    m_spriteEdit->setToolTip(tr("The sprite's name, in sprites/ of the game"));
    outputForm->addRow(tr("Sprite:"), m_spriteEdit);
    m_weaponEdit = new QLineEdit();
    m_weaponEdit->setPlaceholderText(QStringLiteral("weapon_name"));
    m_weaponEdit->setToolTip(tr("The weapon's hud .txt: its classname, or the name a plugin gives it"));
    outputForm->addRow(tr("Hud .txt:"), m_weaponEdit);
    m_outputEdit = new QLineEdit();
    m_outputEdit->setObjectName(QStringLiteral("output"));
    m_outputEdit->setPlaceholderText(QDir::toNativeSeparators(QDir(SettingsDialog::getOutputRootDir()).filePath(QStringLiteral("HudSprites"))));
    auto* outputBrowse = new QPushButton(tr("Browse..."));
    outputForm->addRow(tr("Folder:"), browseRow(m_outputEdit, outputBrowse));
    layout->addWidget(outputGroup);
    layout->addStretch(1);
    auto* buttons = new QHBoxLayout();
    auto* openButton = new QPushButton(tr("Open folder"));
    m_saveButton = new QPushButton(tr("Save sprite"));
    m_saveButton->setObjectName(QStringLiteral("save"));
    buttons->addStretch();
    buttons->addWidget(openButton);
    buttons->addWidget(m_saveButton);
    layout->addLayout(buttons);
    // no horizontal scrolling
    leftScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    leftScroll->setMinimumWidth(left->minimumSizeHint().width() + leftScroll->verticalScrollBar()->sizeHint().width());

    // right: view and cells
    auto* right = new QWidget(splitter);
    auto* rightLayout = new QVBoxLayout(right);
    m_view = new StudioModelView(right);
    m_view->placeholder = tr("Open a weapon's model.");
    m_view->setOrthographic(true);
    m_view->setPlaying(false);
    m_view->setMinimumHeight(220);
    m_view->setTurnHandles(true);
    m_view->setToolTip(tr("Drag the red, green and blue rings to turn the model about X, Y and Z; drag elsewhere to look about it"));
    rightLayout->addWidget(m_view, 1);
    m_normalPreview = new QLabel();
    m_selectedPreview = new QLabel();
    for (QLabel* preview : { m_normalPreview, m_selectedPreview })
    {
        preview->setMinimumSize(hudicon::kCellWidth * 2, hudicon::kCellHeight * 2);
        preview->setAlignment(Qt::AlignCenter);
        preview->setStyleSheet(QStringLiteral("background: black;"));
    }
    // cells, sheet beside them
    auto* cellsRow = new QHBoxLayout();
    auto* cellsColumn = new QVBoxLayout();
    cellsColumn->addWidget(new QLabel(tr("In the list:")));
    cellsColumn->addWidget(m_normalPreview);
    cellsColumn->addWidget(new QLabel(tr("Selected:")));
    cellsColumn->addWidget(m_selectedPreview);
    cellsRow->addLayout(cellsColumn, 1);
    m_sheetPreview = new QLabel();
    m_sheetPreview->setAlignment(Qt::AlignCenter);
    m_sheetPreview->setStyleSheet(QStringLiteral("background: black;"));
    m_sheetPreview->setToolTip(tr("The sheet as it will be written"));
    m_sheetPreview->hide();
    cellsRow->addWidget(m_sheetPreview);
    rightLayout->addLayout(cellsRow);
    m_status = new QLabel();
    m_status->setWordWrap(true);
    rightLayout->addWidget(m_status);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);

    m_renderTimer = new QTimer(this);
    m_renderTimer->setSingleShot(true);
    m_renderTimer->setInterval(120);
    connect(m_renderTimer, &QTimer::timeout, this, &HudSpriteDialog::render);

    connect(modelBrowse, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(this, tr("Select the weapon's model"), QFileInfo(m_modelEdit->text()).absolutePath(),
                                                          tr("Studio models (*.mdl);;All files (*.*)"));
        if (!path.isEmpty()) openModel(path);
    });
    connect(m_modelEdit, &QLineEdit::editingFinished, this, [this]() {
        if (QFileInfo::exists(m_modelEdit->text().trimmed())) openModel(m_modelEdit->text().trimmed());
    });
    connect(outputBrowse, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getExistingDirectory(this, tr("Select the output folder"), outputFolder());
        if (!path.isEmpty()) m_outputEdit->setText(QDir::toNativeSeparators(path));
    });
    connect(openButton, &QPushButton::clicked, this, [this]() {
        QDir().mkpath(outputFolder());
        QDesktopServices::openUrl(QUrl::fromLocalFile(outputFolder()));
    });
    connect(m_saveButton, &QPushButton::clicked, this, &HudSpriteDialog::save);
    connect(m_view, &StudioModelView::turnedByHandle, this, &HudSpriteDialog::turnByHandle);
    connect(sheetBrowse, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(this, tr("Select a 640hud sheet"), QFileInfo(m_sheetEdit->text()).absolutePath(),
                                                          tr("Sprites (*.spr);;All files (*.*)"));
        if (!path.isEmpty()) openSheet(path);
    });
    connect(m_sheetEdit, &QLineEdit::editingFinished, this, [this]() {
        const QString path = m_sheetEdit->text().trimmed();
        if (path.isEmpty())
        {
            m_sheet = QImage();
            showSheetCells();
        }
        else if (QFileInfo::exists(path))
            openSheet(path);
    });
    connect(m_cellsCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &HudSpriteDialog::scheduleRender);
    for (QDoubleSpinBox* spin : { m_turnSpin, m_tiltSpin, m_rollSpin, m_contrastSpin })
        connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &HudSpriteDialog::scheduleRender);
    connect(m_brightnessSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &HudSpriteDialog::scheduleRender);
    for (QCheckBox* check : { m_mirrorCheck, m_rightCheck, m_levelCheck, m_overCheck }) connect(check, &QCheckBox::toggled, this, &HudSpriteDialog::scheduleRender);
    connect(m_viewCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &HudSpriteDialog::scheduleRender);
    connect(m_handsCheck, &QCheckBox::toggled, this, [this]() {
        updateDrawn();
        m_view->clearModels();
        scheduleRender();
    });
    connect(m_labelCombo, &QComboBox::currentTextChanged, this, &HudSpriteDialog::scheduleRender);
}

HudSpriteDialog::~HudSpriteDialog() = default;

QString HudSpriteDialog::outputFolder() const
{
    const QString folder = m_outputEdit->text().trimmed();
    return folder.isEmpty() ? QDir(SettingsDialog::getOutputRootDir()).filePath(QStringLiteral("HudSprites")) : folder;
}

HudIconLook HudSpriteDialog::look() const
{
    HudIconLook look;
    look.label = m_labelCombo->currentText().trimmed().toUpper();
    look.brightness = m_brightnessSpin->value();
    look.contrast = static_cast<float>(m_contrastSpin->value());
    look.alignRight = m_rightCheck->isChecked();
    look.nameOverWeapon = m_overCheck->isChecked();
    return look;
}

void HudSpriteDialog::openModel(const QString& path)
{
    auto model = std::make_shared<StudioModel>();
    if (!model->Load(path.toStdString()))
    {
        m_status->setText(tr("Could not open %1").arg(QDir::toNativeSeparators(path)));
        return;
    }
    m_model = model;
    m_modelEdit->setText(QDir::toNativeSeparators(path));
    // hands found: the box can hide them, off at first
    m_hands = WeaponModelConverter::FindHandMeshes(*m_model);
    bool anyHands = false;
    for (const auto& part : m_hands)
        for (const auto& sub : part)
            for (const bool hand : sub) anyHands = anyHands || hand;
    {
        const QSignalBlocker blockHands(m_handsCheck);
        m_handsCheck->setChecked(false);
        m_handsCheck->setEnabled(anyHands);
    }
    updateDrawn();
    // w_m4a1 -> 640hud_m4a1, weapon_m4a1
    QString stem = QFileInfo(path).completeBaseName();
    for (const char* prefix : { "w_", "p_", "v_" })
        if (stem.startsWith(QLatin1String(prefix), Qt::CaseInsensitive)) stem = stem.mid(2);
    if (m_spriteEdit->text().isEmpty()) m_spriteEdit->setText(QStringLiteral("640hud_") + stem);
    if (m_weaponEdit->text().isEmpty()) m_weaponEdit->setText(QStringLiteral("weapon_") + stem);

    m_view->clearModels();
    m_view->addModel(m_drawn);
    m_view->fitView();
    m_shownLook = -1;
    // pick the view showing the most, then lay it level
    int bestView = 0;
    qint64 most = -1;
    bool upright = false;
    QSignalBlocker blockTurn(m_turnSpin);
    m_turnSpin->setValue(0.0);
    for (int view = 0; view < static_cast<int>(std::size(kLooks)); ++view)
    {
        const QImage weapon = drawWeapon(kLooks[view].yaw, kLooks[view].pitch);
        qint64 covered = 0;
        for (int y = 0; y < weapon.height(); ++y)
            for (int x = 0; x < weapon.width(); ++x) covered += qAlpha(weapon.pixel(x, y)) > 128 ? 1 : 0;
        if (covered > most)
        {
            most = covered;
            bestView = view;
            upright = weapon.height() > weapon.width();
        }
    }
    blockTurn.unblock();
    m_turnSpin->setValue(upright ? 90.0 : 0.0);
    const QSignalBlocker blockView(m_viewCombo);
    m_viewCombo->setCurrentIndex(bestView);
    // muzzle to the left
    const QImage seen = drawWeapon(kLooks[bestView].yaw, kLooks[bestView].pitch);
    if (!seen.isNull())
    {
        const QSignalBlocker blockMirror(m_mirrorCheck);
        m_mirrorCheck->setChecked(!hudicon::MuzzleLeft(m_levelCheck->isChecked() ? hudicon::Level(seen) : seen));
    }
    render();
}

void HudSpriteDialog::updateDrawn()
{
    m_drawn = m_model;
    if (!m_model || !m_handsCheck->isChecked()) return;
    // a copy with the hands' triangles taken out
    auto drawn = std::make_shared<StudioModel>(*m_model);
    for (size_t part = 0; part < m_hands.size() && part < drawn->bodyParts.size(); ++part)
        for (size_t sub = 0; sub < m_hands[part].size() && sub < drawn->bodyParts[part].models.size(); ++sub)
            for (size_t mesh = 0; mesh < m_hands[part][sub].size() && mesh < drawn->bodyParts[part].models[sub].meshes.size(); ++mesh)
                if (m_hands[part][sub][mesh]) drawn->bodyParts[part].models[sub].meshes[mesh].triangles.clear();
    m_drawn = drawn;
}

void HudSpriteDialog::scheduleRender()
{
    // throttled while dragging
    if (!m_renderTimer->isActive()) m_renderTimer->start();
}

aiMatrix4x4 HudSpriteDialog::turning() const
{
    aiMatrix4x4 z, y, x;
    aiMatrix4x4::RotationZ(static_cast<float>(qDegreesToRadians(m_turnSpin->value())), z);
    aiMatrix4x4::RotationY(static_cast<float>(qDegreesToRadians(m_tiltSpin->value())), y);
    aiMatrix4x4::RotationX(static_cast<float>(qDegreesToRadians(m_rollSpin->value())), x);
    return z * y * x;
}

void HudSpriteDialog::turnByHandle(int axis, float degrees)
{
    if (!m_model) return;
    aiMatrix4x4 about;
    const float radians = qDegreesToRadians(degrees);
    if (axis == 0)
        aiMatrix4x4::RotationX(radians, about);
    else if (axis == 1)
        aiMatrix4x4::RotationY(radians, about);
    else
        aiMatrix4x4::RotationZ(radians, about);
    const aiMatrix4x4 m = about * turning();
    // decompose Z * Y * X back to turn, tilt and roll
    const double sine = std::clamp(-static_cast<double>(m.c1), -1.0, 1.0);
    const double tilt = std::asin(sine);
    const bool straight = std::abs(sine) > 0.9999;
    const double turn = straight ? std::atan2(-m.a2, m.b2) : std::atan2(m.b1, m.a1);
    const double roll = straight ? 0.0 : std::atan2(m.c2, m.c3);
    m_turnSpin->setValue(qRadiansToDegrees(turn));
    m_tiltSpin->setValue(qRadiansToDegrees(tilt));
    m_rollSpin->setValue(qRadiansToDegrees(roll));
    m_view->setPlacement(0, turning());
    m_view->recenter();
}

QImage HudSpriteDialog::drawWeapon(float yaw, float pitch)
{
    // first frame of the first sequence
    const float fps = m_model->sequences.empty() ? 30.0f : std::max(1.0f, m_model->sequences[0].fps);
    if (!m_render)
    {
        m_render = std::make_unique<StudioModelView>();
        m_render->setAttribute(Qt::WA_DontShowOnScreen);
        m_render->resize(kRenderWidth, kRenderHeight);
        m_render->setOrthographic(true);
        m_render->setPlaying(false);
        m_render->show();
    }
    m_render->clearModels();
    m_render->addModel(m_drawn);
    m_render->setSequence(0, 0);
    m_render->setPlacement(0, turning());
    m_render->setTime(0.5f / fps);
    m_render->setCameraAngles(yaw, pitch);
    m_render->fitView();
    // over black and white for coverage
    m_render->setBackground(Qt::black);
    const QImage overBlack = m_render->grabFramebuffer();
    m_render->setBackground(Qt::white);
    const QImage overWhite = m_render->grabFramebuffer();
    return hudicon::Separate(overBlack, overWhite);
}

void HudSpriteDialog::render()
{
    m_renderTimer->stop();
    if (!m_model) return;
    const Look& seen = kLooks[std::max(0, m_viewCombo->currentIndex())];
    const float fps = m_model->sequences.empty() ? 30.0f : std::max(1.0f, m_model->sequences[0].fps);
    if (m_view->modelCount() == 0) m_view->addModel(m_drawn);
    m_view->setSequence(0, 0);
    m_view->setPlacement(0, turning());
    m_view->setTime(0.5f / fps);
    // reset the camera only when the view changes
    const int seenIndex = std::max(0, m_viewCombo->currentIndex());
    if (seenIndex != m_shownLook)
    {
        m_view->setCameraAngles(seen.yaw, seen.pitch);
        m_view->fitView();
        m_shownLook = seenIndex;
    }
    else
        m_view->recenter();
    m_weapon = drawWeapon(seen.yaw, seen.pitch);
    if (m_weapon.isNull())
    {
        m_status->setText(tr("Nothing of the model is in sight; turn it."));
        return;
    }
    if (m_levelCheck->isChecked()) m_weapon = hudicon::Level(m_weapon);
    if (m_mirrorCheck->isChecked()) m_weapon = m_weapon.mirrored(true, false);
    m_normalPreview->setPixmap(shown(hudicon::MakeCell(m_weapon, look(), false)));
    m_selectedPreview->setPixmap(shown(hudicon::MakeCell(m_weapon, look(), true)));
    if (!m_sheet.isNull()) m_sheetPreview->setPixmap(QPixmap::fromImage(sheetToWrite()));
    m_status->clear();
}

void HudSpriteDialog::openSheet(const QString& path)
{
    SpriteLoader sprite;
    bool loaded = false;
    try
    {
        loaded = sprite.loadFile(path.toStdString()) && !sprite.getFrames().empty() && !sprite.getFrames().front().image.isNull();
    }
    catch (const std::exception&)
    {
    }
    if (!loaded)
    {
        m_status->setText(tr("Could not open %1").arg(QDir::toNativeSeparators(path)));
        return;
    }
    const QImage sheet = sprite.getFrames().front().image.convertToFormat(QImage::Format_RGB32);
    if (sheet.width() < hudicon::kCellWidth || sheet.height() < 2 * hudicon::kCellHeight)
    {
        m_status->setText(tr("%1 has no room for two cells of the weapon list").arg(QDir::toNativeSeparators(path)));
        return;
    }
    m_sheet = sheet;
    m_sheetEdit->setText(QDir::toNativeSeparators(path));
    // same name, replaces the game's
    m_spriteEdit->setText(QFileInfo(path).completeBaseName());
    showSheetCells();
}

void HudSpriteDialog::showSheetCells()
{
    {
        const QSignalBlocker block(m_cellsCombo);
        m_cellsCombo->clear();
        m_cellsCombo->setEnabled(!m_sheet.isNull());
        m_sheetPreview->setVisible(!m_sheet.isNull());
        if (!m_sheet.isNull())
        {
            // cell pairs; first free pair selected
            const std::vector<bool> used = hudicon::UsedCells(m_sheet);
            int first = -1;
            for (int row = 0; row + 1 < static_cast<int>(used.size()); ++row)
            {
                const bool free = !used[row] && !used[row + 1];
                m_cellsCombo->addItem(free ? tr("%1 and %2, free").arg(row + 1).arg(row + 2) : tr("%1 and %2, over what is there").arg(row + 1).arg(row + 2),
                                      row);
                if (free && first < 0) first = row;
            }
            m_cellsCombo->setCurrentIndex(std::max(0, first));
        }
    }
    scheduleRender();
}

int HudSpriteDialog::sheetRow() const
{
    return m_sheet.isNull() ? 0 : std::max(0, m_cellsCombo->currentData().toInt());
}

QImage HudSpriteDialog::sheetToWrite() const
{
    const QImage normal = hudicon::MakeCell(m_weapon, look(), false), selected = hudicon::MakeCell(m_weapon, look(), true);
    if (m_sheet.isNull()) return hudicon::MakeSheet({ normal, selected });
    QImage sheet = m_sheet.copy();
    QPainter painter(&sheet);
    painter.drawImage(0, sheetRow() * hudicon::kCellHeight, normal);
    painter.drawImage(0, (sheetRow() + 1) * hudicon::kCellHeight, selected);
    return sheet;
}

void HudSpriteDialog::save()
{
    if (m_weapon.isNull())
    {
        m_status->setText(tr("Open a weapon's model first."));
        return;
    }
    const QString sprite = m_spriteEdit->text().trimmed().isEmpty() ? m_spriteEdit->placeholderText() : m_spriteEdit->text().trimmed();
    const QString weapon = m_weaponEdit->text().trimmed().isEmpty() ? m_weaponEdit->placeholderText() : m_weaponEdit->text().trimmed();
    const QDir folder(outputFolder());
    QDir().mkpath(folder.path());
    const QImage sheet = sheetToWrite();
    const int row = sheetRow();
    const QString spritePath = folder.filePath(sprite + QStringLiteral(".spr"));
    if (!hudicon::WriteSprite(sheet, spritePath.toStdString()))
    {
        m_status->setText(tr("Could not write %1").arg(QDir::toNativeSeparators(spritePath)));
        return;
    }
    sheet.save(folder.filePath(sprite + QStringLiteral(".png")));
    QFile text(folder.filePath(weapon + QStringLiteral(".txt")));
    if (text.open(QIODevice::WriteOnly | QIODevice::Truncate)) text.write(hudicon::HudText(sprite, row).toUtf8());
    if (m_sheet.isNull())
        m_status->setText(tr("Written: %1 and %2.txt. Copy both into sprites/ of the game.").arg(QDir::toNativeSeparators(spritePath), weapon));
    else
    {
        m_status->setText(tr("Written: %1, the weapon in its cells %2 and %3, and %4.txt. Copy both into sprites/ of the game.")
                              .arg(QDir::toNativeSeparators(spritePath))
                              .arg(row + 1)
                              .arg(row + 2)
                              .arg(weapon));
        // next save takes the next free cells
        m_sheet = sheet;
        showSheetCells();
    }
    VortigauntLog::Vortigaunt_Printf(QStringLiteral("^2HUD sprite written: %1^7").arg(QDir::toNativeSeparators(spritePath)));
}
