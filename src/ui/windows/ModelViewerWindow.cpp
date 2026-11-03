// Copyright (C) 2026 iQuit (https://github.com/iQuitt)
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#include "ModelViewerWindow.h"

#include "core/VortigauntLog.h"
#include "core/mdl/StudioModel.h"

#include <QApplication>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDir>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QCloseEvent>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMimeData>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSet>
#include <QSoundEffect>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStyle>
#include <QTabBar>
#include <QTimer>
#include <QUndoCommand>
#include <QUndoGroup>
#include <QUndoStack>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <iterator>

namespace
{
constexpr int kPanelHeight = 200;
constexpr int kLayoutVersion = 1;
const char* const kLayoutKey = "ModelViewer/layout";
constexpr int kBlendSteps = 100;
constexpr int kControllerSteps = 200;
constexpr int kSpeedSteps = 100;  // slider steps for 1x
constexpr int kFrameSteps = 100;  // slider steps for one frame
constexpr int kRefreshMs = 16;  // timeline refresh interval
constexpr double kDefaultFov[3] = { 65.0, 74.0, 65.0 };
constexpr int kCameras[3] = { StudioModelView::ArcBall, StudioModelView::FirstPerson, StudioModelView::FreeLook };

// model flags, in bit order
const char* const kModelFlags[] = { QT_TRANSLATE_NOOP("ModelViewerWindow", "Rocket Trail"),   QT_TRANSLATE_NOOP("ModelViewerWindow", "Grenade Smoke"),
                                    QT_TRANSLATE_NOOP("ModelViewerWindow", "Gib Blood"),      QT_TRANSLATE_NOOP("ModelViewerWindow", "Model Rotate"),
                                    QT_TRANSLATE_NOOP("ModelViewerWindow", "Green Trail"),    QT_TRANSLATE_NOOP("ModelViewerWindow", "Zombie Blood"),
                                    QT_TRANSLATE_NOOP("ModelViewerWindow", "Orange Trail"),   QT_TRANSLATE_NOOP("ModelViewerWindow", "Purple Trail"),
                                    QT_TRANSLATE_NOOP("ModelViewerWindow", "No Shade Light"), QT_TRANSLATE_NOOP("ModelViewerWindow", "Hitbox Collision"),
                                    QT_TRANSLATE_NOOP("ModelViewerWindow", "Force Skylight") };

void readOnly(QAbstractSpinBox* spin)
{
    spin->setReadOnly(true);
    spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
}

// placeholder control
void unavailable(QWidget* widget)
{
    widget->setEnabled(false);
}

QDoubleSpinBox* doubleSpin(double low, double high, int decimals = 2)
{
    auto* spin = new QDoubleSpinBox();
    spin->setRange(low, high);
    spin->setDecimals(decimals);
    return spin;
}

QFrame* separator()
{
    auto* line = new QFrame();
    line->setFrameShape(QFrame::VLine);
    line->setFrameShadow(QFrame::Sunken);
    return line;
}

QLabel* number()
{
    auto* label = new QLabel(QStringLiteral("0"));
    label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return label;
}

// name followed by its index
QString indexed(const std::string& name, int index)
{
    return QStringLiteral("%1 (%2)").arg(QString::fromStdString(name)).arg(index);
}

// compact margins to fit the panels under the view
void tighten(QWidget* panel)
{
    if (QLayout* top = panel->layout())
        top->setContentsMargins(4, 4, 4, 4);
    for (QLayout* layout : panel->findChildren<QLayout*>())
        layout->setSpacing(3);
    for (QGroupBox* box : panel->findChildren<QGroupBox*>())
        if (QLayout* inner = box->layout()) inner->setContentsMargins(4, 4, 4, 4);
}

template <typename List, typename Name>
void fillCombo(QComboBox* combo, const List& items, Name name)
{
    for (size_t i = 0; i < items.size(); ++i) combo->addItem(name(i));
}

QString qcNumber(float value)
{
    return QString::number(value, 'f', 6).remove(QRegularExpression(QStringLiteral("\\.?0+$")));
}

// set while filling panels so their changes are not taken as edits
struct Filling
{
    explicit Filling(bool& flag) : flag(flag), was(flag) { flag = true; }
    ~Filling() { flag = was; }
    bool& flag;
    bool was;
};

// set only if different, so the cursor stays
void showText(QLineEdit* edit, const QString& text)
{
    if (edit->text() != text) edit->setText(text);
}

}

bool ModelViewerWindow::textureFromImage(const QImage& source, StudioTexture& texture)
{
    if (source.isNull()) return false;
    const QImage image = source.format() == QImage::Format_Indexed8 ? source : source.convertToFormat(QImage::Format_Indexed8, Qt::ThresholdDither);
    const QList<QRgb> colours = image.colorTable();
    if (colours.isEmpty() || colours.size() > 256) return false;
    texture.width = image.width();
    texture.height = image.height();
    texture.pixels.resize(static_cast<size_t>(image.width()) * image.height());
    for (int y = 0; y < image.height(); ++y)
        std::copy_n(image.constScanLine(y), image.width(), texture.pixels.begin() + static_cast<size_t>(y) * image.width());
    texture.palette.assign(768, 0);
    for (int i = 0; i < colours.size(); ++i)
    {
        texture.palette[i * 3] = static_cast<uint8_t>(qRed(colours[i]));
        texture.palette[i * 3 + 1] = static_cast<uint8_t>(qGreen(colours[i]));
        texture.palette[i * 3 + 2] = static_cast<uint8_t>(qBlue(colours[i]));
    }
    return true;
}

namespace
{
QString textureLabel(const StudioTexture& texture)
{
    return QStringLiteral("%1 (%2 x %3)").arg(QString::fromStdString(texture.name)).arg(texture.width).arg(texture.height);
}

QStringList modelPaths(const QMimeData* mime)
{
    QStringList paths;
    if (!mime->hasUrls()) return paths;
    for (const QUrl& url : mime->urls())
        if (url.isLocalFile() && QFileInfo(url.toLocalFile()).suffix().compare(QLatin1String("mdl"), Qt::CaseInsensitive) == 0)
            paths << url.toLocalFile();
    return paths;
}
}

// undo step: the model before and after
class ModelEdit : public QUndoCommand
{
public:
    ModelEdit(ModelViewerWindow* window, QUndoStack* stack, const QString& text, const QString& key, std::shared_ptr<StudioModel> before,
              std::shared_ptr<StudioModel> after)
        : QUndoCommand(text), m_window(window), m_stack(stack), m_key(key), m_before(std::move(before)), m_after(std::move(after))
    {
    }
    void redo() override { m_window->setModel(m_stack, m_after); }
    void undo() override { m_window->setModel(m_stack, m_before); }
    int id() const override { return m_key.isEmpty() ? -1 : 1; }
    bool mergeWith(const QUndoCommand* other) override
    {
        // spin box steps merge into one change
        const auto* next = static_cast<const ModelEdit*>(other);
        if (next->m_key != m_key) return false;
        m_after = next->m_after;
        return true;
    }

private:
    ModelViewerWindow* m_window;
    QUndoStack* m_stack;
    QString m_key;
    std::shared_ptr<StudioModel> m_before, m_after;
};

void ModelViewerWindow::VectorEdit::set(const aiVector3D& value) const
{
    const float values[3] = { value.x, value.y, value.z };
    for (int i = 0; i < 3; ++i)
    {
        const QSignalBlocker block(axis[i]);
        axis[i]->setValue(values[i]);
    }
}

aiVector3D ModelViewerWindow::VectorEdit::value() const
{
    return aiVector3D(static_cast<float>(axis[0]->value()), static_cast<float>(axis[1]->value()), static_cast<float>(axis[2]->value()));
}

ModelViewerWindow::VectorEdit ModelViewerWindow::vectorEdit(bool editable, double range)
{
    VectorEdit edit;
    edit.widget = new QWidget();
    auto* layout = new QHBoxLayout(edit.widget);
    layout->setContentsMargins(0, 0, 0, 0);
    const char* names[] = { "X", "Y", "Z" };
    for (int i = 0; i < 3; ++i)
    {
        layout->addWidget(new QLabel(QLatin1String(names[i])));
        edit.axis[i] = doubleSpin(-range, range);
        if (!editable) readOnly(edit.axis[i]);
        layout->addWidget(edit.axis[i], 1);
    }
    return edit;
}

ModelViewerWindow::ModelViewerWindow(QWidget* parent)
    : QMainWindow(parent, Qt::Window)
{
    resize(1200, 850);
    setAcceptDrops(true);
    auto* central = new QWidget();
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    m_tabs = new QTabBar();
    m_tabs->setObjectName(QStringLiteral("tabs"));
    m_tabs->setTabsClosable(true);
    m_tabs->setExpanding(false);
    m_tabs->setDocumentMode(true);
    layout->addWidget(m_tabs);

    m_edit = new QMainWindow();
    m_edit->setWindowFlags(Qt::Widget);
    m_edit->setDockOptions(QMainWindow::AnimatedDocks | QMainWindow::AllowTabbedDocks);
    m_edit->setTabPosition(Qt::BottomDockWidgetArea, QTabWidget::North);
    m_edit->setDocumentMode(true);
    m_edit->setCentralWidget(buildView());
    layout->addWidget(m_edit, 1);
    layout->addWidget(buildTimeline());
    setCentralWidget(central);

    buildMenus();
    QDockWidget* first = addPanel(QT_TR_NOOP("Cameras"), buildCameras());
    addPanel(QT_TR_NOOP("Scene"), buildScene(), first);
    m_displayDock = addPanel(QT_TR_NOOP("Model Display"), buildDisplay(), first);
    addPanel(QT_TR_NOOP("Sequences"), buildSequences(), first);
    addPanel(QT_TR_NOOP("Body Parts"), buildBody(), first);
    m_texturesDock = addPanel(QT_TR_NOOP("Textures"), buildTextures(), first);
    addPanel(QT_TR_NOOP("Model Data"), buildModelData(), first);
    addPanel(QT_TR_NOOP("Bones"), buildBones(), first);
    addPanel(QT_TR_NOOP("Bone Controllers"), buildControllers(), first);
    addPanel(QT_TR_NOOP("Attachments"), buildAttachments(), first);
    addPanel(QT_TR_NOOP("Hitboxes"), buildHitboxes(), first);
    addPanel(QT_TR_NOOP("Transformation"), buildTransform(), first);
    addPanel(QT_TR_NOOP("Console"), buildConsole(), first, true);
    m_displayDock->raise();
    m_edit->resizeDocks({ first }, { kPanelHeight }, Qt::Vertical);
    m_defaultLayout = m_edit->saveState(kLayoutVersion);
    connect(m_texturesDock, &QDockWidget::visibilityChanged, this, [this](bool visible) { m_sceneTabs->setCurrentIndex(visible ? 1 : 0); });

    connect(m_tabs, &QTabBar::currentChanged, this, &ModelViewerWindow::showAsset);
    connect(m_tabs, &QTabBar::tabCloseRequested, this, &ModelViewerWindow::closeAsset);
    auto* timer = new QTimer(this);
    timer->setInterval(kRefreshMs);
    timer->setTimerType(Qt::PreciseTimer);
    connect(timer, &QTimer::timeout, this, &ModelViewerWindow::refresh);
    timer->start();
    connectEdits();
    applyCamera();
    applyDisplay();
    showAsset(-1);
}

ModelViewerWindow::~ModelViewerWindow()
{
    // save the layout only if it was loaded
    if (m_layoutRestored)
        QSettings(QStringLiteral("Vortigaunt"), QStringLiteral("VortigauntTool")).setValue(QLatin1String(kLayoutKey), m_edit->saveState(kLayoutVersion));
}

void ModelViewerWindow::resetLayout()
{
    m_edit->restoreState(m_defaultLayout, kLayoutVersion);
    for (QDockWidget* dock : m_edit->findChildren<QDockWidget*>()) dock->show();
    m_displayDock->raise();
}

void ModelViewerWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
    if (m_layoutRestored || !m_remembersLayout || window()->testAttribute(Qt::WA_DontShowOnScreen))
        return;
    m_layoutRestored = true;
    const QByteArray kept = QSettings(QStringLiteral("Vortigaunt"), QStringLiteral("VortigauntTool")).value(QLatin1String(kLayoutKey)).toByteArray();
    if (!kept.isEmpty())
        m_edit->restoreState(kept, kLayoutVersion);
}

QDockWidget* ModelViewerWindow::addPanel(const char* title, QWidget* panel, QDockWidget* tabWith, bool fill)
{
    auto* dock = new QDockWidget(tr(title), m_edit);
    dock->setObjectName(QLatin1String(title));
    tighten(panel);
    if (tabWith && !fill)
    {
        // sized to its controls
        auto* packed = new QWidget();
        auto* row = new QHBoxLayout(packed);
        row->setContentsMargins(0, 0, 0, 0);
        row->addWidget(panel);
        row->addStretch(1);
        panel = packed;
    }
    dock->setWidget(panel);
    dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
    m_edit->addDockWidget(Qt::BottomDockWidgetArea, dock, Qt::Horizontal);
    if (tabWith)
        m_edit->tabifyDockWidget(tabWith, dock);
    // docked elsewhere: the area shrinks to fit; at a side, parts stack vertically
    QWidget* inner = dock->widget() != panel ? panel : nullptr;
    connect(dock, &QDockWidget::dockLocationChanged, this, [this, dock, inner](Qt::DockWidgetArea area) {
        const bool column = area == Qt::LeftDockWidgetArea || area == Qt::RightDockWidgetArea;
        for (QWidget* widget : { dock->widget(), inner })
            if (auto* box = widget ? qobject_cast<QBoxLayout*>(widget->layout()) : nullptr)
                box->setDirection(column ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
        if (area == Qt::LeftDockWidgetArea || area == Qt::RightDockWidgetArea)
            m_edit->resizeDocks({ dock }, { 1 }, Qt::Horizontal);
        else if (area != Qt::NoDockWidgetArea)
            m_edit->resizeDocks({ dock }, { 1 }, Qt::Vertical);
    });
    m_panelMenu->addAction(dock->toggleViewAction());
    return dock;
}

QWidget* ModelViewerWindow::buildView()
{
    auto* area = new QWidget();
    auto* layout = new QVBoxLayout(area);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    m_sceneStack = new QStackedWidget();
    m_view = new StudioModelView();
    m_view->placeholder = tr("Open a model.");
    m_view->setHoldUnlooped(true);
    m_sceneStack->addWidget(m_view);
    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    m_textureImage = new QLabel();
    m_textureImage->setObjectName(QStringLiteral("textureImage"));
    m_textureImage->setAlignment(Qt::AlignCenter);
    scroll->setWidget(m_textureImage);
    m_sceneStack->addWidget(scroll);
    layout->addWidget(m_sceneStack, 1);

    m_controlsBar = new QWidget();
    auto* bar = new QHBoxLayout(m_controlsBar);
    bar->setContentsMargins(6, 2, 6, 2);
    bar->addWidget(new QLabel(tr("FPS:")));
    // fixed widths: a resizing label relayouts and repaints the window
    m_fpsLabel = number();
    m_fpsLabel->setFixedWidth(m_fpsLabel->fontMetrics().horizontalAdvance(QStringLiteral("00000")));
    bar->addWidget(m_fpsLabel);
    bar->addWidget(separator());
    bar->addWidget(new QLabel(tr("Drawn Polygons:")));
    m_polygonsLabel = number();
    m_polygonsLabel->setFixedWidth(m_polygonsLabel->fontMetrics().horizontalAdvance(QStringLiteral("00000000")));
    bar->addWidget(m_polygonsLabel);
    bar->addWidget(separator());
    m_sceneTabs = new QTabBar();
    m_sceneTabs->setObjectName(QStringLiteral("scenes"));
    m_sceneTabs->setDrawBase(false);
    m_sceneTabs->addTab(tr("Scene"));
    m_sceneTabs->addTab(tr("Texture"));
    bar->addWidget(m_sceneTabs);
    bar->addWidget(new QLabel(tr("Pose")));
    m_poseCombo = new QComboBox();
    m_poseCombo->setObjectName(QStringLiteral("pose"));
    m_poseCombo->addItems({ tr("Sequences"), tr("Skeleton") });
    bar->addWidget(m_poseCombo);
    bar->addStretch(1);
    layout->addWidget(m_controlsBar);

    connect(m_sceneTabs, &QTabBar::currentChanged, this, [this](int scene) {
        m_sceneStack->setCurrentIndex(scene);
        showTexture();
    });
    connect(m_poseCombo, &QComboBox::currentIndexChanged, this, [this](int pose) { m_view->setRestPose(0, pose == 1); });
    return area;
}

QWidget* ModelViewerWindow::buildTimeline()
{
    m_timeline = new QWidget();
    m_timeline->setObjectName(QStringLiteral("timeline"));
    auto* row = new QHBoxLayout(m_timeline);
    row->setContentsMargins(4, 2, 4, 4);

    auto* frameBox = new QGroupBox();
    auto* frameRow = new QHBoxLayout(frameBox);
    m_frameSlider = new QSlider(Qt::Horizontal);
    m_frameSlider->setObjectName(QStringLiteral("frame"));
    frameRow->addWidget(m_frameSlider, 1);
    frameRow->addWidget(new QLabel(tr("Frame")));
    m_frameSpin = doubleSpin(0.0, 100.0);
    frameRow->addWidget(m_frameSpin);
    row->addWidget(frameBox, 1);

    auto* controlsBox = new QGroupBox();
    auto* controlsRow = new QHBoxLayout(controlsBox);
    auto button = [this, controlsRow](QStyle::StandardPixmap icon, const QString& tip) {
        auto* pushed = new QPushButton(style()->standardIcon(icon), QString());
        pushed->setToolTip(tip);
        controlsRow->addWidget(pushed);
        return pushed;
    };
    QPushButton* restart = button(QStyle::SP_BrowserReload, tr("Restart Sequence"));
    QPushButton* previous = button(QStyle::SP_ArrowLeft, tr("Previous Frame"));
    QPushButton* earlier = button(QStyle::SP_MediaSeekBackward, tr("Skip 5 Frames Earlier"));
    m_playButton = button(QStyle::SP_MediaPause, tr("Play/Pause"));
    m_playButton->setObjectName(QStringLiteral("play"));
    m_playButton->setCheckable(true);
    m_playButton->setChecked(true);
    QPushButton* later = button(QStyle::SP_MediaSeekForward, tr("Skip 5 Frames Later"));
    QPushButton* next = button(QStyle::SP_ArrowRight, tr("Next Frame"));
    row->addWidget(controlsBox);

    auto* speedBox = new QGroupBox();
    auto* speedRow = new QHBoxLayout(speedBox);
    speedRow->addWidget(new QLabel(tr("Speed")));
    m_speedSlider = new QSlider(Qt::Horizontal);
    m_speedSlider->setRange(0, 10 * kSpeedSteps);
    m_speedSlider->setValue(kSpeedSteps);
    m_speedSlider->setFixedWidth(120);
    speedRow->addWidget(m_speedSlider);
    m_speedSpin = doubleSpin(0.0, 10.0);
    m_speedSpin->setSingleStep(0.1);
    m_speedSpin->setValue(1.0);
    speedRow->addWidget(m_speedSpin);
    row->addWidget(speedBox);

    connect(restart, &QPushButton::clicked, this, [this]() {
        m_view->setFramePosition(0.0f);
        refresh();
    });
    connect(previous, &QPushButton::clicked, this, [this]() { stepFrames(-1); });
    connect(earlier, &QPushButton::clicked, this, [this]() { stepFrames(-5); });
    connect(later, &QPushButton::clicked, this, [this]() { stepFrames(5); });
    connect(next, &QPushButton::clicked, this, [this]() { stepFrames(1); });
    connect(m_playButton, &QPushButton::toggled, this, [this](bool playing) {
        m_view->setPlaying(playing);
        m_playButton->setIcon(style()->standardIcon(playing ? QStyle::SP_MediaPause : QStyle::SP_MediaPlay));
    });
    connect(m_frameSlider, &QSlider::sliderPressed, this, [this]() { m_playButton->setChecked(false); });
    connect(m_frameSlider, &QSlider::valueChanged, this, [this](int steps) {
        if (m_frameSlider->isSliderDown()) m_view->setFramePosition(steps / static_cast<float>(kFrameSteps));
    });
    connect(m_frameSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double frame) {
        m_playButton->setChecked(false);
        m_view->setFramePosition(static_cast<float>(frame));
    });
    connect(m_speedSlider, &QSlider::valueChanged, this, [this](int steps) { m_speedSpin->setValue(steps / static_cast<double>(kSpeedSteps)); });
    connect(m_speedSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double speed) {
        const QSignalBlocker block(m_speedSlider);
        m_speedSlider->setValue(static_cast<int>(std::lround(speed * kSpeedSteps)));
        m_view->setSpeed(static_cast<float>(speed));
    });
    return m_timeline;
}

void ModelViewerWindow::buildMenus()
{
    QMenu* file = menuBar()->addMenu(tr("File"));
    file->addAction(tr("Load..."), QKeySequence::Open, this, &ModelViewerWindow::openFiles);
    file->addAction(tr("Save"), QKeySequence::Save, this, &ModelViewerWindow::saveCurrent);
    file->addAction(tr("Save As..."), QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_S), this, &ModelViewerWindow::saveCurrentAs);
    file->addAction(tr("Close"), QKeySequence(Qt::CTRL | Qt::Key_W), this, [this]() {
        if (m_current >= 0) closeAsset(m_current);
    });
    file->addAction(tr("Close All"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_W), this, [this]() {
        while (!m_assets.empty()) closeAsset(0);
    });
    file->addSeparator();
    file->addAction(tr("Take Screenshot..."), QKeySequence(Qt::Key_F10), this, &ModelViewerWindow::saveScreenshot);

    QMenu* editMenu = menuBar()->addMenu(tr("Edit"));
    m_undoGroup = new QUndoGroup(this);
    QAction* undo = m_undoGroup->createUndoAction(this, tr("Undo"));
    undo->setObjectName(QStringLiteral("undo"));
    undo->setShortcut(QKeySequence::Undo);
    QAction* redo = m_undoGroup->createRedoAction(this, tr("Redo"));
    redo->setObjectName(QStringLiteral("redo"));
    redo->setShortcut(QKeySequence::Redo);
    editMenu->addAction(undo);
    editMenu->addAction(redo);

    QMenu* menu = menuBar()->addMenu(tr("Asset"));
    m_panelMenu = menu->addMenu(tr("Dock Widgets"));
    menu->addAction(tr("Reset Dock Widgets"), this, &ModelViewerWindow::resetLayout);
    QAction* controls = menu->addAction(tr("Show Controls Bar"));
    controls->setCheckable(true);
    controls->setChecked(true);
    connect(controls, &QAction::toggled, this, [this](bool shown) { m_controlsBar->setVisible(shown); });
    QAction* timeline = menu->addAction(tr("Show Timeline"));
    timeline->setCheckable(true);
    timeline->setChecked(true);
    connect(timeline, &QAction::toggled, this, [this](bool shown) { m_timeline->setVisible(shown); });
    menu->addSeparator();
    auto turnCamera = [this](int step) { m_cameraCombo->setCurrentIndex((m_cameraCombo->currentIndex() + step + m_cameraCombo->count()) % m_cameraCombo->count()); };
    menu->addAction(tr("Previous Camera"), this, [turnCamera]() { turnCamera(-1); });
    menu->addAction(tr("Next Camera"), this, [turnCamera]() { turnCamera(1); });
    menu->addSeparator();
    QMenu* center = menu->addMenu(tr("Center View"));
    center->addAction(tr("Center On Positive X"), this, [this]() { centerOn(0.0f, 0.0f); });
    center->addAction(tr("Center On Negative X"), this, [this]() { centerOn(180.0f, 0.0f); });
    center->addAction(tr("Center On Positive Y"), this, [this]() { centerOn(90.0f, 0.0f); });
    center->addAction(tr("Center On Negative Y"), this, [this]() { centerOn(270.0f, 0.0f); });
    center->addAction(tr("Center On Positive Z"), this, [this]() { centerOn(0.0f, 89.0f); });
    center->addAction(tr("Center On Negative Z"), this, [this]() { centerOn(0.0f, -89.0f); });
    menu->addAction(tr("Save View"), this, [this]() {
        m_savedPlace = m_view->viewPlace();
        m_hasSavedPlace = true;
    });
    menu->addAction(tr("Restore View"), this, [this]() {
        if (m_hasSavedPlace) m_view->setViewPlace(m_savedPlace);
    });
    menu->addSeparator();
    m_firstPersonViewModels = menu->addAction(tr("First Person Camera For View Models"));
    m_firstPersonViewModels->setCheckable(true);
    m_firstPersonViewModels->setChecked(true);
    menu->addSeparator();
    menu->addAction(tr("Dump Model Info..."), this, &ModelViewerWindow::dumpInfo);

    QMenu* audio = menuBar()->addMenu(tr("Audio"));
    m_playSounds = audio->addAction(tr("Play Sounds"));
    m_playSounds->setObjectName(QStringLiteral("playSounds"));
    m_playSounds->setCheckable(true);
    m_playSounds->setChecked(true);
}

QWidget* ModelViewerWindow::buildCameras()
{
    auto* panel = new QWidget();
    auto* layout = new QVBoxLayout(panel);
    auto* top = new QHBoxLayout();
    m_cameraCombo = new QComboBox();
    m_cameraCombo->setObjectName(QStringLiteral("camera"));
    m_cameraCombo->addItems({ tr("Arc Ball"), tr("First Person"), tr("Free Look") });
    top->addWidget(m_cameraCombo);
    auto* previous = new QPushButton(tr("Previous"));
    auto* next = new QPushButton(tr("Next"));
    top->addWidget(previous);
    top->addStretch(1);
    top->addWidget(next);
    top->addStretch(1);
    layout->addLayout(top);
    m_cameraStack = new QStackedWidget();
    layout->addWidget(m_cameraStack, 1);

    auto fovRow = [this](QGridLayout* grid, int camera) {
        grid->addWidget(new QLabel(tr("Field Of View")), 0, 0);
        m_fovSpins[camera] = doubleSpin(1.0, 179.0);
        m_fovSpins[camera]->setValue(kDefaultFov[camera]);
        grid->addWidget(m_fovSpins[camera], 0, 1);
        auto* reset = new QPushButton(tr("Default"));
        grid->addWidget(reset, 0, 2);
        connect(reset, &QPushButton::clicked, this, [this, camera]() { m_fovSpins[camera]->setValue(kDefaultFov[camera]); });
        connect(m_fovSpins[camera], QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ModelViewerWindow::applyCamera);
    };
    auto angleRows = [this](QGridLayout* grid, int which) {
        grid->addWidget(new QLabel(tr("Pitch")), 1, 0);
        m_pitchSpins[which] = doubleSpin(-89.0, 89.0);
        grid->addWidget(m_pitchSpins[which], 1, 1, 1, 2);
        grid->addWidget(new QLabel(tr("Yaw")), 2, 0);
        m_yawSpins[which] = doubleSpin(0.0, 360.0);
        m_yawSpins[which]->setWrapping(true);
        grid->addWidget(m_yawSpins[which], 2, 1, 1, 2);
        for (QDoubleSpinBox* spin : { m_pitchSpins[which], m_yawSpins[which] })
            connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ModelViewerWindow::applyCameraPlace);
    };

    auto* arc = new QWidget();
    auto* grid = new QGridLayout(arc);
    fovRow(grid, 0);
    angleRows(grid, 0);
    grid->addWidget(new QLabel(tr("Center")), 3, 0);
    m_cameraCenter = vectorEdit(true);
    grid->addWidget(m_cameraCenter.widget, 3, 1, 1, 2);
    grid->addWidget(new QLabel(tr("Distance")), 4, 0);
    m_distanceSpin = doubleSpin(1.0, 100000.0);
    grid->addWidget(m_distanceSpin, 4, 1, 1, 2);
    grid->addWidget(new QLabel(tr("Projection Mode")), 5, 0);
    m_projectionCombo = new QComboBox();
    m_projectionCombo->addItems({ tr("Perspective"), tr("Orthographic") });
    grid->addWidget(m_projectionCombo, 5, 1, 1, 2);
    grid->setRowStretch(6, 1);
    m_cameraStack->addWidget(arc);

    auto* first = new QWidget();
    grid = new QGridLayout(first);
    fovRow(grid, 1);
    grid->setRowStretch(1, 1);
    m_cameraStack->addWidget(first);

    auto* freeLook = new QWidget();
    grid = new QGridLayout(freeLook);
    fovRow(grid, 2);
    angleRows(grid, 1);
    grid->addWidget(new QLabel(tr("Origin")), 3, 0);
    m_freeOrigin = vectorEdit(true);
    grid->addWidget(m_freeOrigin.widget, 3, 1, 1, 2);
    grid->setRowStretch(4, 1);
    m_cameraStack->addWidget(freeLook);

    for (const VectorEdit* edit : { &m_cameraCenter, &m_freeOrigin })
        for (QDoubleSpinBox* spin : edit->axis) connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ModelViewerWindow::applyCameraPlace);
    connect(m_distanceSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ModelViewerWindow::applyCameraPlace);
    connect(m_projectionCombo, &QComboBox::currentIndexChanged, this, &ModelViewerWindow::applyCamera);
    connect(m_cameraCombo, &QComboBox::currentIndexChanged, this, &ModelViewerWindow::applyCamera);
    connect(previous, &QPushButton::clicked, this, [this]() { m_cameraCombo->setCurrentIndex((m_cameraCombo->currentIndex() + 2) % 3); });
    connect(next, &QPushButton::clicked, this, [this]() { m_cameraCombo->setCurrentIndex((m_cameraCombo->currentIndex() + 1) % 3); });
    return panel;
}

QWidget* ModelViewerWindow::buildScene()
{
    auto* panel = new QWidget();
    auto* row = new QHBoxLayout(panel);
    auto* objects = new QListWidget();
    objects->addItems({ tr("Model"), tr("Ground"), tr("Background") });
    objects->setMaximumWidth(160);
    row->addWidget(objects);
    auto* stack = new QStackedWidget();
    row->addWidget(stack, 1);

    auto* model = new QWidget();
    auto* layout = new QVBoxLayout(model);
    auto* origin = new QHBoxLayout();
    origin->addWidget(new QLabel(tr("Origin")));
    m_origin = vectorEdit(true);
    origin->addWidget(m_origin.widget, 1);
    layout->addLayout(origin);
    auto* toOrigin = new QPushButton(tr("Center On World Origin"));
    auto* toGround = new QPushButton(tr("Align On Ground"));
    layout->addWidget(toOrigin);
    layout->addWidget(toGround);
    layout->addStretch(1);
    stack->addWidget(model);

    const StudioModelView::Scene start;
    auto* ground = new QWidget();
    auto* grid = new QGridLayout(ground);
    m_groundPath = new QLineEdit();
    m_groundPath->setObjectName(QStringLiteral("groundTexture"));
    auto* browseGround = new QPushButton(QStringLiteral("..."));
    grid->addWidget(m_groundPath, 0, 0, 1, 2);
    grid->addWidget(browseGround, 0, 2);
    m_showGround = new QCheckBox(tr("Show Ground"));
    grid->addWidget(m_showGround, 1, 0);
    m_groundTextured = new QCheckBox(tr("Enable Texture"));
    grid->addWidget(m_groundTextured, 1, 1, 1, 2);
    m_mirrorGround = new QCheckBox(tr("Mirror Model"));
    grid->addWidget(m_mirrorGround, 2, 0);
    m_tiling = new QGroupBox(tr("Enable Tiling"));
    m_tiling->setCheckable(true);
    m_tiling->setChecked(start.tiling);
    auto* tilingRow = new QHBoxLayout(m_tiling);
    tilingRow->addWidget(new QLabel(tr("Tile Size")));
    m_tileSize = new QSpinBox();
    m_tileSize->setRange(1, 1024);
    m_tileSize->setValue(start.tileSize);
    tilingRow->addWidget(m_tileSize);
    grid->addWidget(m_tiling, 2, 1, 1, 2);
    auto* groundOrigin = new QHBoxLayout();
    groundOrigin->addWidget(new QLabel(tr("Origin")));
    m_groundOrigin = vectorEdit(true);
    groundOrigin->addWidget(m_groundOrigin.widget, 1);
    grid->addLayout(groundOrigin, 3, 0, 1, 3);
    grid->addWidget(new QLabel(tr("Ground Length")), 4, 0);
    m_groundLength = doubleSpin(16.0, 16384.0, 0);
    m_groundLength->setValue(start.groundLength);
    grid->addWidget(m_groundLength, 4, 1, 1, 2);
    grid->setRowStretch(5, 1);
    stack->addWidget(ground);

    auto* background = new QWidget();
    grid = new QGridLayout(background);
    m_backgroundPath = new QLineEdit();
    m_backgroundPath->setObjectName(QStringLiteral("backgroundTexture"));
    auto* browseBackground = new QPushButton(QStringLiteral("..."));
    grid->addWidget(m_backgroundPath, 0, 0);
    grid->addWidget(browseBackground, 0, 1);
    m_showBackground = new QCheckBox(tr("Show Background"));
    grid->addWidget(m_showBackground, 1, 0, 1, 2);
    auto* colour = new QPushButton(tr("Background colour..."));
    grid->addWidget(colour, 2, 0, 1, 2, Qt::AlignLeft);
    grid->setRowStretch(3, 1);
    stack->addWidget(background);

    connect(objects, &QListWidget::currentRowChanged, stack, &QStackedWidget::setCurrentIndex);
    objects->setCurrentRow(0);
    for (QDoubleSpinBox* spin : m_origin.axis) connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ModelViewerWindow::applyPlacement);
    connect(toOrigin, &QPushButton::clicked, this, [this]() {
        m_origin.set(aiVector3D());
        applyPlacement();
    });
    connect(toGround, &QPushButton::clicked, this, [this]() {
        QVector3D low, high;
        if (!m_view->bounds(low, high, 0)) return;
        aiVector3D place = m_origin.value();
        place.z -= low.z() - m_groundOrigin.value().z;
        m_origin.set(place);
        applyPlacement();
    });
    connect(m_showGround, &QCheckBox::toggled, this, &ModelViewerWindow::applyDisplay);
    // choosing an image enables it
    auto browse = [this](QLineEdit* path, QCheckBox* use) {
        const QString picked = QFileDialog::getOpenFileName(this, tr("Open an image"), path->text(), tr("Images (*.png *.bmp *.jpg *.tga)"));
        if (picked.isEmpty()) return;
        path->setText(picked);
        use->setChecked(true);
        applyScene();
    };
    connect(browseGround, &QPushButton::clicked, this, [browse, this]() { browse(m_groundPath, m_groundTextured); });
    connect(browseBackground, &QPushButton::clicked, this, [browse, this]() { browse(m_backgroundPath, m_showBackground); });
    for (QLineEdit* path : { m_groundPath, m_backgroundPath }) connect(path, &QLineEdit::editingFinished, this, &ModelViewerWindow::applyScene);
    for (QCheckBox* box : { m_groundTextured, m_mirrorGround, m_showBackground }) connect(box, &QCheckBox::toggled, this, &ModelViewerWindow::applyScene);
    connect(m_tiling, &QGroupBox::toggled, this, &ModelViewerWindow::applyScene);
    connect(m_tileSize, QOverload<int>::of(&QSpinBox::valueChanged), this, &ModelViewerWindow::applyScene);
    connect(m_groundLength, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ModelViewerWindow::applyScene);
    for (QDoubleSpinBox* spin : m_groundOrigin.axis) connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ModelViewerWindow::applyScene);
    connect(colour, &QPushButton::clicked, this, [this]() {
        const QColor picked = QColorDialog::getColor(Qt::black, this, tr("Background colour"));
        if (picked.isValid()) m_view->setBackground(picked);
    });
    return panel;
}

void ModelViewerWindow::applyScene()
{
    StudioModelView::Scene scene = m_view->scene();
    // reload the image only when the file changes
    auto picture = [](QLineEdit* path, QImage& image) {
        const QString file = path->text();
        if (path->property("loaded").toString() == file) return;
        path->setProperty("loaded", file);
        image = file.isEmpty() ? QImage() : QImage(file);
        if (!file.isEmpty() && image.isNull())
            VortigauntLog::Vortigaunt_Printf(QStringLiteral("ERROR: Could not read the image %1").arg(QDir::toNativeSeparators(file)));
    };
    picture(m_groundPath, scene.groundTexture);
    picture(m_backgroundPath, scene.background);
    scene.groundTextured = m_groundTextured->isChecked();
    scene.mirror = m_mirrorGround->isChecked();
    scene.tiling = m_tiling->isChecked();
    scene.tileSize = m_tileSize->value();
    scene.groundLength = static_cast<float>(m_groundLength->value());
    const aiVector3D middle = m_groundOrigin.value();
    scene.groundOrigin = QVector3D(middle.x, middle.y, middle.z);
    scene.showBackground = m_showBackground->isChecked();
    m_view->setScene(scene);
}

QWidget* ModelViewerWindow::buildDisplay()
{
    using Display = StudioModelView::Display;
    auto* panel = new QWidget();
    auto* row = new QHBoxLayout(panel);
    auto* grid = new QGridLayout();
    grid->addWidget(new QLabel(tr("Render Mode")), 0, 0, 1, 2);
    m_modeCombo = new QComboBox();
    m_modeCombo->setObjectName(QStringLiteral("mode"));
    m_modeCombo->addItems({ tr("Wireframe"), tr("Flat Shaded"), tr("Smooth Shaded"), tr("Texture Shaded") });
    m_modeCombo->setCurrentIndex(3);
    grid->addWidget(m_modeCombo, 1, 0, 1, 2);
    grid->addWidget(new QLabel(tr("Opacity")), 2, 0);
    m_opacityLabel = new QLabel(QStringLiteral("100%"));
    grid->addWidget(m_opacityLabel, 2, 1, Qt::AlignRight);
    m_opacitySlider = new QSlider(Qt::Horizontal);
    m_opacitySlider->setRange(0, 100);
    m_opacitySlider->setValue(100);
    grid->addWidget(m_opacitySlider, 3, 0, 1, 2);
    row->addLayout(grid);
    row->addSpacing(12);

    const Display start;
    auto check = [this, &start](QVBoxLayout* column, const QString& text, bool Display::*member) {
        auto* box = new QCheckBox(text);
        column->addWidget(box);
        if (member)
        {
            box->setChecked(start.*member);
            m_displayChecks.emplace_back(box, member);
            connect(box, &QCheckBox::toggled, this, &ModelViewerWindow::applyDisplay);
        }
        else
            unavailable(box);
        return box;
    };
    auto* first = new QVBoxLayout();
    grid->addLayout(first, 4, 0, 1, 2);
    check(first, tr("Backface Culling"), &Display::backfaceCulling);
    check(first, tr("Wireframe Overlay"), &Display::wireframeOverlay);
    check(first, tr("Draw Shadows"), &Display::shadows);
    check(first, tr("Fix Shadow Z Fighting"), &Display::fixShadowZFighting);
    grid->setRowStretch(5, 1);

    auto* second = new QVBoxLayout();
    check(second, tr("Show Hitboxes"), &Display::hitboxes);
    check(second, tr("Show Bones"), &Display::bones);
    check(second, tr("Show Attachments"), &Display::attachments);
    check(second, tr("Show Eye Position"), &Display::eyePosition);
    check(second, tr("Show Bounding Box"), &Display::boundingBox);
    check(second, tr("Show Clipping Box"), &Display::clippingBox);
    check(second, tr("Show Normals"), &Display::normals);
    second->addStretch(1);
    row->addLayout(second);

    auto* third = new QVBoxLayout();
    check(third, tr("Show Axes"), &Display::axes);
    check(third, tr("Show Player Hitbox"), &Display::playerBox);
    check(third, tr("Mirror On X Axis"), &Display::mirrorX);
    check(third, tr("Mirror On Y Axis"), &Display::mirrorY);
    check(third, tr("Mirror On Z Axis"), &Display::mirrorZ);
    third->addStretch(1);
    row->addLayout(third);

    auto* fourth = new QVBoxLayout();
    check(fourth, tr("Show Crosshair"), &Display::crosshair);
    check(fourth, tr("Show Guidelines"), &Display::guidelines);
    check(fourth, tr("Show Off-screen Areas"), &Display::offscreenAreas);
    auto* aspect = new QGroupBox(tr("Aspect Ratio"));
    auto* aspectRow = new QHBoxLayout(aspect);
    m_aspectWidth = new QSpinBox();
    m_aspectHeight = new QSpinBox();
    for (QSpinBox* side : { m_aspectWidth, m_aspectHeight })
    {
        side->setRange(1, 100);
        connect(side, QOverload<int>::of(&QSpinBox::valueChanged), this, &ModelViewerWindow::applyDisplay);
    }
    m_aspectWidth->setValue(start.aspectWidth);
    m_aspectHeight->setValue(start.aspectHeight);
    aspectRow->addWidget(m_aspectWidth);
    aspectRow->addWidget(new QLabel(QStringLiteral("/")));
    aspectRow->addWidget(m_aspectHeight);
    fourth->addWidget(aspect);
    fourth->addStretch(1);
    row->addLayout(fourth);

    connect(m_modeCombo, &QComboBox::currentIndexChanged, this, &ModelViewerWindow::applyDisplay);
    connect(m_opacitySlider, &QSlider::valueChanged, this, [this](int percent) {
        m_opacityLabel->setText(QStringLiteral("%1%").arg(percent));
        applyDisplay();
    });
    return panel;
}

QWidget* ModelViewerWindow::buildSequences()
{
    auto* panel = new QWidget();
    auto* row = new QHBoxLayout(panel);
    auto* grid = new QGridLayout();
    m_sequenceCombo = new QComboBox();
    m_sequenceCombo->setObjectName(QStringLiteral("sequences"));
    grid->addWidget(m_sequenceCombo, 0, 0, 1, 2);
    grid->addWidget(new QLabel(tr("Looping Mode")), 1, 0);
    m_loopCombo = new QComboBox();
    m_loopCombo->addItems({ tr("Always Loop"), tr("Never Loop"), tr("Default") });
    m_loopCombo->setCurrentIndex(2);
    m_loopCombo->setToolTip(tr("Controls the viewer's looping mode. Default uses the sequence setting."));
    grid->addWidget(m_loopCombo, 1, 1);
    grid->addWidget(new QLabel(tr("Blends")), 2, 0);
    m_blendCount = number();
    grid->addWidget(m_blendCount, 2, 1);
    grid->addWidget(new QLabel(tr("Duration")), 3, 0);
    m_duration = number();
    grid->addWidget(m_duration, 3, 1);
    grid->addWidget(new QLabel(tr("Frames")), 4, 0);
    m_frameCount = number();
    grid->addWidget(m_frameCount, 4, 1);
    auto* blends = new QGroupBox(tr("Blends"));
    auto* blendGrid = new QGridLayout(blends);
    auto* blendMode = new QComboBox();
    blendMode->addItems({ tr("Standard"), tr("Counter-Strike") });
    unavailable(blendMode);
    blendGrid->addWidget(blendMode, 0, 0, 1, 3);
    blendGrid->addWidget(new QLabel(QStringLiteral("X")), 1, 0);
    m_blendSlider = new QSlider(Qt::Horizontal);
    m_blendSlider->setRange(0, kBlendSteps);
    m_blendSlider->setValue(kBlendSteps / 2);
    blendGrid->addWidget(m_blendSlider, 1, 1);
    m_blendSpin = doubleSpin(-360.0, 360.0);
    blendGrid->addWidget(m_blendSpin, 1, 2);
    blendGrid->addWidget(new QLabel(QStringLiteral("Y")), 2, 0);
    auto* blendY = new QSlider(Qt::Horizontal);
    auto* blendYSpin = doubleSpin(-360.0, 360.0);
    blendGrid->addWidget(blendY, 2, 1);
    blendGrid->addWidget(blendYSpin, 2, 2);
    unavailable(blendY);
    unavailable(blendYSpin);
    grid->addWidget(blends, 5, 0, 1, 2);
    grid->setRowStretch(6, 1);
    row->addLayout(grid, 1);

    auto* properties = new QGroupBox(tr("Properties"));
    grid = new QGridLayout(properties);
    m_isLooping = new QCheckBox(tr("Is Looping"));
    grid->addWidget(m_isLooping, 0, 0, 1, 2);
    grid->addWidget(new QLabel(tr("FPS")), 1, 0);
    m_sequenceFps = doubleSpin(0.0, 1000.0);
    m_sequenceFps->setObjectName(QStringLiteral("sequenceFps"));
    grid->addWidget(m_sequenceFps, 1, 1);
    grid->addWidget(new QLabel(tr("Activity")), 2, 0);
    m_activity = new QSpinBox();
    m_activity->setRange(0, 100000);
    m_activity->setSpecialValueText(tr("None (0)"));
    grid->addWidget(m_activity, 2, 1);
    grid->addWidget(new QLabel(tr("Activity Weight")), 3, 0);
    m_activityWeight = new QSpinBox();
    m_activityWeight->setRange(-100000, 100000);
    grid->addWidget(m_activityWeight, 3, 1);
    grid->addWidget(new QLabel(tr("Linear Movement")), 4, 0);
    m_linearMovement = vectorEdit(true);
    grid->addWidget(m_linearMovement.widget, 4, 1);
    grid->setRowStretch(5, 1);
    row->addWidget(properties, 1);

    auto* events = new QGroupBox(tr("Events"));
    grid = new QGridLayout(events);
    m_eventCombo = new QComboBox();
    grid->addWidget(m_eventCombo, 0, 0);
    auto* add = new QPushButton(QStringLiteral("+"));
    add->setObjectName(QStringLiteral("addEvent"));
    add->setToolTip(tr("Add New Event"));
    auto* remove = new QPushButton(QStringLiteral("-"));
    remove->setObjectName(QStringLiteral("removeEvent"));
    remove->setToolTip(tr("Remove Current Event"));
    for (QPushButton* button : { add, remove }) button->setMaximumWidth(30);
    grid->addWidget(add, 0, 1);
    grid->addWidget(remove, 0, 2);
    auto* data = new QGridLayout();
    data->addWidget(new QLabel(tr("Frame")), 0, 0);
    m_eventFrame = new QSpinBox();
    m_eventFrame->setRange(0, 100000);
    data->addWidget(m_eventFrame, 0, 1);
    data->addWidget(new QLabel(tr("Id")), 1, 0);
    m_eventId = new QSpinBox();
    m_eventId->setRange(-1000000, 1000000);
    data->addWidget(m_eventId, 1, 1);
    data->addWidget(new QLabel(tr("Options")), 2, 0);
    m_eventOptions = new QLineEdit();
    m_eventOptions->setMaxLength(63);
    data->addWidget(m_eventOptions, 2, 1);
    data->setRowStretch(3, 1);
    grid->addLayout(data, 1, 0, 1, 3);
    grid->setRowStretch(1, 1);
    row->addWidget(events, 1);

    connect(add, &QPushButton::clicked, this, [this]() {
        const int sequence = m_sequenceCombo->currentIndex();
        editModel(tr("Add New Event"), QString(), [sequence](StudioModel& model) {
            if (sequence >= 0 && sequence < static_cast<int>(model.sequences.size())) model.sequences[sequence].events.push_back({});
        });
        m_eventCombo->setCurrentIndex(m_eventCombo->count() - 1);
    });
    connect(remove, &QPushButton::clicked, this, [this]() {
        const int sequence = m_sequenceCombo->currentIndex(), event = m_eventCombo->currentIndex();
        editModel(tr("Remove Current Event"), QString(), [sequence, event](StudioModel& model) {
            if (sequence < 0 || sequence >= static_cast<int>(model.sequences.size())) return;
            std::vector<StudioEvent>& events = model.sequences[sequence].events;
            if (event >= 0 && event < static_cast<int>(events.size())) events.erase(events.begin() + event);
        });
    });
    connect(m_sequenceCombo, &QComboBox::currentIndexChanged, this, &ModelViewerWindow::showSequence);
    connect(m_eventCombo, &QComboBox::currentIndexChanged, this, &ModelViewerWindow::showSequenceEvent);
    connect(m_loopCombo, &QComboBox::currentIndexChanged, this, [this](int mode) {
        const int looping[] = { StudioModelView::AlwaysLoop, StudioModelView::NeverLoop, StudioModelView::SequenceLooping };
        m_view->setLooping(looping[std::clamp(mode, 0, 2)]);
    });
    auto aim = [this]() {
        const Asset* shown = asset();
        if (!shown || shown->sequence < 0 || shown->sequence >= static_cast<int>(shown->model->sequences.size())) return;
        const StudioSequence& sequence = shown->model->sequences[shown->sequence];
        const float place = m_blendSlider->value() / static_cast<float>(kBlendSteps);
        const float degrees = sequence.blendStart + (sequence.blendEnd - sequence.blendStart) * place;
        const QSignalBlocker block(m_blendSpin);
        m_blendSpin->setValue(degrees);
        m_view->setAimPitch(0, degrees);
    };
    connect(m_blendSlider, &QSlider::valueChanged, this, aim);
    connect(m_blendSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double degrees) {
        const Asset* shown = asset();
        if (!shown || shown->sequence < 0 || shown->sequence >= static_cast<int>(shown->model->sequences.size())) return;
        const StudioSequence& sequence = shown->model->sequences[shown->sequence];
        const float reach = sequence.blendEnd - sequence.blendStart;
        const QSignalBlocker block(m_blendSlider);
        if (reach != 0.0f) m_blendSlider->setValue(static_cast<int>(std::lround((degrees - sequence.blendStart) / reach * kBlendSteps)));
        m_view->setAimPitch(0, static_cast<float>(degrees));
    });
    return panel;
}

QWidget* ModelViewerWindow::buildBody()
{
    auto* panel = new QWidget();
    auto* grid = new QGridLayout(panel);
    auto label = [grid](const QString& text, int row) { grid->addWidget(new QLabel(text), row, 0); };
    label(tr("Part"), 0);
    m_partCombo = new QComboBox();
    grid->addWidget(m_partCombo, 0, 1);
    label(tr("Submodel"), 1);
    m_subModelCombo = new QComboBox();
    grid->addWidget(m_subModelCombo, 1, 1);
    label(tr("Skin"), 2);
    m_skinCombo = new QComboBox();
    m_skinCombo->setObjectName(QStringLiteral("skin"));
    grid->addWidget(m_skinCombo, 2, 1);
    label(tr("Name"), 3);
    m_subModelName = new QLineEdit();
    m_subModelName->setMaxLength(63);
    grid->addWidget(m_subModelName, 3, 1);
    label(tr("# Meshes"), 4);
    m_meshCount = number();
    grid->addWidget(m_meshCount, 4, 1);
    label(tr("# Vertices"), 5);
    m_vertexCount = number();
    grid->addWidget(m_vertexCount, 5, 1);
    label(tr("# Normals"), 6);
    m_normalCount = number();
    grid->addWidget(m_normalCount, 6, 1);
    label(tr("Body Value"), 7);
    m_bodyValue = new QLineEdit();
    m_bodyValue->setReadOnly(true);
    m_bodyValue->setAlignment(Qt::AlignRight);
    grid->addWidget(m_bodyValue, 7, 1);
    grid->setRowStretch(8, 1);
    grid->setColumnStretch(1, 1);

    connect(m_partCombo, &QComboBox::currentIndexChanged, this, &ModelViewerWindow::showPart);
    connect(m_subModelCombo, &QComboBox::currentIndexChanged, this, &ModelViewerWindow::showSubModel);
    connect(m_skinCombo, &QComboBox::currentIndexChanged, this, [this](int skin) {
        if (Asset* shown = asset(); shown && skin >= 0)
        {
            shown->skin = skin;
            m_view->setSkin(0, skin);
        }
    });
    return panel;
}

QWidget* ModelViewerWindow::buildTextures()
{
    auto* panel = new QWidget();
    auto* row = new QHBoxLayout(panel);
    auto* grid = new QGridLayout();
    m_textureCombo = new QComboBox();
    m_textureCombo->setObjectName(QStringLiteral("textures"));
    grid->addWidget(m_textureCombo, 0, 0, 1, 3);
    grid->addWidget(new QLabel(tr("Name")), 1, 0);
    m_textureName = new QLineEdit();
    m_textureName->setMaxLength(63);
    grid->addWidget(m_textureName, 1, 1, 1, 2);
    grid->addWidget(new QLabel(tr("Texture Scale")), 2, 0);
    m_scaleSlider = new QSlider(Qt::Horizontal);
    m_scaleSlider->setRange(1, 20);
    grid->addWidget(m_scaleSlider, 2, 1);
    m_scaleSpin = doubleSpin(1.0, 20.0);
    grid->addWidget(m_scaleSpin, 2, 2);
    auto* mesh = new QGroupBox(tr("Mesh"));
    auto* meshGrid = new QGridLayout(mesh);
    m_meshCombo = new QComboBox();
    meshGrid->addWidget(m_meshCombo, 0, 0, Qt::AlignTop);
    auto* uvColumn = new QVBoxLayout();
    m_showUv = new QCheckBox(tr("Show UV Map"));
    m_overlayUv = new QCheckBox(tr("Overlay UV Map"));
    m_smoothUv = new QCheckBox(tr("Anti-Alias Lines"));
    for (QCheckBox* box : { m_showUv, m_overlayUv, m_smoothUv }) uvColumn->addWidget(box);
    meshGrid->addLayout(uvColumn, 0, 1);
    meshGrid->setRowStretch(1, 1);
    grid->addWidget(mesh, 3, 0, 1, 3);
    grid->setRowStretch(4, 1);
    row->addLayout(grid, 2);

    grid = new QGridLayout();
    auto* flags = new QGroupBox(tr("Flags"));
    auto* flagColumn = new QVBoxLayout(flags);
    const std::pair<QString, int> flagNames[] = { { tr("Chrome"), STUDIO_NF_CHROME },       { tr("Additive"), STUDIO_NF_ADDITIVE },
                                                  { tr("Transparent"), STUDIO_NF_MASKED },  { tr("Flat Shade"), STUDIO_NF_FLATSHADE },
                                                  { tr("Fullbright"), STUDIO_NF_FULLBRIGHT }, { tr("Mipmaps"), STUDIO_NF_MIPMAPS } };
    for (const auto& [text, flag] : flagNames)
    {
        auto* box = new QCheckBox(text);
        flagColumn->addWidget(box);
        m_textureFlags.emplace_back(box, flag);
    }
    flagColumn->addStretch(1);
    grid->addWidget(flags, 1, 0, 6, 2);
    auto* importOne = new QPushButton(tr("Import Texture"));
    auto* exportOne = new QPushButton(tr("Export Texture"));
    auto* exportUv = new QPushButton(tr("Export UV Map"));
    auto* importAll = new QPushButton(tr("Import All Textures"));
    auto* exportAll = new QPushButton(tr("Export All Textures"));
    int place = 1;
    for (QPushButton* button : { importOne, exportOne, exportUv, importAll, exportAll }) grid->addWidget(button, place++, 2);
    connect(importOne, &QPushButton::clicked, this, &ModelViewerWindow::importTexture);
    connect(importAll, &QPushButton::clicked, this, &ModelViewerWindow::importAllTextures);
    grid->setRowStretch(6, 1);
    row->addLayout(grid, 1);

    grid = new QGridLayout();
    auto colour = [this, grid](const QString& text, int line, QSlider*& slider, QSpinBox*& spin) {
        grid->addWidget(new QLabel(text), line, 0);
        slider = new QSlider(Qt::Horizontal);
        spin = new QSpinBox();
        // palette on the far left
        slider->setRange(-1, 255);
        spin->setRange(-1, 255);
        spin->setSpecialValueText(tr("Off"));
        slider->setValue(-1);
        spin->setValue(-1);
        grid->addWidget(slider, line, 1);
        grid->addWidget(spin, line, 2);
        connect(slider, &QSlider::valueChanged, spin, &QSpinBox::setValue);
        connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), slider, &QSlider::setValue);
        connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() {
            const int top = m_topSpin->value(), bottom = m_bottomSpin->value();
            m_colormapValue->setText(QString::number(top < 0 || bottom < 0 ? 0 : top | (bottom << 8)));
            m_view->setColormap(0, top, bottom);
            showTexture();
        });
    };
    m_colormapValue = new QLineEdit(QStringLiteral("0"));
    m_colormapValue->setReadOnly(true);
    colour(tr("Top Color"), 0, m_topSlider, m_topSpin);
    colour(tr("Bottom Color"), 1, m_bottomSlider, m_bottomSpin);
    grid->addWidget(new QLabel(tr("Colormap value")), 2, 0);
    grid->addWidget(m_colormapValue, 2, 1, 1, 2);
    grid->setRowStretch(3, 1);
    row->addLayout(grid, 2);

    connect(m_textureCombo, &QComboBox::currentIndexChanged, this, [this](int texture) {
        const QSignalBlocker block(m_meshCombo);
        m_meshCombo->clear();
        const Asset* shown = asset();
        if (shown && texture >= 0)
        {
            m_meshCombo->addItem(tr("All"));
            int count = 0;
            for (const StudioBodyPart& part : shown->model->bodyParts)
                for (const StudioSubModel& sub : part.models)
                    for (const StudioMesh& each : sub.meshes)
                        if (each.texture == texture) m_meshCombo->addItem(tr("Mesh %1").arg(++count));
        }
        showTexture();
    });
    connect(m_meshCombo, &QComboBox::currentIndexChanged, this, &ModelViewerWindow::showTexture);
    connect(m_scaleSlider, &QSlider::valueChanged, m_scaleSpin, [this](int scale) { m_scaleSpin->setValue(scale); });
    connect(m_scaleSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double scale) {
        const QSignalBlocker block(m_scaleSlider);
        m_scaleSlider->setValue(static_cast<int>(scale));
        showTexture();
    });
    for (QCheckBox* box : { m_showUv, m_overlayUv, m_smoothUv }) connect(box, &QCheckBox::toggled, this, &ModelViewerWindow::showTexture);
    connect(exportOne, &QPushButton::clicked, this, &ModelViewerWindow::exportTexture);
    connect(exportUv, &QPushButton::clicked, this, &ModelViewerWindow::exportUvMap);
    connect(exportAll, &QPushButton::clicked, this, &ModelViewerWindow::exportAllTextures);
    return panel;
}

QWidget* ModelViewerWindow::buildModelData()
{
    auto* panel = new QWidget();
    auto* row = new QHBoxLayout(panel);
    auto* column = new QVBoxLayout();
    auto* eye = new QGroupBox(tr("Eye Position"));
    auto* eyeRow = new QHBoxLayout(eye);
    m_eyePosition = vectorEdit(true);
    eyeRow->addWidget(m_eyePosition.widget);
    column->addWidget(eye);
    auto boxPair = [this, column](const QString& title, VectorEdit& low, VectorEdit& high) {
        auto* box = new QGroupBox(title);
        auto* grid = new QGridLayout(box);
        low = vectorEdit(true);
        high = vectorEdit(true);
        grid->addWidget(new QLabel(tr("Min")), 0, 0);
        grid->addWidget(low.widget, 0, 1);
        grid->addWidget(new QLabel(tr("Max")), 1, 0);
        grid->addWidget(high.widget, 1, 1);
        column->addWidget(box);
    };
    boxPair(tr("Bounding Box"), m_boundsMin, m_boundsMax);
    boxPair(tr("Clipping Box"), m_clipMin, m_clipMax);
    column->addStretch(1);
    row->addLayout(column, 2);

    auto* flags = new QGroupBox(tr("Flags"));
    auto* flagRow = new QHBoxLayout(flags);
    QVBoxLayout* flagColumn = nullptr;
    for (int bit = 0; bit < static_cast<int>(std::size(kModelFlags)); ++bit)
    {
        if (bit == 0 || bit == 6)
        {
            if (flagColumn) flagColumn->addStretch(1);
            flagColumn = new QVBoxLayout();
            flagRow->addLayout(flagColumn);
        }
        auto* box = new QCheckBox(tr(kModelFlags[bit]));
        flagColumn->addWidget(box);
        m_modelFlags.emplace_back(box, 1 << bit);
    }
    flagColumn->addStretch(1);
    row->addWidget(flags, 1);
    return panel;
}

QWidget* ModelViewerWindow::buildBones()
{
    auto* panel = new QWidget();
    auto* row = new QHBoxLayout(panel);
    auto* grid = new QGridLayout();
    m_boneCombo = new QComboBox();
    m_boneCombo->setObjectName(QStringLiteral("bones"));
    grid->addWidget(m_boneCombo, 0, 0, 1, 2);
    m_highlightBone = new QCheckBox(tr("Highlight Bone"));
    grid->addWidget(m_highlightBone, 1, 0, 1, 2);
    grid->addWidget(new QLabel(tr("# Root Bones")), 2, 0);
    m_rootBones = number();
    grid->addWidget(m_rootBones, 2, 1);
    grid->addWidget(new QLabel(tr("Bone Name")), 3, 0);
    m_boneName = new QLineEdit();
    m_boneName->setMaxLength(31);
    grid->addWidget(m_boneName, 3, 1);
    grid->addWidget(new QLabel(tr("Parent Bone")), 4, 0);
    m_parentBone = new QLineEdit();
    m_parentBone->setReadOnly(true);
    grid->addWidget(m_parentBone, 4, 1);
    grid->setRowStretch(5, 1);
    row->addLayout(grid, 1);

    grid = new QGridLayout();
    const std::pair<QString, VectorEdit*> values[] = { { tr("Position"), &m_bonePosition },
                                                       { tr("Position Scale"), &m_bonePositionScale },
                                                       { tr("Rotation"), &m_boneRotation },
                                                       { tr("Rotation Scale"), &m_boneRotationScale } };
    int line = 0;
    for (const auto& [text, edit] : values)
    {
        grid->addWidget(new QLabel(text), line, 0);
        *edit = vectorEdit(true);
        for (QDoubleSpinBox* spin : edit->axis) spin->setDecimals(4);
        grid->addWidget(edit->widget, line++, 1);
    }
    grid->setRowStretch(line, 1);
    row->addLayout(grid, 2);

    connect(m_boneCombo, &QComboBox::currentIndexChanged, this, &ModelViewerWindow::showBone);
    connect(m_highlightBone, &QCheckBox::toggled, this, &ModelViewerWindow::applyHighlight);
    return panel;
}

QWidget* ModelViewerWindow::buildControllers()
{
    auto* panel = new QWidget();
    auto* row = new QHBoxLayout(panel);
    auto* grid = new QGridLayout();
    grid->addWidget(new QLabel(tr("Controller")), 0, 0);
    m_controllerCombo = new QComboBox();
    m_controllerCombo->setObjectName(QStringLiteral("controllers"));
    grid->addWidget(m_controllerCombo, 0, 1, 1, 2);
    grid->addWidget(new QLabel(tr("Value")), 1, 0);
    m_controllerSlider = new QSlider(Qt::Horizontal);
    m_controllerSlider->setObjectName(QStringLiteral("controller"));
    m_controllerSlider->setRange(0, kControllerSteps);
    grid->addWidget(m_controllerSlider, 1, 1);
    m_controllerSpin = doubleSpin(-360.0, 360.0);
    grid->addWidget(m_controllerSpin, 1, 2);
    grid->addWidget(new QLabel(tr("Start")), 2, 0);
    m_controllerStart = doubleSpin(-100000.0, 100000.0);
    grid->addWidget(m_controllerStart, 2, 1, 1, 2);
    grid->addWidget(new QLabel(tr("End")), 3, 0);
    m_controllerEnd = doubleSpin(-100000.0, 100000.0);
    grid->addWidget(m_controllerEnd, 3, 1, 1, 2);
    grid->addWidget(new QLabel(tr("Bone")), 4, 0);
    m_controllerBone = new QComboBox();
    grid->addWidget(m_controllerBone, 4, 1, 1, 2);
    // left-aligned lists would shrink to the shortest name
    for (QWidget* wide : std::initializer_list<QWidget*>{ m_controllerCombo, m_controllerBone }) wide->setMinimumWidth(220);
    m_controllerSlider->setMinimumWidth(140);
    grid->setRowStretch(5, 1);
    grid->setColumnStretch(1, 1);
    row->addLayout(grid, 2);
    row->addStretch(1);

    grid = new QGridLayout();
    grid->addWidget(new QLabel(tr("Rest")), 0, 0);
    m_controllerRest = new QSpinBox();
    m_controllerRest->setRange(0, 255);
    grid->addWidget(m_controllerRest, 0, 1);
    grid->addWidget(new QLabel(tr("Index")), 1, 0);
    m_controllerIndex = new QComboBox();
    for (int i = 0; i < 4; ++i) m_controllerIndex->addItem(tr("Controller %1").arg(i));
    for (int i = 0; i < 4; ++i) m_controllerIndex->addItem(tr("Mouth %1").arg(i));

    grid->addWidget(m_controllerIndex, 1, 1);
    grid->addWidget(new QLabel(tr("Axis")), 2, 0);
    m_controllerAxis = new QComboBox();
    m_controllerAxis->addItems({ QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Z"), QStringLiteral("XR"), QStringLiteral("YR"), QStringLiteral("ZR") });

    grid->addWidget(m_controllerAxis, 2, 1);
    grid->setRowStretch(3, 1);
    row->addLayout(grid, 1);

    connect(m_controllerCombo, &QComboBox::currentIndexChanged, this, &ModelViewerWindow::showController);
    connect(m_controllerSlider, &QSlider::valueChanged, this, [this](int steps) {
        const Asset* shown = asset();
        const int index = m_controllerCombo->currentIndex();
        if (!shown || index < 0 || index >= static_cast<int>(shown->controllers.size())) return;
        const StudioBoneController& picked = shown->model->controllers[index];
        setControllerValue(picked.start + (picked.end - picked.start) * steps / kControllerSteps);
    });
    connect(m_controllerSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double value) { setControllerValue(static_cast<float>(value)); });
    return panel;
}

QWidget* ModelViewerWindow::buildAttachments()
{
    auto* panel = new QWidget();
    auto* layout = new QVBoxLayout(panel);
    auto* row = new QHBoxLayout();
    auto* column = new QVBoxLayout();
    m_attachmentCombo = new QComboBox();
    m_attachmentCombo->setObjectName(QStringLiteral("attachments"));
    column->addWidget(m_attachmentCombo);
    auto* buttons = new QHBoxLayout();
    auto* add = new QPushButton(QStringLiteral("+"));
    add->setObjectName(QStringLiteral("addAttachment"));
    add->setToolTip(tr("Add New Attachment"));
    auto* remove = new QPushButton(QStringLiteral("-"));
    remove->setObjectName(QStringLiteral("removeAttachment"));
    remove->setToolTip(tr("Remove Current Attachment"));
    for (QPushButton* button : { add, remove }) button->setMaximumWidth(30);
    m_placeAttachment = new QPushButton(tr("Place on Model"));
    m_placeAttachment->setObjectName(QStringLiteral("placeAttachment"));
    m_placeAttachment->setToolTip(tr("Click the model to put the current attachment there; dragging still turns the view"));
    m_placeAttachment->setCheckable(true);
    buttons->addWidget(add);
    buttons->addWidget(remove);
    buttons->addWidget(m_placeAttachment, 1);
    column->addLayout(buttons);
    m_highlightAttachment = new QCheckBox(tr("Highlight Attachment"));
    column->addWidget(m_highlightAttachment);
    m_attachmentLimit = new QLabel();
    m_attachmentLimit->setObjectName(QStringLiteral("attachmentLimit"));
    m_attachmentLimit->setStyleSheet(QStringLiteral("color: #e53935; font-weight: bold;"));
    m_attachmentLimit->setWordWrap(true);
    m_attachmentLimit->hide();
    column->addWidget(m_attachmentLimit);
    column->addStretch(1);
    row->addLayout(column, 1);
    auto* grid = new QGridLayout();
    grid->addWidget(new QLabel(tr("Name")), 0, 0);
    m_attachmentName = new QLineEdit();
    m_attachmentName->setMaxLength(31);
    grid->addWidget(m_attachmentName, 0, 1);
    grid->addWidget(new QLabel(tr("Bone")), 1, 0);
    m_attachmentBone = new QComboBox();
    grid->addWidget(m_attachmentBone, 1, 1);
    grid->addWidget(new QLabel(tr("Origin")), 2, 0);
    m_attachmentOrigin = vectorEdit(true);
    grid->addWidget(m_attachmentOrigin.widget, 2, 1);
    grid->setRowStretch(3, 1);
    row->addLayout(grid, 2);
    layout->addLayout(row, 1);
    auto* qc = new QGroupBox(tr("QC String"));
    auto* qcLayout = new QVBoxLayout(qc);
    m_attachmentQc = new QLineEdit();
    m_attachmentQc->setReadOnly(true);
    qcLayout->addWidget(m_attachmentQc);
    layout->addWidget(qc);
    connect(m_attachmentCombo, &QComboBox::currentIndexChanged, this, &ModelViewerWindow::showAttachment);
    connect(m_highlightAttachment, &QCheckBox::toggled, this, &ModelViewerWindow::applyHighlight);
    connect(add, &QPushButton::clicked, this, [this]() {
        const int bone = std::max(0, m_boneCombo->currentIndex());
        editModel(tr("Add New Attachment"), QString(), [bone](StudioModel& model) {
            StudioAttachment made;
            made.bone = std::min(bone, std::max(0, static_cast<int>(model.bones.size()) - 1));
            model.attachments.push_back(made);
        });
        m_attachmentCombo->setCurrentIndex(m_attachmentCombo->count() - 1);
        // the next click places it
        m_highlightAttachment->setChecked(true);
        m_placeAttachment->setChecked(true);
    });
    connect(remove, &QPushButton::clicked, this, [this]() {
        const int index = m_attachmentCombo->currentIndex();
        editModel(tr("Remove Current Attachment"), QString(), [index](StudioModel& model) {
            if (index >= 0 && index < static_cast<int>(model.attachments.size())) model.attachments.erase(model.attachments.begin() + index);
        });
    });
    connect(m_placeAttachment, &QPushButton::toggled, m_view, &StudioModelView::setSurfacePicking);
    connect(m_view, &StudioModelView::surfacePicked, this, [this](int bone, const QVector3D& position) {
        const int index = m_attachmentCombo->currentIndex();
        if (!m_placeAttachment->isChecked() || index < 0) return;
        editModel(tr("Place on Model"), QString(), [index, bone, position](StudioModel& model) {
            if (index >= static_cast<int>(model.attachments.size())) return;
            model.attachments[index].bone = bone;
            model.attachments[index].origin = aiVector3D(position.x(), position.y(), position.z());
        });
    });
    return panel;
}

QWidget* ModelViewerWindow::buildHitboxes()
{
    auto* panel = new QWidget();
    auto* layout = new QVBoxLayout(panel);
    auto* row = new QHBoxLayout();
    auto* column = new QVBoxLayout();
    m_hitboxCombo = new QComboBox();
    m_hitboxCombo->setObjectName(QStringLiteral("hitboxes"));
    column->addWidget(m_hitboxCombo);
    m_highlightHitbox = new QCheckBox(tr("Highlight Hitbox"));
    column->addWidget(m_highlightHitbox);
    column->addStretch(1);
    row->addLayout(column, 1);
    auto* grid = new QGridLayout();
    grid->addWidget(new QLabel(tr("Bone")), 0, 0);
    m_hitboxBone = new QComboBox();
    grid->addWidget(m_hitboxBone, 0, 1);
    grid->addWidget(new QLabel(tr("Hitgroup")), 1, 0);
    m_hitboxGroup = new QSpinBox();
    m_hitboxGroup->setRange(-1000, 1000);
    grid->addWidget(m_hitboxGroup, 1, 1);
    grid->addWidget(new QLabel(tr("Minimum")), 2, 0);
    m_hitboxMin = vectorEdit(true);
    grid->addWidget(m_hitboxMin.widget, 2, 1);
    grid->addWidget(new QLabel(tr("Maximum")), 3, 0);
    m_hitboxMax = vectorEdit(true);
    grid->addWidget(m_hitboxMax.widget, 3, 1);
    grid->setRowStretch(4, 1);
    row->addLayout(grid, 2);
    layout->addLayout(row, 1);
    auto* qc = new QGroupBox(tr("QC String"));
    auto* qcLayout = new QVBoxLayout(qc);
    m_hitboxQc = new QLineEdit();
    m_hitboxQc->setReadOnly(true);
    qcLayout->addWidget(m_hitboxQc);
    layout->addWidget(qc);
    connect(m_hitboxCombo, &QComboBox::currentIndexChanged, this, &ModelViewerWindow::showHitbox);
    connect(m_highlightHitbox, &QCheckBox::toggled, this, &ModelViewerWindow::applyHighlight);
    return panel;
}

QWidget* ModelViewerWindow::buildConsole()
{
    auto* panel = new QWidget();
    auto* layout = new QVBoxLayout(panel);
    auto* top = new QHBoxLayout();
    auto* clear = new QPushButton(tr("Clear"));
    top->addWidget(clear);
    top->addStretch(1);
    layout->addLayout(top);
    auto* console = new QPlainTextEdit();
    console->setObjectName(QStringLiteral("console"));
    console->setReadOnly(true);
    console->setMaximumBlockCount(5000);
    VortigauntLog::addLogWidget(console);
    layout->addWidget(console, 1);
    connect(clear, &QPushButton::clicked, console, &QPlainTextEdit::clear);
    m_consolePanel = panel;
    return panel;
}

QWidget* ModelViewerWindow::buildTransform()
{
    // whole-model edits; each apply is one undo step
    auto* panel = new QWidget();
    auto* grid = new QGridLayout(panel);
    auto* mode = new QGroupBox(tr("Mode"));
    auto* modes = new QVBoxLayout(mode);
    m_rotateMode = new QRadioButton(tr("Rotate"));
    m_rotateMode->setChecked(true);
    m_scaleMode = new QRadioButton(tr("Scale"));
    m_scaleMode->setObjectName(QStringLiteral("scaleMode"));
    auto* moveMode = new QRadioButton(tr("Move"));
    m_doubleSideMode = new QRadioButton(tr("Double-Side Face"));
    m_doubleSideMode->setObjectName(QStringLiteral("doubleSideMode"));
    m_doubleSideMode->setToolTip(tr("Adds a reversed face behind the faces the engine hides from behind (backface culling); a thickness gives them depth, as Blender's Solidify does"));
    for (QRadioButton* button : { m_rotateMode, m_scaleMode, moveMode, m_doubleSideMode }) modes->addWidget(button);
    modes->addStretch(1);
    grid->addWidget(mode, 0, 0);

    auto* values = new QGroupBox(tr("Values"));
    auto* valueLayout = new QVBoxLayout(values);
    auto* pages = new QStackedWidget();
    m_rotateValues = vectorEdit(true, 360.0);
    auto* rotatePage = new QWidget();
    auto* rotateLayout = new QVBoxLayout(rotatePage);
    rotateLayout->addWidget(m_rotateValues.widget);
    rotateLayout->addStretch(1);
    pages->addWidget(rotatePage);
    auto* scalePage = new QWidget();
    auto* scaleLayout = new QVBoxLayout(scalePage);
    m_scaleValue = doubleSpin(0.001, 1000.0, 3);
    m_scaleValue->setObjectName(QStringLiteral("scaleValue"));
    m_scaleValue->setValue(1.0);
    scaleLayout->addWidget(m_scaleValue);
    const std::pair<QString, int> parts[] = { { tr("Scale Meshes"), StudioModel::ScaleMeshes },
                                              { tr("Scale Hitboxes"), StudioModel::ScaleHitboxes },
                                              { tr("Scale Sequence BBoxes"), StudioModel::ScaleSequenceBoxes },
                                              { tr("Scale Bones"), StudioModel::ScaleBones },
                                              { tr("Scale Eye Position"), StudioModel::ScaleEyePosition },
                                              { tr("Scale Attachments"), StudioModel::ScaleAttachments } };
    auto* partGrid = new QGridLayout();
    int place = 0;
    for (const auto& [text, part] : parts)
    {
        auto* box = new QCheckBox(text);
        box->setChecked(true);
        partGrid->addWidget(box, place % 3, place / 3);
        ++place;
        m_scaleParts.emplace_back(box, part);
    }
    scaleLayout->addLayout(partGrid);
    scaleLayout->addStretch(1);
    pages->addWidget(scalePage);
    m_moveValues = vectorEdit(true);
    auto* movePage = new QWidget();
    auto* moveLayout = new QVBoxLayout(movePage);
    moveLayout->addWidget(m_moveValues.widget);
    moveLayout->addStretch(1);
    pages->addWidget(movePage);
    auto* solidPage = new QWidget();
    auto* solidLayout = new QGridLayout(solidPage);
    solidLayout->addWidget(new QLabel(tr("Thickness")), 0, 0);
    m_solidThickness = doubleSpin(0.0, 64.0, 2);
    m_solidThickness->setObjectName(QStringLiteral("solidThickness"));
    m_solidThickness->setToolTip(tr("0 shows the same faces from behind; more pushes the back side inward and gives the model depth"));
    solidLayout->addWidget(m_solidThickness, 0, 1);
    solidLayout->addWidget(new QLabel(tr("Faces")), 1, 0);
    m_solidFaces = new QComboBox();
    m_solidFaces->setObjectName(QStringLiteral("solidFaces"));
    m_solidFaces->addItems({ tr("All Textures"), tr("Current Texture") });
    solidLayout->addWidget(m_solidFaces, 1, 1);
    m_solidRims = new QCheckBox(tr("Fill Rims"));
    m_solidRims->setToolTip(tr("Closes the open edges between the two sides when there is a thickness"));
    m_solidRims->setChecked(true);
    solidLayout->addWidget(m_solidRims, 2, 0, 1, 2);
    solidLayout->setRowStretch(3, 1);
    solidLayout->setColumnStretch(2, 1);
    pages->addWidget(solidPage);
    valueLayout->addWidget(pages);
    grid->addWidget(values, 0, 1);

    m_resetOnApply = new QCheckBox(tr("Reset values on apply"));
    m_resetOnApply->setChecked(true);
    grid->addWidget(m_resetOnApply, 1, 0, 1, 2);
    auto* apply = new QPushButton(tr("Apply"));
    apply->setObjectName(QStringLiteral("applyTransform"));
    grid->addWidget(apply, 2, 0, Qt::AlignLeft);
    grid->setRowStretch(3, 1);
    grid->setColumnStretch(2, 1);

    connect(m_rotateMode, &QRadioButton::toggled, this, [pages](bool on) { if (on) pages->setCurrentIndex(0); });
    connect(m_scaleMode, &QRadioButton::toggled, this, [pages](bool on) { if (on) pages->setCurrentIndex(1); });
    connect(moveMode, &QRadioButton::toggled, this, [pages](bool on) { if (on) pages->setCurrentIndex(2); });
    connect(m_doubleSideMode, &QRadioButton::toggled, this, [pages](bool on) { if (on) pages->setCurrentIndex(3); });
    connect(apply, &QPushButton::clicked, this, &ModelViewerWindow::applyTransform);
    return panel;
}

void ModelViewerWindow::applyTransform()
{
    if (m_rotateMode->isChecked())
    {
        const aiVector3D angles = m_rotateValues.value();
        editModel(tr("Rotate"), QString(), [angles](StudioModel& model) { model.Rotate(angles); });
    }
    else if (m_scaleMode->isChecked())
    {
        const float factor = static_cast<float>(m_scaleValue->value());
        int chosen = 0;
        for (const auto& [box, part] : m_scaleParts)
            if (box->isChecked()) chosen |= part;
        editModel(tr("Scale"), QString(), [factor, chosen](StudioModel& model) { model.Scale(factor, chosen); });
    }
    else if (m_doubleSideMode->isChecked())
    {
        const Asset* shown = asset();
        if (!shown) return;
        const int texture = m_solidFaces->currentIndex() == 1 ? m_textureCombo->currentIndex() : -1;
        if (m_solidFaces->currentIndex() == 1 && texture < 0) return;
        // try on a copy; a model past the limits stays unchanged
        auto made = std::make_shared<StudioModel>(*shown->model);
        int added = 0;
        std::string error;
        if (!made->MakeDoubleSided(static_cast<float>(m_solidThickness->value()), m_solidRims->isChecked(), texture, added, error))
        {
            VortigauntLog::Vortigaunt_Printf(QStringLiteral("ERROR: Double-Side Face: %1").arg(QString::fromStdString(error)));
            QMessageBox::warning(this, tr("Double-Side Face"), tr("The model would pass the engine's limits; see the console."));
            return;
        }
        VortigauntLog::Vortigaunt_Printf(QStringLiteral("Double-Side Face: %1 faces added").arg(added));
        if (added == 0) return;
        editModel(tr("Double-Side Face"), QString(), [made](StudioModel& model) { model = *made; });
    }
    else
    {
        const aiVector3D offset = m_moveValues.value();
        editModel(tr("Move"), QString(), [offset](StudioModel& model) { model.Move(offset); });
    }
    if (m_resetOnApply->isChecked())
    {
        m_rotateValues.set(aiVector3D());
        m_moveValues.set(aiVector3D());
        m_scaleValue->setValue(1.0);
    }
}

void ModelViewerWindow::importTexture()
{
    const Asset* shown = asset();
    const int index = m_textureCombo->currentIndex();
    if (!shown || index < 0 || index >= static_cast<int>(shown->model->textures.size())) return;
    const QString path = QFileDialog::getOpenFileName(this, tr("Import Texture"), QFileInfo(shown->path).absolutePath(), tr("Images (*.bmp *.png *.tga *.jpg)"));
    if (path.isEmpty()) return;
    StudioTexture picture;
    if (!textureFromImage(QImage(path), picture))
    {
        QMessageBox::warning(this, tr("Import Texture"), tr("Could not read %1 as a picture of 256 colours or fewer.").arg(QFileInfo(path).fileName()));
        return;
    }
    editModel(tr("Import Texture"), QString(), [index, picture](StudioModel& model) {
        if (index >= static_cast<int>(model.textures.size())) return;
        StudioTexture& texture = model.textures[index];
        texture.width = picture.width;
        texture.height = picture.height;
        texture.pixels = picture.pixels;
        texture.palette = picture.palette;
    });
}

void ModelViewerWindow::importAllTextures()
{
    const Asset* shown = asset();
    if (!shown) return;
    const QString folder = QFileDialog::getExistingDirectory(this, tr("Import All Textures"), QFileInfo(shown->path).absolutePath());
    if (folder.isEmpty()) return;
    // match images by texture name, as Export All Textures writes them
    std::vector<std::pair<int, StudioTexture>> found;
    const QFileInfoList files = QDir(folder).entryInfoList(QDir::Files);
    for (size_t i = 0; i < shown->model->textures.size(); ++i)
    {
        const QString base = QFileInfo(QString::fromStdString(shown->model->textures[i].name)).completeBaseName();
        for (const QFileInfo& file : files)
        {
            StudioTexture picture;
            if (file.completeBaseName().compare(base, Qt::CaseInsensitive) == 0 && textureFromImage(QImage(file.absoluteFilePath()), picture))
            {
                found.emplace_back(static_cast<int>(i), std::move(picture));
                break;
            }
        }
    }
    VortigauntLog::Vortigaunt_Printf(QStringLiteral("%1 of %2 textures found in %3").arg(found.size()).arg(shown->model->textures.size()).arg(QDir::toNativeSeparators(folder)));
    if (found.empty()) return;
    editModel(tr("Import All Textures"), QString(), [found](StudioModel& model) {
        for (const auto& [index, picture] : found)
        {
            if (index >= static_cast<int>(model.textures.size())) continue;
            StudioTexture& texture = model.textures[index];
            texture.width = picture.width;
            texture.height = picture.height;
            texture.pixels = picture.pixels;
            texture.palette = picture.palette;
        }
    });
}

bool ModelViewerWindow::isSoundEvent(int event)
{
    return event == 1004 || event == 1008 || event == 5004 || event == 1011 || event == 1012 || event == 1013;
}

QString ModelViewerWindow::findSound(const QString& modelPath, const QString& sound)
{
    // prefix characters the engine allows on sound names
    QString name = sound.trimmed();
    name.replace(QLatin1Char('\\'), QLatin1Char('/'));
    while (!name.isEmpty() && QStringLiteral("*!#$%^&()<>?@").contains(name.front())) name.remove(0, 1);
    if (name.isEmpty()) return {};
    // search up from the model, then the game of a _downloads folder, then valve
    QDir at = QFileInfo(modelPath).absoluteDir();
    for (int up = 0; up < 6; ++up)
    {
        QStringList folders = { at.absolutePath() };
        static const QRegularExpression extra(QStringLiteral("_(downloads|addon|hd|lv)$"), QRegularExpression::CaseInsensitiveOption);
        QString game = at.dirName();
        game.remove(extra);
        QDir parent = at;
        if (parent.cdUp())
        {
            if (game != at.dirName()) folders << parent.filePath(game);
            folders << parent.filePath(QStringLiteral("valve"));
        }
        for (const QString& folder : folders)
        {
            const QString file = QDir(folder).filePath(QStringLiteral("sound/") + name);
            if (QFileInfo::exists(file)) return file;
        }
        if (!at.cdUp()) break;
    }
    return {};
}

void ModelViewerWindow::playEvents()
{
    const Asset* shown = asset();
    const int sequence = m_view->sequence(0);
    const float now = m_view->framePosition();
    if (sequence != m_soundSequence)
    {
        // start before frame 0 so its events play
        m_soundSequence = sequence;
        m_soundFrame = -1.0f;
    }
    const float before = m_soundFrame;
    m_soundFrame = now;
    if (!shown || !m_playSounds->isChecked() || !m_view->isPlaying() || now == before) return;
    if (sequence < 0 || sequence >= static_cast<int>(shown->model->sequences.size())) return;
    // from the last check, wrapping when the sequence loops
    auto passed = [before, now](int frame) { return before <= now ? (frame > before && frame <= now) : (frame > before || frame <= now); };
    for (const StudioEvent& event : shown->model->sequences[sequence].events)
        if (isSoundEvent(event.event) && passed(event.frame)) playSound(QString::fromStdString(event.options));
}

void ModelViewerWindow::playSound(const QString& sound)
{
    const Asset* shown = asset();
    const QString file = shown ? findSound(shown->path, sound) : QString();
    if (file.isEmpty())
    {
        static QSet<QString> told;
        if (!told.contains(sound))
        {
            told.insert(sound);
            VortigauntLog::Vortigaunt_Printf(QStringLiteral("Sound not found: %1").arg(sound));
        }
        return;
    }
    // one player per sound, deleted when done
    auto* effect = new QSoundEffect(this);
    effect->setSource(QUrl::fromLocalFile(file));
    connect(effect, &QSoundEffect::playingChanged, effect, [effect]() {
        if (!effect->isPlaying() && effect->status() == QSoundEffect::Ready) effect->deleteLater();
    });
    connect(effect, &QSoundEffect::statusChanged, effect, [effect]() {
        if (effect->status() == QSoundEffect::Error) effect->deleteLater();
    });
    effect->play();
}

void ModelViewerWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (!modelPaths(event->mimeData()).isEmpty()) event->acceptProposedAction();
}

void ModelViewerWindow::dropEvent(QDropEvent* event)
{
    const QStringList paths = modelPaths(event->mimeData());
    for (const QString& path : paths) load(path);
    if (!paths.isEmpty()) event->acceptProposedAction();
}

ModelViewerWindow::Asset* ModelViewerWindow::asset()
{
    return m_current >= 0 && m_current < static_cast<int>(m_assets.size()) ? &m_assets[m_current] : nullptr;
}

bool ModelViewerWindow::load(const QString& path)
{
    const QString whole = QFileInfo(path).absoluteFilePath();
    for (size_t i = 0; i < m_assets.size(); ++i)
        if (m_assets[i].path == whole)
        {
            m_tabs->setCurrentIndex(static_cast<int>(i));
            return true;
        }
    auto model = std::make_shared<StudioModel>();
    if (!model->Load(whole.toStdString()))
    {
        VortigauntLog::Vortigaunt_Printf(QStringLiteral("ERROR: Could not read %1: %2").arg(QDir::toNativeSeparators(whole), QString::fromStdString(model->GetError())));
        return false;
    }
    VortigauntLog::Vortigaunt_Printf(QStringLiteral("Model opened: %1 (%2 sequences, %3 textures)")
                                         .arg(QDir::toNativeSeparators(whole))
                                         .arg(model->sequences.size())
                                         .arg(model->textures.size()));
    if (model->attachments.size() > static_cast<size_t>(kStudioMaxAttachments))
        VortigauntLog::Vortigaunt_Printf(QStringLiteral("^1GoldSrc supports at most %1 attachments: %2 has %3, Please decrease to 4 attachments. otherwise the engine will crash")
                                             .arg(kStudioMaxAttachments)
                                             .arg(QFileInfo(whole).fileName())
                                             .arg(model->attachments.size()));
    Asset added;
    added.path = whole;
    added.model = model;
    added.undo = new QUndoStack(this);
    m_undoGroup->addStack(added.undo);
    connect(added.undo, &QUndoStack::cleanChanged, this, [this, stack = added.undo]() {
        for (size_t i = 0; i < m_assets.size(); ++i)
            if (m_assets[i].undo == stack) updateTabTitle(static_cast<int>(i));
    });
    added.body.assign(model->bodyParts.size(), 0);
    added.controllers.assign(model->controllers.size(), 0.0f);
    m_assets.push_back(std::move(added));
    // the new tab becomes current; the first one is shown by hand
    const int index = m_tabs->addTab(QFileInfo(path).fileName());
    updateTabTitle(index);
    if (m_tabs->currentIndex() != index)
        m_tabs->setCurrentIndex(index);
    else
        showAsset(index);
    if (m_firstPersonViewModels->isChecked() && QFileInfo(path).fileName().startsWith(QStringLiteral("v_"), Qt::CaseInsensitive))
        m_cameraCombo->setCurrentIndex(1);
    return true;
}

void ModelViewerWindow::closeAsset(int index)
{
    if (index < 0 || index >= static_cast<int>(m_assets.size()) || !confirmClose(index)) return;
    QUndoStack* undo = m_assets[index].undo;
    m_current = -1;
    m_assets.erase(m_assets.begin() + index);
    m_tabs->removeTab(index);
    showAsset(m_assets.empty() ? -1 : m_tabs->currentIndex());
    m_undoGroup->removeStack(undo);
    delete undo;
}

void ModelViewerWindow::showAsset(int index)
{
    const Filling filling(m_filling);
    m_current = index;
    m_view->clearModels();
    const Asset* shown = asset();
    m_undoGroup->setActiveStack(shown ? shown->undo : nullptr);
    QComboBox* combos[] = { m_sequenceCombo, m_eventCombo,      m_partCombo,      m_subModelCombo, m_skinCombo,      m_textureCombo, m_meshCombo,
                            m_boneCombo,     m_controllerCombo, m_controllerBone, m_attachmentCombo, m_attachmentBone, m_hitboxCombo,  m_hitboxBone };
    {
        std::vector<std::unique_ptr<QSignalBlocker>> blocks;
        for (QComboBox* combo : combos)
        {
            blocks.push_back(std::make_unique<QSignalBlocker>(combo));
            combo->clear();
        }
        m_textureImage->clear();
        if (!shown)
        {
            setWindowTitle(tr("Model Viewer"));
            m_modelInfo.clear();
        }
        // with no model only the console is enabled
        for (QDockWidget* panel : m_edit->findChildren<QDockWidget*>()) panel->widget()->setEnabled(shown || panel->widget() == m_consolePanel);
        if (!shown) return;
        const StudioModel& data = *shown->model;
        updateTabTitle(m_current);
        m_view->addModel(shown->model);
        m_view->setSkin(0, shown->skin);
        m_view->setControllers(0, shown->controllers);
        m_view->setColormap(0, m_topSpin->value(), m_bottomSpin->value());
        for (size_t part = 0; part < shown->body.size(); ++part) m_view->setBody(0, static_cast<int>(part), shown->body[part]);
        m_view->setRestPose(0, m_poseCombo->currentIndex() == 1);
        applyPlacement();

        fillCombo(m_sequenceCombo, data.sequences, [&](size_t i) { return indexed(data.sequences[i].name, static_cast<int>(i)); });
        fillCombo(m_partCombo, data.bodyParts, [&](size_t i) { return QString::fromStdString(data.bodyParts[i].name); });
        fillCombo(m_skinCombo, data.skinFamilies, [&](size_t i) { return tr("Skin %1").arg(i + 1); });
        fillCombo(m_textureCombo, data.textures, [&](size_t i) { return textureLabel(data.textures[i]); });
        for (QComboBox* bones : { m_boneCombo, m_controllerBone, m_attachmentBone, m_hitboxBone })
            fillCombo(bones, data.bones, [&](size_t i) { return indexed(data.bones[i].name, static_cast<int>(i)); });
        fillCombo(m_controllerCombo, data.controllers, [&](size_t i) {
            return data.controllers[i].index == 4 ? tr("Mouth") : tr("Controller %1").arg(data.controllers[i].index);
        });
        fillCombo(m_attachmentCombo, data.attachments, [&](size_t i) {
            return data.attachments[i].name.empty() ? tr("Attachment %1").arg(i + 1) : QString::fromStdString(data.attachments[i].name);
        });
        fillCombo(m_hitboxCombo, data.hitboxes, [&](size_t i) { return tr("Hitbox %1").arg(i + 1); });
        m_skinCombo->setCurrentIndex(shown->skin);
    }
    const StudioModel& data = *shown->model;
    showModelData();

    m_view->fitView();
    const int sequence = std::clamp(shown->sequence, 0, std::max(0, static_cast<int>(data.sequences.size()) - 1));
    m_sequenceCombo->setCurrentIndex(sequence);
    showSequence(sequence);
    m_partCombo->setCurrentIndex(data.bodyParts.empty() ? -1 : 0);
    showPart(m_partCombo->currentIndex());
    m_textureCombo->setCurrentIndex(-1);
    m_textureCombo->setCurrentIndex(data.textures.empty() ? -1 : 0);
    if (data.textures.empty()) showTexture();
    for (QComboBox* combo : { m_boneCombo, m_controllerCombo, m_attachmentCombo, m_hitboxCombo })
        if (combo->count()) combo->setCurrentIndex(0);
    showBone(m_boneCombo->currentIndex());
    showController(m_controllerCombo->currentIndex());
    showAttachment(m_attachmentCombo->currentIndex());
    showHitbox(m_hitboxCombo->currentIndex());
}

void ModelViewerWindow::editModel(const QString& what, const QString& key, const std::function<void(StudioModel&)>& change)
{
    Asset* shown = asset();
    if (m_filling || !shown || !shown->undo) return;
    auto after = std::make_shared<StudioModel>(*shown->model);
    change(*after);
    shown->undo->push(new ModelEdit(this, shown->undo, what, key, shown->model, after));
}

void ModelViewerWindow::setModel(QUndoStack* stack, const std::shared_ptr<StudioModel>& model)
{
    for (size_t i = 0; i < m_assets.size(); ++i)
    {
        if (m_assets[i].undo != stack) continue;
        m_assets[i].model = model;
        if (static_cast<int>(i) == m_current)
        {
            m_view->replaceModel(0, model);
            refreshPanels();
        }
        return;
    }
}

void ModelViewerWindow::connectEdits()
{
    // edits keyed by part index; the same key merges steps
    auto onSequence = [this](const char* what, std::function<void(StudioSequence&)> change) {
        const int i = m_sequenceCombo->currentIndex();
        editModel(tr(what), QStringLiteral("sequence %1 %2").arg(i).arg(QLatin1String(what)), [i, change](StudioModel& model) {
            if (i >= 0 && i < static_cast<int>(model.sequences.size())) change(model.sequences[i]);
        });
    };
    auto onEvent = [this](const char* what, std::function<void(StudioEvent&)> change) {
        const int s = m_sequenceCombo->currentIndex(), e = m_eventCombo->currentIndex();
        editModel(tr(what), QStringLiteral("event %1 %2 %3").arg(s).arg(e).arg(QLatin1String(what)), [s, e, change](StudioModel& model) {
            if (s >= 0 && s < static_cast<int>(model.sequences.size()) && e >= 0 && e < static_cast<int>(model.sequences[s].events.size()))
                change(model.sequences[s].events[e]);
        });
    };
    auto onBone = [this](const char* what, std::function<void(StudioBone&)> change) {
        const int i = m_boneCombo->currentIndex();
        editModel(tr(what), QStringLiteral("bone %1 %2").arg(i).arg(QLatin1String(what)), [i, change](StudioModel& model) {
            if (i >= 0 && i < static_cast<int>(model.bones.size())) change(model.bones[i]);
        });
    };
    auto onController = [this](const char* what, std::function<void(StudioBoneController&)> change) {
        const int i = m_controllerCombo->currentIndex();
        editModel(tr(what), QStringLiteral("controller %1 %2").arg(i).arg(QLatin1String(what)), [i, change](StudioModel& model) {
            if (i >= 0 && i < static_cast<int>(model.controllers.size())) change(model.controllers[i]);
        });
    };
    auto onAttachment = [this](const char* what, std::function<void(StudioAttachment&)> change) {
        const int i = m_attachmentCombo->currentIndex();
        editModel(tr(what), QStringLiteral("attachment %1 %2").arg(i).arg(QLatin1String(what)), [i, change](StudioModel& model) {
            if (i >= 0 && i < static_cast<int>(model.attachments.size())) change(model.attachments[i]);
        });
    };
    auto onHitbox = [this](const char* what, std::function<void(StudioHitbox&)> change) {
        const int i = m_hitboxCombo->currentIndex();
        editModel(tr(what), QStringLiteral("hitbox %1 %2").arg(i).arg(QLatin1String(what)), [i, change](StudioModel& model) {
            if (i >= 0 && i < static_cast<int>(model.hitboxes.size())) change(model.hitboxes[i]);
        });
    };
    auto onTexture = [this](const char* what, std::function<void(StudioTexture&)> change) {
        const int i = m_textureCombo->currentIndex();
        editModel(tr(what), QStringLiteral("texture %1 %2").arg(i).arg(QLatin1String(what)), [i, change](StudioModel& model) {
            if (i >= 0 && i < static_cast<int>(model.textures.size())) change(model.textures[i]);
        });
    };
    auto onModel = [this](const char* what, std::function<void(StudioModel&)> change) { editModel(tr(what), QStringLiteral("model %1").arg(QLatin1String(what)), change); };
    auto onVector = [this](const VectorEdit& edit, std::function<void(const aiVector3D&)> apply) {
        for (QDoubleSpinBox* axis : edit.axis)
            connect(axis, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [&edit, apply]() { apply(edit.value()); });
    };

    connect(m_isLooping, &QCheckBox::toggled, this, [onSequence](bool on) {
        onSequence(QT_TR_NOOP("Is Looping"), [on](StudioSequence& s) { s.flags = on ? (s.flags | 1) : (s.flags & ~1); });
    });
    connect(m_sequenceFps, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [onSequence](double fps) {
        onSequence(QT_TR_NOOP("FPS"), [fps](StudioSequence& s) { s.fps = static_cast<float>(fps); });
    });
    connect(m_activity, QOverload<int>::of(&QSpinBox::valueChanged), this, [onSequence](int activity) {
        onSequence(QT_TR_NOOP("Activity"), [activity](StudioSequence& s) { s.activity = activity; });
    });
    connect(m_activityWeight, QOverload<int>::of(&QSpinBox::valueChanged), this, [onSequence](int weight) {
        onSequence(QT_TR_NOOP("Activity Weight"), [weight](StudioSequence& s) { s.activityWeight = weight; });
    });
    onVector(m_linearMovement, [onSequence](const aiVector3D& v) { onSequence(QT_TR_NOOP("Linear Movement"), [v](StudioSequence& s) { s.linearMovement = v; }); });
    connect(m_eventFrame, QOverload<int>::of(&QSpinBox::valueChanged), this, [onEvent](int frame) {
        onEvent(QT_TR_NOOP("Frame"), [frame](StudioEvent& e) { e.frame = frame; });
    });
    connect(m_eventId, QOverload<int>::of(&QSpinBox::valueChanged), this, [onEvent](int id) {
        onEvent(QT_TR_NOOP("Id"), [id](StudioEvent& e) { e.event = id; });
    });
    connect(m_eventOptions, &QLineEdit::textEdited, this, [onEvent](const QString& text) {
        onEvent(QT_TR_NOOP("Options"), [text](StudioEvent& e) { e.options = text.toStdString(); });
    });

    connect(m_subModelName, &QLineEdit::textEdited, this, [this](const QString& text) {
        const int part = m_partCombo->currentIndex(), sub = m_subModelCombo->currentIndex();
        editModel(tr("Name"), QStringLiteral("submodel %1 %2").arg(part).arg(sub), [part, sub, text](StudioModel& model) {
            if (part >= 0 && part < static_cast<int>(model.bodyParts.size()) && sub >= 0 && sub < static_cast<int>(model.bodyParts[part].models.size()))
                model.bodyParts[part].models[sub].name = text.toStdString();
        });
    });

    connect(m_textureName, &QLineEdit::textEdited, this, [onTexture](const QString& text) {
        onTexture(QT_TR_NOOP("Name"), [text](StudioTexture& t) { t.name = text.toStdString(); });
    });
    for (const auto& [box, flag] : m_textureFlags)
    {
        const int bit = flag;
        connect(box, &QCheckBox::toggled, this, [onTexture, bit](bool on) {
            onTexture(QT_TR_NOOP("Flags"), [on, bit](StudioTexture& t) { t.flags = on ? (t.flags | bit) : (t.flags & ~bit); });
        });
    }

    onVector(m_eyePosition, [onModel](const aiVector3D& v) { onModel(QT_TR_NOOP("Eye Position"), [v](StudioModel& m) { m.eyePosition = v; }); });
    onVector(m_boundsMin, [onModel](const aiVector3D& v) { onModel(QT_TR_NOOP("Bounding Box"), [v](StudioModel& m) { m.boundsMin = v; }); });
    onVector(m_boundsMax, [onModel](const aiVector3D& v) { onModel(QT_TR_NOOP("Bounding Box"), [v](StudioModel& m) { m.boundsMax = v; }); });
    onVector(m_clipMin, [onModel](const aiVector3D& v) { onModel(QT_TR_NOOP("Clipping Box"), [v](StudioModel& m) { m.clipMin = v; }); });
    onVector(m_clipMax, [onModel](const aiVector3D& v) { onModel(QT_TR_NOOP("Clipping Box"), [v](StudioModel& m) { m.clipMax = v; }); });
    for (const auto& [box, flag] : m_modelFlags)
    {
        const int bit = flag;
        connect(box, &QCheckBox::toggled, this, [onModel, bit](bool on) {
            onModel(QT_TR_NOOP("Flags"), [on, bit](StudioModel& m) { m.flags = on ? (m.flags | bit) : (m.flags & ~bit); });
        });
    }

    connect(m_boneName, &QLineEdit::textEdited, this, [onBone](const QString& text) {
        onBone(QT_TR_NOOP("Bone Name"), [text](StudioBone& b) { b.name = text.toStdString(); });
    });
    // value and scale: position     [0..2], angles [3..5]
    auto boneTriple = [onBone](const char* what, bool scale, int first) {
        return [onBone, what, scale, first](const aiVector3D& v) {
            onBone(what, [v, scale, first](StudioBone& b) {
                float* into = scale ? b.scale : b.value;
                into[first] = v.x;
                into[first + 1] = v.y;
                into[first + 2] = v.z;
            });
        };
    };
    onVector(m_bonePosition, boneTriple(QT_TR_NOOP("Position"), false, 0));
    onVector(m_bonePositionScale, boneTriple(QT_TR_NOOP("Position Scale"), true, 0));
    onVector(m_boneRotation, boneTriple(QT_TR_NOOP("Rotation"), false, 3));
    onVector(m_boneRotationScale, boneTriple(QT_TR_NOOP("Rotation Scale"), true, 3));

    connect(m_controllerStart, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [onController](double value) {
        onController(QT_TR_NOOP("Start"), [value](StudioBoneController& c) { c.start = static_cast<float>(value); });
    });
    connect(m_controllerEnd, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [onController](double value) {
        onController(QT_TR_NOOP("End"), [value](StudioBoneController& c) { c.end = static_cast<float>(value); });
    });
    connect(m_controllerBone, &QComboBox::currentIndexChanged, this, [onController](int bone) {
        if (bone >= 0) onController(QT_TR_NOOP("Bone"), [bone](StudioBoneController& c) { c.bone = bone; });
    });
    connect(m_controllerRest, QOverload<int>::of(&QSpinBox::valueChanged), this, [onController](int rest) {
        onController(QT_TR_NOOP("Rest"), [rest](StudioBoneController& c) { c.rest = rest; });
    });
    connect(m_controllerIndex, &QComboBox::currentIndexChanged, this, [onController](int index) {
        if (index >= 0) onController(QT_TR_NOOP("Index"), [index](StudioBoneController& c) { c.index = index; });
    });
    connect(m_controllerAxis, &QComboBox::currentIndexChanged, this, [onController](int axis) {
        // motion flags hold the axis plus the wrap flag
        if (axis >= 0) onController(QT_TR_NOOP("Axis"), [axis](StudioBoneController& c) { c.type = (c.type & ~0x3f) | (1 << axis); });
    });

    connect(m_attachmentName, &QLineEdit::textEdited, this, [onAttachment](const QString& text) {
        onAttachment(QT_TR_NOOP("Name"), [text](StudioAttachment& a) { a.name = text.toStdString(); });
    });
    connect(m_attachmentBone, &QComboBox::currentIndexChanged, this, [onAttachment](int bone) {
        if (bone >= 0) onAttachment(QT_TR_NOOP("Bone"), [bone](StudioAttachment& a) { a.bone = bone; });
    });
    onVector(m_attachmentOrigin, [onAttachment](const aiVector3D& v) { onAttachment(QT_TR_NOOP("Origin"), [v](StudioAttachment& a) { a.origin = v; }); });

    connect(m_hitboxBone, &QComboBox::currentIndexChanged, this, [onHitbox](int bone) {
        if (bone >= 0) onHitbox(QT_TR_NOOP("Bone"), [bone](StudioHitbox& h) { h.bone = bone; });
    });
    connect(m_hitboxGroup, QOverload<int>::of(&QSpinBox::valueChanged), this, [onHitbox](int group) {
        onHitbox(QT_TR_NOOP("Hitgroup"), [group](StudioHitbox& h) { h.group = group; });
    });
    onVector(m_hitboxMin, [onHitbox](const aiVector3D& v) { onHitbox(QT_TR_NOOP("Minimum"), [v](StudioHitbox& h) { h.min = v; }); });
    onVector(m_hitboxMax, [onHitbox](const aiVector3D& v) { onHitbox(QT_TR_NOOP("Maximum"), [v](StudioHitbox& h) { h.max = v; }); });
}

void ModelViewerWindow::refreshPanels()
{
    const Filling filling(m_filling);
    const Asset* shown = asset();
    if (!shown) return;
    const StudioModel& data = *shown->model;
    // names may have been edited
    auto rename = [](QComboBox* combo, const std::function<QString(int)>& name) {
        const QSignalBlocker block(combo);
        for (int i = 0; i < combo->count(); ++i) combo->setItemText(i, name(i));
    };
    rename(m_sequenceCombo, [&](int i) { return indexed(data.sequences[i].name, i); });
    rename(m_textureCombo, [&](int i) { return textureLabel(data.textures[i]); });
    for (QComboBox* bones : { m_boneCombo, m_controllerBone, m_attachmentBone, m_hitboxBone })
        rename(bones, [&](int i) { return indexed(data.bones[i].name, i); });
    if (m_attachmentCombo->count() != static_cast<int>(data.attachments.size())) fillAttachments();
    rename(m_attachmentCombo, [&](int i) {
        return data.attachments[i].name.empty() ? tr("Attachment %1").arg(i + 1) : QString::fromStdString(data.attachments[i].name);
    });
    showModelData();
    const int event = m_eventCombo->currentIndex();
    fillSequence(m_sequenceCombo->currentIndex());
    if (event >= 0 && event < m_eventCombo->count())
    {
        {
            const QSignalBlocker block(m_eventCombo);
            m_eventCombo->setCurrentIndex(event);
        }
        showSequenceEvent(event);
    }
    showSubModel(m_subModelCombo->currentIndex());
    showTexture();
    showBone(m_boneCombo->currentIndex());
    showController(m_controllerCombo->currentIndex());
    showAttachment(m_attachmentCombo->currentIndex());
    showHitbox(m_hitboxCombo->currentIndex());
}

void ModelViewerWindow::updateTabTitle(int index)
{
    if (index < 0 || index >= static_cast<int>(m_assets.size())) return;
    const Asset& tab = m_assets[index];
    // a star marks unsaved changes
    const QString title = QFileInfo(tab.path).fileName() + (tab.undo && !tab.undo->isClean() ? QStringLiteral("*") : QString());
    m_tabs->setTabText(index, title);
    m_tabs->setTabToolTip(index, QDir::toNativeSeparators(tab.path));
    if (index == m_current) setWindowTitle(title);
}

bool ModelViewerWindow::saveAs(const QString& path)
{
    Asset* shown = asset();
    if (!shown) return false;
    const QString whole = QFileInfo(path).absoluteFilePath();
    std::string error;
    if (!shown->model->Save(whole.toStdString(), error))
    {
        VortigauntLog::Vortigaunt_Printf(QStringLiteral("ERROR: Could not save %1: %2").arg(QDir::toNativeSeparators(whole), QString::fromStdString(error)));
        QMessageBox::warning(this, tr("Save"), tr("Could not save %1.").arg(QFileInfo(whole).fileName()));
        return false;
    }
    VortigauntLog::Vortigaunt_Printf(QStringLiteral("Model saved: %1").arg(QDir::toNativeSeparators(whole)));
    shown->path = whole;
    if (shown->undo) shown->undo->setClean();
    updateTabTitle(m_current);
    return true;
}

void ModelViewerWindow::saveCurrent()
{
    if (const Asset* shown = asset()) saveAs(shown->path);
}

void ModelViewerWindow::saveCurrentAs()
{
    const Asset* shown = asset();
    if (!shown) return;
    const QString path = QFileDialog::getSaveFileName(this, tr("Save As"), shown->path, tr("Studio models (*.mdl)"));
    if (!path.isEmpty()) saveAs(path);
}

bool ModelViewerWindow::confirmClose(int index)
{
    if (index < 0 || index >= static_cast<int>(m_assets.size()) || !m_assets[index].undo || m_assets[index].undo->isClean()) return true;
    m_tabs->setCurrentIndex(index);
    const QMessageBox::StandardButton answer = QMessageBox::question(
        this, tr("Unsaved changes"), tr("%1 has changes that are not saved. Save them?").arg(QFileInfo(m_assets[index].path).fileName()),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (answer == QMessageBox::Save) return saveAs(m_assets[index].path);
    return answer == QMessageBox::Discard;
}

bool ModelViewerWindow::closeAll()
{
    for (int i = 0; i < static_cast<int>(m_assets.size()); ++i)
        if (!confirmClose(i)) return false;
    // discarded changes are not asked about again
    for (Asset& tab : m_assets)
        if (tab.undo) tab.undo->setClean();
    return true;
}

void ModelViewerWindow::closeEvent(QCloseEvent* event)
{
    if (!closeAll())
    {
        event->ignore();
        return;
    }
    QMainWindow::closeEvent(event);
}

void ModelViewerWindow::showModelData()
{
    const Filling filling(m_filling);
    const Asset* shown = asset();
    if (!shown) return;
    const StudioModel& data = *shown->model;
    m_rootBones->setText(QString::number(std::count_if(data.bones.begin(), data.bones.end(), [](const StudioBone& bone) { return bone.parent < 0; })));
    m_eyePosition.set(data.eyePosition);
    m_boundsMin.set(data.boundsMin);
    m_boundsMax.set(data.boundsMax);
    m_clipMin.set(data.clipMin);
    m_clipMax.set(data.clipMax);
    for (const auto& [box, flag] : m_modelFlags) box->setChecked((data.flags & flag) != 0);

    // Dump Model Info text
    QString text = QStringLiteral("File: %1\n").arg(QDir::toNativeSeparators(shown->path));
    text += QStringLiteral("Bones: %1, sequences: %2, textures: %3, skins: %4\n").arg(data.bones.size()).arg(data.sequences.size()).arg(data.textures.size()).arg(data.skinFamilies.size());
    text += QStringLiteral("Attachments: %1, hitboxes: %2, bone controllers: %3\n").arg(data.attachments.size()).arg(data.hitboxes.size()).arg(data.controllers.size());
    auto vector = [](const aiVector3D& v) { return QStringLiteral("%1 %2 %3").arg(qcNumber(v.x), qcNumber(v.y), qcNumber(v.z)); };
    text += QStringLiteral("Eye position: %1\n").arg(vector(data.eyePosition));
    text += QStringLiteral("Bounding box: %1 to %2\n").arg(vector(data.boundsMin), vector(data.boundsMax));
    text += QStringLiteral("Clipping box: %1 to %2\n").arg(vector(data.clipMin), vector(data.clipMax));
    text += QStringLiteral("Flags: 0x%1\n").arg(data.flags, 0, 16);
    for (const StudioBodyPart& part : data.bodyParts)
    {
        text += QStringLiteral("Body part %1:\n").arg(QString::fromStdString(part.name));
        for (const StudioSubModel& sub : part.models)
        {
            int triangles = 0;
            for (const StudioMesh& mesh : sub.meshes) triangles += static_cast<int>(mesh.triangles.size()) / 3;
            text += QStringLiteral("    %1: %2 meshes, %3 vertices, %4 triangles\n").arg(QString::fromStdString(sub.name)).arg(sub.meshes.size()).arg(sub.vertices.size()).arg(triangles);
        }
    }
    for (size_t i = 0; i < data.sequences.size(); ++i)
        text += QStringLiteral("Sequence %1: %2, %3 frames at %4 fps\n").arg(i).arg(QString::fromStdString(data.sequences[i].name)).arg(data.sequences[i].frames).arg(data.sequences[i].fps);
    m_modelInfo = text;
}

void ModelViewerWindow::showSequence(int index)
{
    Asset* shown = asset();
    if (!shown || index < 0 || index >= static_cast<int>(shown->model->sequences.size())) return;
    shown->sequence = index;
    m_view->setSequence(0, index);
    fillSequence(index);
}

void ModelViewerWindow::fillSequence(int index)
{
    const Filling filling(m_filling);
    const Asset* shown = asset();
    if (!shown || index < 0 || index >= static_cast<int>(shown->model->sequences.size())) return;
    const StudioSequence& sequence = shown->model->sequences[index];
    const int last = std::max(0, sequence.frames - 1);
    m_frameSlider->setRange(0, last * kFrameSteps);
    {
        const QSignalBlocker block(m_frameSpin);
        m_frameSpin->setRange(0.0, last);
    }
    m_blendCount->setText(QString::number(sequence.blends));
    m_duration->setText(QString::number(sequence.fps > 0.0f ? sequence.frames / sequence.fps : 0.0f, 'f', 2));
    m_frameCount->setText(QString::number(sequence.frames));
    m_blendSlider->setEnabled(sequence.blends > 1);
    m_blendSpin->setEnabled(sequence.blends > 1);
    {
        const QSignalBlocker block(m_blendSpin);
        m_blendSpin->setRange(std::min(sequence.blendStart, sequence.blendEnd), std::max(sequence.blendStart, sequence.blendEnd));
    }
    const float degrees = sequence.blendStart + (sequence.blendEnd - sequence.blendStart) * m_blendSlider->value() / kBlendSteps;
    {
        const QSignalBlocker block(m_blendSpin);
        m_blendSpin->setValue(degrees);
    }
    m_view->setAimPitch(0, degrees);
    m_isLooping->setChecked((sequence.flags & 1) != 0);
    m_sequenceFps->setValue(sequence.fps);
    m_activity->setValue(sequence.activity);
    m_activityWeight->setValue(sequence.activityWeight);
    m_linearMovement.set(sequence.linearMovement);
    {
        const QSignalBlocker block(m_eventCombo);
        m_eventCombo->clear();
        fillCombo(m_eventCombo, sequence.events, [this](size_t i) { return tr("Event %1").arg(i + 1); });
    }
    m_eventCombo->setCurrentIndex(sequence.events.empty() ? -1 : 0);
    showSequenceEvent(m_eventCombo->currentIndex());
}

void ModelViewerWindow::showSequenceEvent(int index)
{
    const Filling filling(m_filling);
    const Asset* shown = asset();
    const StudioEvent* event = nullptr;
    if (shown && shown->sequence >= 0 && shown->sequence < static_cast<int>(shown->model->sequences.size()))
    {
        const std::vector<StudioEvent>& events = shown->model->sequences[shown->sequence].events;
        if (index >= 0 && index < static_cast<int>(events.size())) event = &events[index];
    }
    m_eventFrame->setValue(event ? event->frame : 0);
    m_eventId->setValue(event ? event->event : 0);
    showText(m_eventOptions, event ? QString::fromStdString(event->options) : QString());
}

void ModelViewerWindow::showPart(int part)
{
    const Asset* shown = asset();
    {
        const QSignalBlocker block(m_subModelCombo);
        m_subModelCombo->clear();
        if (!shown || part < 0 || part >= static_cast<int>(shown->model->bodyParts.size())) return;
        const StudioBodyPart& body = shown->model->bodyParts[part];
        fillCombo(m_subModelCombo, body.models, [&](size_t i) { return tr("Submodel %1").arg(i + 1); });
        m_subModelCombo->setCurrentIndex(shown->body[part]);
    }
    showSubModel(shown->body[part]);
}

void ModelViewerWindow::showSubModel(int sub)
{
    const Filling filling(m_filling);
    Asset* shown = asset();
    const int part = m_partCombo->currentIndex();
    if (!shown || part < 0 || part >= static_cast<int>(shown->body.size())) return;
    const StudioBodyPart& body = shown->model->bodyParts[part];
    if (sub < 0 || sub >= static_cast<int>(body.models.size())) return;
    shown->body[part] = sub;
    m_view->setBody(0, part, sub);
    const StudioSubModel& model = body.models[sub];
    showText(m_subModelName, QString::fromStdString(model.name));
    m_meshCount->setText(QString::number(model.meshes.size()));
    m_vertexCount->setText(QString::number(model.vertices.size()));
    m_normalCount->setText(QString::number(model.normals.size()));
    // body value: each part's choice times the sizes of the parts before it
    int value = 0, base = 1;
    for (size_t i = 0; i < shown->body.size(); ++i)
    {
        value += shown->body[i] * base;
        base *= std::max<int>(1, static_cast<int>(shown->model->bodyParts[i].models.size()));
    }
    m_bodyValue->setText(QString::number(value));
}

QImage ModelViewerWindow::textureView(int index, double scale, bool picture, bool lines)
{
    const Asset* shown = asset();
    if (!shown || index < 0 || index >= static_cast<int>(shown->model->textures.size())) return QImage();
    const StudioTexture& texture = shown->model->textures[index];
    QImage image = StudioModelView::textureImage(texture, m_topSpin->value(), m_bottomSpin->value());
    if (image.isNull()) return image;
    const QSize size(static_cast<int>(texture.width * scale), static_cast<int>(texture.height * scale));
    image = picture ? image.scaled(size, Qt::IgnoreAspectRatio, Qt::FastTransformation).convertToFormat(QImage::Format_ARGB32) : QImage(size, QImage::Format_ARGB32);
    if (!picture) image.fill(Qt::black);
    if (!lines) return image;
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, m_smoothUv->isChecked());
    painter.setPen(QColor(255, 255, 255, 200));
    // first mesh item = all meshes of the texture
    const int onlyMesh = m_meshCombo->currentIndex() - 1;
    int meshNumber = -1;
    for (const StudioBodyPart& part : shown->model->bodyParts)
        for (const StudioSubModel& sub : part.models)
            for (const StudioMesh& mesh : sub.meshes)
            {
                if (mesh.texture != index) continue;
                ++meshNumber;
                if (onlyMesh >= 0 && meshNumber != onlyMesh) continue;
                for (size_t i = 0; i + 2 < mesh.triangles.size(); i += 3)
                {
                    QPointF corner[3];
                    for (int k = 0; k < 3; ++k) corner[k] = QPointF(mesh.triangles[i + k].u * size.width(), mesh.triangles[i + k].v * size.height());
                    painter.drawPolygon(corner, 3);
                }
            }
    return image;
}

void ModelViewerWindow::showTexture()
{
    const Filling filling(m_filling);
    const Asset* shown = asset();
    const int index = m_textureCombo->currentIndex();
    if (!shown || index < 0 || index >= static_cast<int>(shown->model->textures.size()))
    {
        m_textureName->clear();
        for (const auto& [box, flag] : m_textureFlags) box->setChecked(false);
        return;
    }
    const StudioTexture& texture = shown->model->textures[index];
    showText(m_textureName, QString::fromStdString(texture.name));
    for (const auto& [box, flag] : m_textureFlags) box->setChecked((texture.flags & flag) != 0);
    // UV map alone is drawn on black
    const bool lines = m_showUv->isChecked();
    const QImage image = textureView(index, m_scaleSpin->value(), !lines || m_overlayUv->isChecked(), lines);
    if (!image.isNull()) m_textureImage->setPixmap(QPixmap::fromImage(image));
}

void ModelViewerWindow::showBone(int index)
{
    const Filling filling(m_filling);
    const Asset* shown = asset();
    if (!shown || index < 0 || index >= static_cast<int>(shown->model->bones.size()))
    {
        m_boneName->clear();
        m_parentBone->clear();
        return;
    }
    const StudioBone& bone = shown->model->bones[index];
    showText(m_boneName, QString::fromStdString(bone.name));
    m_parentBone->setText(bone.parent >= 0 ? indexed(shown->model->bones[bone.parent].name, bone.parent) : tr("None (-1)"));
    m_bonePosition.set(aiVector3D(bone.value[0], bone.value[1], bone.value[2]));
    m_bonePositionScale.set(aiVector3D(bone.scale[0], bone.scale[1], bone.scale[2]));
    m_boneRotation.set(aiVector3D(bone.value[3], bone.value[4], bone.value[5]));
    m_boneRotationScale.set(aiVector3D(bone.scale[3], bone.scale[4], bone.scale[5]));
    applyHighlight();
}

void ModelViewerWindow::showController(int index)
{
    const Filling filling(m_filling);
    const Asset* shown = asset();
    if (!shown || index < 0 || index >= static_cast<int>(shown->controllers.size())) return;
    const StudioBoneController& picked = shown->model->controllers[index];
    m_controllerStart->setValue(picked.start);
    m_controllerEnd->setValue(picked.end);
    m_controllerBone->setCurrentIndex(picked.bone);
    m_controllerRest->setValue(picked.rest);
    m_controllerIndex->setCurrentIndex(std::clamp(picked.index, 0, m_controllerIndex->count() - 1));
    // motion flag bit = axis
    int axis = 0;
    while (axis < 5 && !(picked.type & (1 << axis))) ++axis;
    m_controllerAxis->setCurrentIndex(axis);
    {
        const QSignalBlocker block(m_controllerSpin);
        m_controllerSpin->setRange(std::min(picked.start, picked.end), std::max(picked.start, picked.end));
    }
    setControllerValue(shown->controllers[index]);
}

void ModelViewerWindow::setControllerValue(float value)
{
    Asset* shown = asset();
    const int index = m_controllerCombo->currentIndex();
    if (!shown || index < 0 || index >= static_cast<int>(shown->controllers.size())) return;
    const StudioBoneController& picked = shown->model->controllers[index];
    // all mouth controllers share one value
    for (size_t i = 0; i < shown->controllers.size(); ++i)
        if (static_cast<int>(i) == index || (picked.index == 4 && shown->model->controllers[i].index == 4)) shown->controllers[i] = value;
    m_view->setControllers(0, shown->controllers);
    const float reach = picked.end - picked.start;
    const QSignalBlocker blockSlider(m_controllerSlider), blockSpin(m_controllerSpin);
    m_controllerSlider->setValue(reach != 0.0f ? std::clamp(static_cast<int>(std::lround((value - picked.start) / reach * kControllerSteps)), 0, kControllerSteps) : 0);
    m_controllerSpin->setValue(value);
}

void ModelViewerWindow::showAttachment(int index)
{
    const Filling filling(m_filling);
    const Asset* shown = asset();
    const int count = shown ? static_cast<int>(shown->model->attachments.size()) : 0;
    m_attachmentLimit->setText(tr("GoldSrc supports at most %1 attachments; this model has %2. Please decrease to 4 attachments. otherwise the engine will crash.").arg(kStudioMaxAttachments).arg(count));
    m_attachmentLimit->setVisible(count > kStudioMaxAttachments);
    if (!shown || index < 0 || index >= static_cast<int>(shown->model->attachments.size()))
    {
        m_attachmentName->clear();
        m_attachmentQc->clear();
        return;
    }
    const StudioAttachment& attachment = shown->model->attachments[index];
    showText(m_attachmentName, QString::fromStdString(attachment.name));
    m_attachmentBone->setCurrentIndex(attachment.bone);
    m_attachmentOrigin.set(attachment.origin);
    const QString bone = QString::fromStdString(shown->model->bones[std::clamp(attachment.bone, 0, static_cast<int>(shown->model->bones.size()) - 1)].name);
    m_attachmentQc->setText(QStringLiteral("$attachment %1 \"%2\" %3 %4 %5").arg(index).arg(bone, qcNumber(attachment.origin.x), qcNumber(attachment.origin.y), qcNumber(attachment.origin.z)));
    applyHighlight();
}

void ModelViewerWindow::fillAttachments()
{
    const Asset* shown = asset();
    if (!shown) return;
    const StudioModel& data = *shown->model;
    const int was = m_attachmentCombo->currentIndex();
    const QSignalBlocker block(m_attachmentCombo);
    m_attachmentCombo->clear();
    fillCombo(m_attachmentCombo, data.attachments, [&](size_t i) {
        return data.attachments[i].name.empty() ? tr("Attachment %1").arg(i + 1) : QString::fromStdString(data.attachments[i].name);
    });
    m_attachmentCombo->setCurrentIndex(std::min(std::max(was, 0), m_attachmentCombo->count() - 1));
}

void ModelViewerWindow::showHitbox(int index)
{
    const Filling filling(m_filling);
    const Asset* shown = asset();
    if (!shown || index < 0 || index >= static_cast<int>(shown->model->hitboxes.size()))
    {
        m_hitboxQc->clear();
        return;
    }
    const StudioHitbox& hitbox = shown->model->hitboxes[index];
    m_hitboxBone->setCurrentIndex(hitbox.bone);
    m_hitboxGroup->setValue(hitbox.group);
    m_hitboxMin.set(hitbox.min);
    m_hitboxMax.set(hitbox.max);
    const QString bone = QString::fromStdString(shown->model->bones[std::clamp(hitbox.bone, 0, static_cast<int>(shown->model->bones.size()) - 1)].name);
    m_hitboxQc->setText(QStringLiteral("$hbox %1 \"%2\" %3 %4 %5 %6 %7 %8")
                            .arg(hitbox.group)
                            .arg(bone, qcNumber(hitbox.min.x), qcNumber(hitbox.min.y), qcNumber(hitbox.min.z), qcNumber(hitbox.max.x), qcNumber(hitbox.max.y), qcNumber(hitbox.max.z)));
    applyHighlight();
}

void ModelViewerWindow::applyHighlight()
{
    m_view->setHighlight(m_highlightBone->isChecked() ? m_boneCombo->currentIndex() : -1,
                         m_highlightAttachment->isChecked() ? m_attachmentCombo->currentIndex() : -1,
                         m_highlightHitbox->isChecked() ? m_hitboxCombo->currentIndex() : -1);
}

void ModelViewerWindow::applyDisplay()
{
    StudioModelView::Display display;
    // list order is the reverse of the view's modes
    display.mode = StudioModelView::Display::Wireframe - m_modeCombo->currentIndex();
    display.opacity = m_opacitySlider->value() / 100.0f;
    for (const auto& [check, member] : m_displayChecks) display.*member = check->isChecked();
    display.ground = m_showGround->isChecked();
    display.aspectWidth = m_aspectWidth->value();
    display.aspectHeight = m_aspectHeight->value();
    m_view->setDisplay(display);
}

void ModelViewerWindow::applyPlacement()
{
    const aiVector3D place = m_origin.value();
    aiMatrix4x4 placement;
    placement.a4 = place.x;
    placement.b4 = place.y;
    placement.c4 = place.z;
    m_view->setPlacement(0, placement);
}

void ModelViewerWindow::applyCamera()
{
    const int camera = std::clamp(m_cameraCombo->currentIndex(), 0, 2);
    m_cameraStack->setCurrentIndex(camera);
    m_view->setCamera(kCameras[camera]);
    m_view->setFieldOfView(static_cast<float>(m_fovSpins[camera]->value()));
    m_view->setOrthographic(camera == 0 && m_projectionCombo->currentIndex() == 1);
}

void ModelViewerWindow::applyCameraPlace()
{
    StudioModelView::ViewPlace place = m_view->viewPlace();
    const int camera = m_cameraCombo->currentIndex();
    if (camera == 1) return;
    const int which = camera == 0 ? 0 : 1;
    place.pitch = static_cast<float>(m_pitchSpins[which]->value());
    place.yaw = static_cast<float>(m_yawSpins[which]->value());
    if (camera == 0)
    {
        const aiVector3D center = m_cameraCenter.value();
        place.target = QVector3D(center.x, center.y, center.z);
        place.distance = static_cast<float>(m_distanceSpin->value());
    }
    else
    {
        const aiVector3D eye = m_freeOrigin.value();
        place.freeEye = QVector3D(eye.x, eye.y, eye.z);
    }
    m_view->setViewPlace(place);
}

void ModelViewerWindow::stepFrames(int frames)
{
    m_playButton->setChecked(false);
    m_view->setFramePosition(static_cast<float>(std::max(0, m_view->frame() + frames)));
    refresh();
}

void ModelViewerWindow::centerOn(float yaw, float pitch)
{
    m_cameraCombo->setCurrentIndex(0);
    m_view->setCameraAngles(yaw, pitch);
    m_view->fitView();
}

void ModelViewerWindow::refresh()
{
    if (!m_view->isVisible() && !testAttribute(Qt::WA_DontShowOnScreen)) return;
    playEvents();
    const float position = m_view->framePosition();
    if (!m_frameSlider->isSliderDown())
    {
        const QSignalBlocker block(m_frameSlider);
        m_frameSlider->setValue(static_cast<int>(std::lround(position * kFrameSteps)));
    }
    if (!m_frameSpin->hasFocus())
    {
        const QSignalBlocker block(m_frameSpin);
        m_frameSpin->setValue(position);
    }
    // update counts once a second
    if (++m_refreshTicks % (1000 / kRefreshMs) == 0)
    {
        m_fpsLabel->setText(QString::number(m_view->framesPerSecond(), 'f', 0));
        m_polygonsLabel->setText(QString::number(m_view->drawnTriangles()));
    }

    // camera fields follow the mouse unless being edited
    const QWidget* focus = QApplication::focusWidget();
    if (focus && m_cameraStack->isAncestorOf(focus)) return;
    const StudioModelView::ViewPlace place = m_view->viewPlace();
    for (int which = 0; which < 2; ++which)
    {
        const QSignalBlocker blockPitch(m_pitchSpins[which]), blockYaw(m_yawSpins[which]);
        m_pitchSpins[which]->setValue(place.pitch);
        m_yawSpins[which]->setValue(std::fmod(std::fmod(place.yaw, 360.0f) + 360.0f, 360.0f));
    }
    m_cameraCenter.set(aiVector3D(place.target.x(), place.target.y(), place.target.z()));
    m_freeOrigin.set(aiVector3D(place.freeEye.x(), place.freeEye.y(), place.freeEye.z()));
    const QSignalBlocker blockDistance(m_distanceSpin);
    m_distanceSpin->setValue(place.distance);
}

void ModelViewerWindow::openFiles()
{
    const QStringList paths = QFileDialog::getOpenFileNames(this, tr("Open a model"), QString(), tr("Studio models (*.mdl);;All files (*.*)"));
    for (const QString& path : paths) load(path);
}

void ModelViewerWindow::saveScreenshot()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Save screenshot"), QString(), tr("Images (*.png *.jpg *.bmp)"));
    if (path.isEmpty()) return;
    if (!m_view->grabFramebuffer().save(path)) VortigauntLog::Vortigaunt_Printf(QStringLiteral("ERROR: Could not write %1").arg(QDir::toNativeSeparators(path)));
}

void ModelViewerWindow::dumpInfo()
{
    const Asset* shown = asset();
    if (!shown) return;
    const QString path = QFileDialog::getSaveFileName(this, tr("Save model info"), QFileInfo(shown->path).completeBaseName() + QStringLiteral(".txt"), tr("Text files (*.txt)"));
    if (path.isEmpty()) return;
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        file.write(m_modelInfo.toUtf8());
    else
        VortigauntLog::Vortigaunt_Printf(QStringLiteral("ERROR: Could not write %1").arg(QDir::toNativeSeparators(path)));
}

void ModelViewerWindow::exportTexture()
{
    const Asset* shown = asset();
    const int index = m_textureCombo->currentIndex();
    if (!shown || index < 0 || index >= static_cast<int>(shown->model->textures.size())) return;
    const StudioTexture& texture = shown->model->textures[index];
    const QString path = QFileDialog::getSaveFileName(this, tr("Export the texture"), QString::fromStdString(texture.name), tr("Images (*.bmp *.png)"));
    if (path.isEmpty()) return;
    if (!StudioModelView::textureImage(texture).convertToFormat(QImage::Format_RGB32).save(path))
        VortigauntLog::Vortigaunt_Printf(QStringLiteral("ERROR: Could not write %1").arg(QDir::toNativeSeparators(path)));
}

void ModelViewerWindow::exportUvMap()
{
    const Asset* shown = asset();
    const int index = m_textureCombo->currentIndex();
    if (!shown || index < 0 || index >= static_cast<int>(shown->model->textures.size())) return;
    const QString name = QFileInfo(QString::fromStdString(shown->model->textures[index].name)).completeBaseName() + QStringLiteral("_uv.png");
    const QString path = QFileDialog::getSaveFileName(this, tr("Export UV Map"), name, tr("Images (*.png *.bmp)"));
    if (path.isEmpty()) return;
    if (!textureView(index, 1.0, false, true).save(path))
        VortigauntLog::Vortigaunt_Printf(QStringLiteral("ERROR: Could not write %1").arg(QDir::toNativeSeparators(path)));
}

void ModelViewerWindow::exportAllTextures()
{
    const Asset* shown = asset();
    if (!shown) return;
    const QString folder = QFileDialog::getExistingDirectory(this, tr("Export the textures to"));
    if (folder.isEmpty()) return;
    int written = 0;
    for (const StudioTexture& texture : shown->model->textures)
        if (StudioModelView::textureImage(texture).convertToFormat(QImage::Format_RGB32).save(QDir(folder).filePath(QFileInfo(QString::fromStdString(texture.name)).completeBaseName() + QStringLiteral(".bmp"))))
            ++written;
    VortigauntLog::Vortigaunt_Printf(QStringLiteral("%1 textures exported to %2").arg(written).arg(QDir::toNativeSeparators(folder)));
}
