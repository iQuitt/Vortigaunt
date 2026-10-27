// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#include "WadMakerDialog.h"
#include "WadMaker.h"
#include "ui/UiUtils.h"
#include "ui/widgets/PaletteWidget.h"
#include "ui/widgets/TextureView.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QInputDialog>
#include <QAction>
#include <QButtonGroup>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QSlider>
#include <QUrl>
#include <QCheckBox>
#include <QComboBox>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSpinBox>
#include <QTimer>
#include <QToolButton>

#include <algorithm>
#include <map>

namespace {

// GoldSrc plays animated textures at 10 frames a second
constexpr int kAnimationIntervalMs = 100;
constexpr int kMipPreviewSize = 64;
constexpr int kListIconSize = 32;
// the id of the "All" type filter; QButtonGroup takes -1 as "pick an id"
constexpr int kAllKinds = 100;

const QStringList kImageExtensions = { "bmp", "png", "jpg", "jpeg", "tga", "dds" };

bool isImageFile(const QString& path)
{
    return kImageExtensions.contains(QFileInfo(path).suffix().toLower());
}

QStringList droppedFiles(const QMimeData* mime)
{
    QStringList files;
    if (mime && mime->hasUrls())
    {
        for (const QUrl& url : mime->urls())
        {
            if (url.isLocalFile())
                files << url.toLocalFile();
        }
    }
    return files;
}

} // namespace

WadMakerDialog::WadMakerDialog(QWidget* parent)
    : QDialog(parent)
    , m_wadMaker(new WadArchive())
    , m_modified(false)
{
    setWindowTitle(tr("WAD Editor"));
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint | Qt::WindowMinimizeButtonHint);
    setMinimumSize(600, 400);

    UiUtils::resizeToScreen(this, 0.7);

    setupUi();
    refreshTextureList();
}

WadMakerDialog::~WadMakerDialog()
{
    delete m_wadMaker;
}

void WadMakerDialog::setupUi()
{
    auto action = [this](const QString& text, const QString& tip) {
        auto* created = new QAction(text, this);
        created->setToolTip(tip);
        addAction(created);
        return created;
    };
    m_newAction = action(tr("New"), tr("Create new WAD"));
    m_openAction = action(tr("Open WAD"), tr("Open existing WAD file"));
    m_saveAction = action(tr("Save"), tr("Save WAD file"));
    m_saveAction->setEnabled(false);
    m_saveAsAction = action(tr("Save As"), tr("Save WAD to new file"));
    m_addAction = action(tr("Add images"), tr("Add images as textures; they are fitted into 512 x 512 and 256 colors"));
    m_removeAction = action(tr("Remove"), tr("Remove the selected textures"));
    m_extractAction = action(tr("Extract"), tr("Save the selected textures as images"));
    m_renameAction = action(tr("Rename"), tr("Rename the selected texture (F2, or double-click)"));
    m_replaceAction = action(tr("Replace image"), tr("Give the selected texture the picture of another image; its name stays"));
    m_removeAction->setEnabled(false);
    m_extractAction->setEnabled(false);
    m_renameAction->setEnabled(false);
    m_replaceAction->setEnabled(false);
    m_renameAction->setShortcut(Qt::Key_F2);
    m_removeAction->setShortcut(QKeySequence::Delete);
    for (QAction* shortcut : { m_renameAction, m_removeAction })
        shortcut->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    setAcceptDrops(true);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    mainLayout->addWidget(splitter, 1);

    // left: the textures
    auto* left = new QWidget(splitter);
    auto* leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(4, 4, 4, 4);
    // search, and the type: all, { ! + ~
    auto* filterRow = new QHBoxLayout();
    m_filterEdit = new QLineEdit(left);
    m_filterEdit->setPlaceholderText(tr("Type to filter textures..."));
    m_filterEdit->setClearButtonEnabled(true);
    filterRow->addWidget(m_filterEdit, 1);
    m_kindFilter = new QButtonGroup(this);
    const struct { const char* text; int kind; const char* tip; } kinds[] = {
        { QT_TR_NOOP("All"), kAllKinds, QT_TR_NOOP("Every texture") },
        { "{", static_cast<int>(Kind::Masked), QT_TR_NOOP("Masked textures") },
        { "!", static_cast<int>(Kind::Water), QT_TR_NOOP("Water textures") },
        { "+", static_cast<int>(Kind::Animated), QT_TR_NOOP("Animated and toggled textures") },
        { "~", static_cast<int>(Kind::Light), QT_TR_NOOP("Light textures") },
    };
    for (const auto& kind : kinds)
    {
        auto* button = new QToolButton(left);
        button->setText(kind.kind == kAllKinds ? tr(kind.text) : QString::fromLatin1(kind.text));
        button->setToolTip(tr(kind.tip));
        button->setCheckable(true);
        button->setAutoRaise(true);
        button->setMinimumWidth(28);
        m_kindFilter->addButton(button, kind.kind);
        filterRow->addWidget(button);
    }
    m_kindFilter->button(kAllKinds)->setChecked(true);
    leftLayout->addLayout(filterRow);

    auto* orderRow = new QHBoxLayout();
    orderRow->addWidget(new QLabel(tr("Sort:"), left));
    m_sortCombo = new QComboBox(left);
    m_sortCombo->addItem(tr("Name"));
    m_sortCombo->addItem(tr("Size"));
    m_sortCombo->addItem(tr("Type"));
    orderRow->addWidget(m_sortCombo);
    orderRow->addStretch();
    m_sizeSlider = new QSlider(Qt::Horizontal, left);
    m_sizeSlider->setRange(32, 128);
    m_sizeSlider->setSingleStep(16);
    m_sizeSlider->setPageStep(16);
    m_sizeSlider->setValue(m_thumbnailSize);
    m_sizeSlider->setMaximumWidth(120);
    m_sizeSlider->setToolTip(tr("Thumbnail size"));
    orderRow->addWidget(m_sizeSlider);
    m_listViewButton = new QToolButton(left);
    m_listViewButton->setText(tr("List"));
    m_listViewButton->setToolTip(tr("Show the textures as a list with their sizes"));
    m_listViewButton->setCheckable(true);
    m_listViewButton->setAutoRaise(true);
    orderRow->addWidget(m_listViewButton);
    leftLayout->addLayout(orderRow);

    m_textureGrid = new QListWidget(left);
    m_textureGrid->setObjectName("WadTextureGrid");
    m_textureGrid->setResizeMode(QListWidget::Adjust);
    m_textureGrid->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_textureGrid->setMovement(QListWidget::Static);
    m_textureGrid->setUniformItemSizes(true);
    m_textureGrid->setTextElideMode(Qt::ElideMiddle);
    m_textureGrid->setWordWrap(true);
    m_textureGrid->setContextMenuPolicy(Qt::CustomContextMenu);
    m_textureGrid->setToolTip(tr("Drop images here to add them, or a WAD to open it."));
    leftLayout->addWidget(m_textureGrid, 1);

    // what is done to images whose sides are not multiples of 16
    auto* fitRow = new QHBoxLayout();
    fitRow->addWidget(new QLabel(tr("New images:"), left));
    m_fitCombo = new QComboBox(left);
    m_fitCombo->addItem(tr("Scale to multiples of 16"), static_cast<int>(WadFit::Scale));
    m_fitCombo->addItem(tr("Keep size, see-through border"), static_cast<int>(WadFit::Pad));
    m_fitCombo->setToolTip(tr("GoldSrc needs sides that are multiples of 16. Scaling stretches the image a little; "
                              "the border keeps it as it is and makes the texture masked ({)."));
    fitRow->addWidget(m_fitCombo, 1);
    leftLayout->addLayout(fitRow);

    // centre: the selected texture large, as the game shows it
    auto* centre = new QWidget(splitter);
    auto* centreLayout = new QVBoxLayout(centre);
    centreLayout->setContentsMargins(4, 4, 4, 4);
    auto* viewRow = new QHBoxLayout();
    auto* fitButton = new QPushButton(tr("Fit"), centre);
    auto* actualButton = new QPushButton(tr("1:1"), centre);
    m_zoomLabel = new QLabel(centre);
    m_zoomLabel->setMinimumWidth(50);
    m_animateCheck = new QCheckBox(tr("Play animation"), centre);
    m_animateCheck->setToolTip(tr("Play the frames of this animated texture (+0 ... +9) at the game's 10 frames a second"));
    m_animateCheck->setVisible(false);
    viewRow->addWidget(fitButton);
    viewRow->addWidget(actualButton);
    viewRow->addWidget(m_zoomLabel);
    viewRow->addStretch();
    viewRow->addWidget(m_animateCheck);
    centreLayout->addLayout(viewRow);
    m_view = new TextureView(centre);
    m_view->placeholder = tr("Select a texture\nto preview");
    centreLayout->addWidget(m_view, 1);
    m_animationTimer = new QTimer(this);
    m_animationTimer->setInterval(kAnimationIntervalMs);

    // right: what GoldSrc makes of the texture
    auto* rightScroll = new QScrollArea(splitter);
    rightScroll->setWidgetResizable(true);
    rightScroll->setFrameShape(QFrame::NoFrame);
    rightScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    rightScroll->setMinimumWidth(300);
    auto* right = new QWidget();
    rightScroll->setWidget(right);
    auto* rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(4, 4, 4, 4);
    m_noSelectionLabel = new QLabel(tr("Select a texture to see its properties."), right);
    m_noSelectionLabel->setWordWrap(true);
    m_noSelectionLabel->setForegroundRole(QPalette::PlaceholderText);
    rightLayout->addWidget(m_noSelectionLabel);

    m_propertiesPanel = new QWidget(right);
    auto* propertiesLayout = new QVBoxLayout(m_propertiesPanel);
    propertiesLayout->setContentsMargins(0, 0, 0, 0);
    m_titleLabel = new QLabel(m_propertiesPanel);
    m_titleLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    propertiesLayout->addWidget(m_titleLabel);

    auto* nameGroup = new QGroupBox(tr("Name"), m_propertiesPanel);
    auto* nameLayout = new QGridLayout(nameGroup);
    m_nameEdit = new QLineEdit(nameGroup);
    m_nameEdit->setToolTip(tr("The name without its prefix; the type below adds the prefix. GoldSrc allows 15 characters in all."));
    m_nameCounter = new QLabel(nameGroup);
    m_nameCounter->setMinimumWidth(40);
    m_nameCounter->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_kindCombo = new QComboBox(nameGroup);
    m_kindCombo->addItem(tr("Normal"), static_cast<int>(Kind::Normal));
    m_kindCombo->addItem(tr("Masked  {"), static_cast<int>(Kind::Masked));
    m_kindCombo->addItem(tr("Water  !"), static_cast<int>(Kind::Water));
    m_kindCombo->addItem(tr("Animated  +0 ... +9"), static_cast<int>(Kind::Animated));
    m_kindCombo->addItem(tr("Toggled  +a ... +j"), static_cast<int>(Kind::Toggle));
    m_kindCombo->addItem(tr("Light  ~"), static_cast<int>(Kind::Light));
    m_kindCombo->setToolTip(tr("Masked: palette colour 255 (blue) is see-through.\nWater: drawn as a wavy liquid surface.\n"
                               "Animated: +0 to +9 of one name play in a loop.\nToggled: +a to +j are the second look a button switches to.\n"
                               "Light: emits light when the map is compiled with lights.rad."));
    m_frameLabel = new QLabel(tr("Frame:"), nameGroup);
    m_frameSpin = new QSpinBox(nameGroup);
    m_frameSpin->setRange(0, 9);
    m_nameProblem = new QLabel(nameGroup);
    m_nameProblem->setWordWrap(true);
    m_nameProblem->setStyleSheet("color: #ff6b6b;");
    m_nameProblem->setVisible(false);
    nameLayout->addWidget(m_nameEdit, 0, 0, 1, 2);
    nameLayout->addWidget(m_nameCounter, 0, 2);
    nameLayout->addWidget(new QLabel(tr("Type:"), nameGroup), 1, 0);
    nameLayout->addWidget(m_kindCombo, 1, 1, 1, 2);
    nameLayout->addWidget(m_frameLabel, 2, 0);
    nameLayout->addWidget(m_frameSpin, 2, 1);
    nameLayout->addWidget(m_nameProblem, 3, 0, 1, 3);
    nameLayout->setColumnStretch(1, 1);
    propertiesLayout->addWidget(nameGroup);

    auto* sizeGroup = new QGroupBox(tr("Size"), m_propertiesPanel);
    auto* sizeLayout = new QVBoxLayout(sizeGroup);
    m_sizeLabel = new QLabel(sizeGroup);
    m_sizeLabel->setWordWrap(true);
    m_sizeLabel->setTextFormat(Qt::RichText);
    sizeLayout->addWidget(m_sizeLabel);
    propertiesLayout->addWidget(sizeGroup);

    auto* paletteGroup = new QGroupBox(tr("Color Palette"), m_propertiesPanel);
    auto* paletteLayout = new QVBoxLayout(paletteGroup);
    m_palette = new PaletteWidget(paletteGroup);
    m_paletteInfo = new QLabel(tr("Click a color to see RGB values"), paletteGroup);
    m_paletteInfo->setWordWrap(true);
    paletteLayout->addWidget(m_palette, 0, Qt::AlignHCenter);
    paletteLayout->addWidget(m_paletteInfo);
    propertiesLayout->addWidget(paletteGroup);

    auto* mipGroup = new QGroupBox(tr("Mipmaps"), m_propertiesPanel);
    auto* mipLayout = new QHBoxLayout(mipGroup);
    for (QLabel*& label : m_mipLabels)
    {
        label = new QLabel(mipGroup);
        label->setMinimumSize(kMipPreviewSize, kMipPreviewSize + 18);
        label->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
        mipLayout->addWidget(label);
    }
    propertiesLayout->addWidget(mipGroup);
    rightLayout->addWidget(m_propertiesPanel);
    rightLayout->addStretch();

    splitter->addWidget(left);
    splitter->addWidget(centre);
    splitter->addWidget(rightScroll);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 3);
    splitter->setStretchFactor(2, 1);
    splitter->setSizes({ 460, 600, 320 });

    // bottom: how many textures, how big, and what GoldSrc would not like
    auto* statusRow = new QHBoxLayout();
    statusRow->setContentsMargins(6, 0, 6, 4);
    m_statusLabel = new QLabel(this);
    m_statusLabel->setForegroundRole(QPalette::PlaceholderText);
    m_warningsButton = new QToolButton(this);
    m_warningsButton->setPopupMode(QToolButton::InstantPopup);
    m_warningsButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_warningsButton->setAutoRaise(true);
    m_warningsButton->setStyleSheet("QToolButton { color: #ffd93d; }");
    m_warningsButton->setMenu(new QMenu(m_warningsButton));
    m_warningsButton->setVisible(false);
    statusRow->addWidget(m_statusLabel);
    statusRow->addWidget(m_warningsButton);
    statusRow->addStretch();
    mainLayout->addLayout(statusRow);

    // === Connections ===
    connect(m_newAction, &QAction::triggered, this, &WadMakerDialog::onNewWad);
    connect(m_openAction, &QAction::triggered, this, &WadMakerDialog::onOpenWad);
    connect(m_saveAction, &QAction::triggered, this, &WadMakerDialog::onSaveWad);
    connect(m_saveAsAction, &QAction::triggered, this, &WadMakerDialog::onSaveWadAs);

    connect(m_addAction, &QAction::triggered, this, &WadMakerDialog::onAddImages);
    connect(m_removeAction, &QAction::triggered, this, &WadMakerDialog::onRemoveSelected);
    connect(m_extractAction, &QAction::triggered, this, &WadMakerDialog::onExtractSelected);
    connect(m_renameAction, &QAction::triggered, this, &WadMakerDialog::onRenameSelected);
    connect(m_replaceAction, &QAction::triggered, this, &WadMakerDialog::onReplaceSelected);
    connect(m_kindFilter, &QButtonGroup::idClicked, this, &WadMakerDialog::applyFilter);
    connect(m_sortCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { refreshTextureList(currentIndex()); });
    connect(m_listViewButton, &QToolButton::toggled, this, [this]() { refreshTextureList(currentIndex()); });
    // thumbnails are made again once the slider is let go
    connect(m_sizeSlider, &QSlider::valueChanged, this, [this](int value) {
        m_thumbnailSize = value / 16 * 16;
        if (!m_sizeSlider->isSliderDown())
            refreshTextureList(currentIndex());
    });
    connect(m_sizeSlider, &QSlider::sliderReleased, this, [this]() { refreshTextureList(currentIndex()); });
    connect(m_textureGrid, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        if (!m_textureGrid->itemAt(pos))
            return;
        QMenu menu(this);
        menu.addAction(m_renameAction);
        menu.addAction(m_replaceAction);
        menu.addAction(m_extractAction);
        menu.addSeparator();
        menu.addAction(m_removeAction);
        menu.exec(m_textureGrid->viewport()->mapToGlobal(pos));
    });

    connect(m_textureGrid, &QListWidget::itemSelectionChanged, this, &WadMakerDialog::onSelectionChanged);
    connect(m_textureGrid, &QListWidget::itemDoubleClicked, this, &WadMakerDialog::onItemDoubleClicked);
    connect(m_filterEdit, &QLineEdit::textChanged, this, &WadMakerDialog::onFilterChanged);

    m_view->zoomChanged = [this]() { m_zoomLabel->setText(QStringLiteral("%1%").arg(qRound(m_view->zoom() * 100))); };
    connect(fitButton, &QPushButton::clicked, this, [this]() { m_view->fit(); });
    connect(actualButton, &QPushButton::clicked, this, [this]() { m_view->actualSize(); });
    connect(m_nameEdit, &QLineEdit::textEdited, this, [this]() {
        const int length = prefixOf(static_cast<Kind>(m_kindCombo->currentData().toInt()), m_frameSpin->value()).size()
                         + m_nameEdit->text().size();
        m_nameCounter->setText(QStringLiteral("%1/%2").arg(length).arg(MAX_NAME_LENGTH));
    });
    connect(m_nameEdit, &QLineEdit::editingFinished, this, &WadMakerDialog::applyName);
    connect(m_kindCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &WadMakerDialog::applyName);
    connect(m_frameSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &WadMakerDialog::applyName);
    connect(m_palette, &PaletteWidget::colorClicked, this, [this](int index, QRgb color) {
        m_paletteInfo->setText(tr("Index %1: R %2, G %3, B %4").arg(index).arg(qRed(color)).arg(qGreen(color)).arg(qBlue(color)));
    });
    connect(m_animateCheck, &QCheckBox::toggled, this, [this](bool on) {
        m_animationFrame = 0;
        if (on && m_animation.size() > 1)
            m_animationTimer->start();
        else
            m_animationTimer->stop();
        if (!on)
            updatePreview(currentIndex());
    });
    connect(m_animationTimer, &QTimer::timeout, this, [this]() {
        if (m_animation.empty())
            return;
        m_animationFrame = (m_animationFrame + 1) % m_animation.size();
        m_view->setImage(m_wadMaker->textureImage(static_cast<size_t>(m_animation[m_animationFrame])), true);
    });
}

WadMakerDialog::Kind WadMakerDialog::kindOf(const QString& name, int* frame)
{
    if (frame)
        *frame = 0;
    if (name.startsWith('{')) return Kind::Masked;
    if (name.startsWith('!')) return Kind::Water;
    if (name.startsWith('~')) return Kind::Light;
    if (name.size() >= 2 && name[0] == '+')
    {
        const QChar c = name[1].toLower();
        if (c >= '0' && c <= '9')
        {
            if (frame) *frame = c.unicode() - '0';
            return Kind::Animated;
        }
        if (c >= 'a' && c <= 'j')
        {
            if (frame) *frame = c.unicode() - 'a';
            return Kind::Toggle;
        }
    }
    return Kind::Normal;
}

QString WadMakerDialog::prefixOf(Kind kind, int frame)
{
    switch (kind)
    {
    case Kind::Masked: return QStringLiteral("{");
    case Kind::Water: return QStringLiteral("!");
    case Kind::Light: return QStringLiteral("~");
    case Kind::Animated: return QStringLiteral("+") + QChar('0' + frame);
    case Kind::Toggle: return QStringLiteral("+") + QChar('A' + frame);
    case Kind::Normal: break;
    }
    return QString();
}

qint64 WadMakerDialog::diskSizeOf(int width, int height)
{
    // miptex header, four mip levels, palette size, palette, padding
    const qint64 pixels = static_cast<qint64>(width) * height;
    return 40 + pixels + pixels / 4 + pixels / 16 + pixels / 64 + 2 + 768 + 2;
}

int WadMakerDialog::currentIndex() const
{
    QListWidgetItem* item = m_textureGrid->currentItem();
    if (!item || !item->isSelected())
    {
        const QList<QListWidgetItem*> selected = m_textureGrid->selectedItems();
        item = selected.isEmpty() ? nullptr : selected.first();
    }
    return item ? static_cast<int>(item->data(Qt::UserRole).toUInt()) : -1;
}

void WadMakerDialog::selectTexture(int index)
{
    for (int i = 0; i < m_textureGrid->count(); ++i)
    {
        QListWidgetItem* item = m_textureGrid->item(i);
        if (static_cast<int>(item->data(Qt::UserRole).toUInt()) == index)
        {
            item->setHidden(false);
            m_textureGrid->clearSelection();
            m_textureGrid->setCurrentItem(item);
            m_textureGrid->scrollToItem(item);
            return;
        }
    }
}

void WadMakerDialog::setModified(bool modified)
{
    m_modified = modified;
    m_saveAction->setEnabled(modified || !m_currentFilePath.isEmpty());
    updateTitle();
    emit modifiedChanged(modified);
}

void WadMakerDialog::updateTitle()
{
    const QString name = m_currentFilePath.isEmpty() ? tr("New") : QFileInfo(m_currentFilePath).fileName();
    setWindowTitle(tr("WAD Editor - %1").arg(name) + (m_modified ? QStringLiteral("*") : QString()));
}

void WadMakerDialog::onNewWad()
{
    if (!maybeSave()) return;

    m_wadMaker->clear();
    m_currentFilePath.clear();
    setModified(false);
    refreshTextureList();
}

void WadMakerDialog::onOpenWad()
{
    if (!maybeSave()) return;

    QString path = QFileDialog::getOpenFileName(
        this, tr("Open WAD File"), QString(),
        tr("WAD Files (*.wad);;All Files (*.*)")
    );

    if (path.isEmpty()) return;

    loadWad(path);
}

bool WadMakerDialog::loadWad(const QString& path)
{

    if (m_wadMaker->load(path.toStdString()))
    {
        m_currentFilePath = path;
        setModified(false);
        refreshTextureList();
        return true;
    }
    else
    {
        QMessageBox::critical(this, tr("Error"),
            tr("Failed to load WAD file:\n%1").arg(path));
        return false;
    }
}

void WadMakerDialog::onSaveWad()
{
    if (m_currentFilePath.isEmpty())
    {
        onSaveWadAs();
        return;
    }


    if (m_wadMaker->save(m_currentFilePath.toStdString()))
    {
        setModified(false);
        QMessageBox::information(this, tr("Success"),
            tr("WAD file saved successfully!"));
    }
    else
    {
        QMessageBox::critical(this, tr("Error"),
            tr("Failed to save WAD file."));
    }
}

void WadMakerDialog::onSaveWadAs()
{
    QString path = QFileDialog::getSaveFileName(
        this, tr("Save WAD File"), QString(),
        tr("WAD Files (*.wad);;All Files (*.*)")
    );

    if (path.isEmpty()) return;

    if (!path.toLower().endsWith(".wad"))
        path += ".wad";


    if (m_wadMaker->save(path.toStdString()))
    {
        m_currentFilePath = path;
        setModified(false);
        QMessageBox::information(this, tr("Success"),
            tr("WAD file saved successfully!\n\nTextures: %1").arg(m_wadMaker->textureCount()));
    }
    else
    {
        QMessageBox::critical(this, tr("Error"),
            tr("Failed to save WAD file."));
    }
}

void WadMakerDialog::onAddImages()
{
    const QStringList files = QFileDialog::getOpenFileNames(this, tr("Add Images"), QString(),
        tr("All Images (*.bmp *.png *.jpg *.jpeg *.tga *.dds);;"
           "BMP (*.bmp);;PNG (*.png);;JPEG (*.jpg *.jpeg);;"
           "TGA (*.tga);;DDS (*.dds);;All Files (*.*)"));
    if (!files.isEmpty())
        addImages(files);
}

int WadMakerDialog::addImages(const QStringList& paths)
{
    const WadFit fit = static_cast<WadFit>(m_fitCombo->currentData().toInt());
    int added = 0;
    QStringList failed;
    QStringList renamed;
    for (const QString& path : paths)
    {
        if (m_wadMaker->textureCount() >= MAX_TEXTURES)
        {
            failed << tr("%1 (the WAD is full)").arg(QFileInfo(path).fileName());
            continue;
        }
        const QString wanted = QFileInfo(path).completeBaseName().toUpper();
        if (!m_wadMaker->addTextureFromImage(wanted.toStdString(), path.toStdString(), fit))
        {
            failed << QFileInfo(path).fileName();
            continue;
        }
        added++;
        // a { for see-through pixels is expected; a shortened name is worth telling
        QString name = QString::fromStdString(m_wadMaker->textures().back().name);
        if (name.startsWith('{') && !wanted.startsWith('{'))
            name.remove(0, 1);
        if (!wanted.startsWith(name))
            renamed << QStringLiteral("%1 → %2").arg(QFileInfo(path).fileName(), QString::fromStdString(m_wadMaker->textures().back().name));
    }
    reportAdded(added, failed, renamed);
    return added;
}

int WadMakerDialog::addImages(const QList<QPair<QString, QImage>>& images)
{
    const WadFit fit = static_cast<WadFit>(m_fitCombo->currentData().toInt());
    int added = 0;
    QStringList failed;
    QStringList renamed;
    for (const auto& [wanted, image] : images)
    {
        if (m_wadMaker->textureCount() >= MAX_TEXTURES || !m_wadMaker->addTextureFromImage(wanted.toUpper().toStdString(), image, fit))
        {
            failed << wanted;
            continue;
        }
        added++;
        QString name = QString::fromStdString(m_wadMaker->textures().back().name);
        if (name.startsWith('{') && !wanted.startsWith('{'))
            name.remove(0, 1);
        if (!wanted.toUpper().startsWith(name))
            renamed << QStringLiteral("%1 → %2").arg(wanted, QString::fromStdString(m_wadMaker->textures().back().name));
    }
    reportAdded(added, failed, renamed);
    return added;
}

void WadMakerDialog::reportAdded(int added, const QStringList& failed, const QStringList& renamed)
{
    if (added > 0)
    {
        setModified(true);
        refreshTextureList(static_cast<int>(m_wadMaker->textureCount()) - 1);
    }
    // only what needs a look is told; the new textures are on screen
    if (failed.isEmpty() && renamed.isEmpty())
        return;
    QString text = tr("Added: %1").arg(added);
    if (!renamed.isEmpty())
        text += QStringLiteral("\n\n") + tr("Names shortened to the 15 characters GoldSrc allows:") +
                QStringLiteral("\n") + renamed.join(QStringLiteral("\n"));
    if (!failed.isEmpty())
        text += QStringLiteral("\n\n") + tr("Not added:") + QStringLiteral("\n") + failed.join(QStringLiteral("\n"));
    QMessageBox::information(this, tr("Images Added"), text);
}

void WadMakerDialog::onReplaceSelected()
{
    const int index = currentIndex();
    if (index < 0)
        return;
    const QString file = QFileDialog::getOpenFileName(this, tr("Replace Image"), QString(),
        tr("All Images (*.bmp *.png *.jpg *.jpeg *.tga *.dds);;All Files (*.*)"));
    if (file.isEmpty())
        return;
    const WadFit fit = static_cast<WadFit>(m_fitCombo->currentData().toInt());
    if (!m_wadMaker->replaceTextureImage(static_cast<size_t>(index), file.toStdString(), fit))
    {
        QMessageBox::warning(this, tr("Error"), tr("The image could not be read."));
        return;
    }
    setModified(true);
    refreshTextureList(index);
}

void WadMakerDialog::dragEnterEvent(QDragEnterEvent* event)
{
    for (const QString& file : droppedFiles(event->mimeData()))
    {
        if (isImageFile(file) || QFileInfo(file).suffix().compare(QStringLiteral("wad"), Qt::CaseInsensitive) == 0)
        {
            event->acceptProposedAction();
            return;
        }
    }
}

void WadMakerDialog::dropEvent(QDropEvent* event)
{
    QStringList images;
    QString wad;
    for (const QString& file : droppedFiles(event->mimeData()))
    {
        if (isImageFile(file))
            images << file;
        else if (QFileInfo(file).suffix().compare(QStringLiteral("wad"), Qt::CaseInsensitive) == 0)
            wad = file;
    }
    event->acceptProposedAction();
    if (!wad.isEmpty() && images.isEmpty())
    {
        if (maybeSave())
            loadWad(wad);
        return;
    }
    if (!images.isEmpty())
        addImages(images);
}

void WadMakerDialog::onRemoveSelected()
{
    QList<QListWidgetItem*> selected = m_textureGrid->selectedItems();
    if (selected.isEmpty()) return;

    int ret = QMessageBox::question(this, tr("Remove Textures"),
        tr("Remove %1 selected texture(s)?").arg(selected.count()));

    if (ret != QMessageBox::Yes) return;

    // Get indices in reverse order to remove safely
    std::vector<size_t> indices;
    for (QListWidgetItem* item : selected)
    {
        indices.push_back(item->data(Qt::UserRole).toUInt());
    }
    std::sort(indices.rbegin(), indices.rend());

    for (size_t idx : indices)
    {
        m_wadMaker->removeTexture(idx);
    }

    setModified(true);
    refreshTextureList();
}

void WadMakerDialog::onExtractSelected()
{
    QList<QListWidgetItem*> selected = m_textureGrid->selectedItems();
    if (selected.isEmpty()) return;

    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Output Directory"));
    if (dir.isEmpty()) return;

    int extracted = 0;
    for (QListWidgetItem* item : selected)
    {
        size_t idx = item->data(Qt::UserRole).toUInt();
        const auto& textures = m_wadMaker->textures();
        if (idx < textures.size())
        {
            QString outPath = dir + "/" + QString::fromStdString(textures[idx].name) + ".bmp";
            if (m_wadMaker->extractTextureToBmp(idx, outPath.toStdString()))
            {
                extracted++;
            }
        }
    }

    QMessageBox::information(this, tr("Extraction Complete"),
        tr("Extracted %1 texture(s) to:\n%2").arg(extracted).arg(dir));
}

void WadMakerDialog::onRenameSelected()
{
    // the name lives in the panel on the right
    if (currentIndex() < 0)
        return;
    m_nameEdit->setFocus();
    m_nameEdit->selectAll();
}

void WadMakerDialog::applyName()
{
    const int index = currentIndex();
    const auto& textures = m_wadMaker->textures();
    if (m_updatingProperties || index < 0 || static_cast<size_t>(index) >= textures.size())
        return;

    const Kind kind = static_cast<Kind>(m_kindCombo->currentData().toInt());
    const bool framed = kind == Kind::Animated || kind == Kind::Toggle;
    m_frameLabel->setVisible(framed);
    m_frameSpin->setVisible(framed);

    // what renameTexture keeps of a name: letters, digits, _ - and the prefixes
    QString base = m_nameEdit->text().trimmed().toUpper();
    base.remove(QRegularExpression(QStringLiteral("[^A-Z0-9_\\-]")));
    const QString name = prefixOf(kind, framed ? m_frameSpin->value() : 0) + base;
    m_nameCounter->setText(QStringLiteral("%1/%2").arg(name.size()).arg(MAX_NAME_LENGTH));

    QString problem;
    if (base.isEmpty())
        problem = tr("The name is empty.");
    else if (name.size() > MAX_NAME_LENGTH)
        problem = tr("%1 characters: GoldSrc allows %2 with the prefix.").arg(name.size()).arg(MAX_NAME_LENGTH);
    for (size_t i = 0; problem.isEmpty() && i < textures.size(); ++i)
    {
        if (static_cast<int>(i) != index && QString::fromStdString(textures[i].name).compare(name, Qt::CaseInsensitive) == 0)
            problem = tr("%1 is already in this WAD.").arg(name);
    }
    m_nameProblem->setText(problem);
    m_nameProblem->setVisible(!problem.isEmpty());
    if (!problem.isEmpty() || name == QString::fromStdString(textures[index].name))
        return;

    if (m_wadMaker->renameTexture(static_cast<size_t>(index), name.toStdString()))
    {
        setModified(true);
        refreshTextureList(index);
    }
}

std::vector<int> WadMakerDialog::animationFrames(int index) const
{
    const auto& textures = m_wadMaker->textures();
    const QString name = QString::fromStdString(textures[index].name);
    int frame = 0;
    const Kind kind = kindOf(name, &frame);
    if (kind != Kind::Animated && kind != Kind::Toggle)
        return {};
    const QString base = name.mid(2);
    std::map<int, int> frames;
    for (size_t i = 0; i < textures.size(); ++i)
    {
        const QString other = QString::fromStdString(textures[i].name);
        int otherFrame = 0;
        if (kindOf(other, &otherFrame) == kind && other.mid(2).compare(base, Qt::CaseInsensitive) == 0)
            frames.emplace(otherFrame, static_cast<int>(i));
    }
    std::vector<int> ordered;
    for (const auto& [frameNumber, textureIndex] : frames)
        ordered.push_back(textureIndex);
    return ordered;
}

std::vector<WadMakerDialog::Warning> WadMakerDialog::findWarnings() const
{
    std::vector<Warning> warnings;
    const auto& textures = m_wadMaker->textures();
    std::map<QString, int> firstOfName;
    for (size_t i = 0; i < textures.size(); ++i)
    {
        const WadTexture& texture = textures[i];
        const QString name = QString::fromStdString(texture.name);
        const int index = static_cast<int>(i);
        if (name.size() > MAX_NAME_LENGTH)
            warnings.push_back({ index, tr("%1: the name is longer than %2 characters").arg(name).arg(MAX_NAME_LENGTH) });
        if (!firstOfName.emplace(name.toUpper(), index).second)
            warnings.push_back({ index, tr("%1: the name is used twice").arg(name) });
        if (texture.width % 16 != 0 || texture.height % 16 != 0)
            warnings.push_back({ index, tr("%1: %2 x %3 is not a multiple of 16").arg(name).arg(texture.width).arg(texture.height) });
        if (texture.width > 512 || texture.height > 512)
            warnings.push_back({ index, tr("%1: larger than 512 x 512").arg(name) });

        // an animation needs its frames from +0 up without a gap
        int frame = 0;
        const Kind kind = kindOf(name, &frame);
        if ((kind == Kind::Animated || kind == Kind::Toggle) && frame == 0)
        {
            const size_t count = animationFrames(index).size();
            int highest = 0;
            for (int other : animationFrames(index))
            {
                int otherFrame = 0;
                kindOf(QString::fromStdString(textures[other].name), &otherFrame);
                highest = (std::max)(highest, otherFrame);
            }
            if (count == 1 && kind == Kind::Animated)
                warnings.push_back({ index, tr("%1: an animation with a single frame").arg(name) });
            else if (static_cast<int>(count) != highest + 1)
                warnings.push_back({ index, tr("%1: frames of the animation are missing").arg(name) });
        }
    }
    return warnings;
}

void WadMakerDialog::onSelectionChanged()
{
    QList<QListWidgetItem*> selected = m_textureGrid->selectedItems();
    bool hasSelection = !selected.isEmpty();

    m_removeAction->setEnabled(hasSelection);
    m_extractAction->setEnabled(hasSelection);
    m_renameAction->setEnabled(selected.count() == 1);
    m_replaceAction->setEnabled(selected.count() == 1);
    updatePreview(currentIndex());
}

void WadMakerDialog::onFilterChanged(const QString&)
{
    applyFilter();
}

void WadMakerDialog::applyFilter()
{
    const QString filter = m_filterEdit->text().toLower();
    const int kind = m_kindFilter->checkedId();
    const auto& textures = m_wadMaker->textures();
    for (int i = 0; i < m_textureGrid->count(); ++i)
    {
        QListWidgetItem* item = m_textureGrid->item(i);
        const QString name = QString::fromStdString(textures[item->data(Qt::UserRole).toUInt()].name);
        Kind itemKind = kindOf(name);
        if (itemKind == Kind::Toggle)
            itemKind = Kind::Animated;
        const bool match = (filter.isEmpty() || name.toLower().contains(filter)) &&
                           (kind == kAllKinds || static_cast<int>(itemKind) == kind);
        item->setHidden(!match);
    }
}

void WadMakerDialog::onItemDoubleClicked(QListWidgetItem* item)
{
    // Double-click to rename
    if (item)
    {
        m_textureGrid->setCurrentItem(item);
        onRenameSelected();
    }
}

void WadMakerDialog::refreshTextureList(int keepSelected)
{
    // the grid of thumbnails, or a list with the sizes
    const bool list = m_listViewButton->isChecked();
    m_textureGrid->setViewMode(list ? QListView::ListMode : QListView::IconMode);
    m_textureGrid->setWrapping(!list);
    const int iconSize = list ? kListIconSize : m_thumbnailSize;
    m_textureGrid->setIconSize(QSize(iconSize, iconSize));
    m_textureGrid->setGridSize(list ? QSize() : QSize(iconSize + 28, iconSize + 30));
    m_sizeSlider->setEnabled(!list);

    m_textureGrid->blockSignals(true);
    m_textureGrid->clear();
    populateThumbnails();
    m_textureGrid->blockSignals(false);
    applyFilter();
    updateSummary();
    if (keepSelected >= 0 && static_cast<size_t>(keepSelected) < m_wadMaker->textureCount())
        selectTexture(keepSelected);
    else
        onSelectionChanged();
}

std::vector<int> WadMakerDialog::sortedIndices() const
{
    const auto& textures = m_wadMaker->textures();
    std::vector<int> order(textures.size());
    for (size_t i = 0; i < order.size(); ++i)
        order[i] = static_cast<int>(i);
    const int sort = m_sortCombo->currentIndex();
    auto name = [&textures](int i) { return QString::fromStdString(textures[i].name); };
    if (sort == 0)
        std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return name(a).compare(name(b), Qt::CaseInsensitive) < 0; });
    else if (sort == 1)
        std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
            return textures[a].width * textures[a].height > textures[b].width * textures[b].height;
        });
    else if (sort == 2)
        std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
            const int kindA = static_cast<int>(kindOf(name(a)));
            const int kindB = static_cast<int>(kindOf(name(b)));
            return kindA != kindB ? kindA < kindB : name(a).compare(name(b), Qt::CaseInsensitive) < 0;
        });
    return order;
}

void WadMakerDialog::populateThumbnails()
{
    const auto& textures = m_wadMaker->textures();
    const bool list = m_listViewButton->isChecked();
    const int iconSize = m_textureGrid->iconSize().width();
    for (int i : sortedIndices())
    {
        const WadTexture& texture = textures[i];
        const QString name = QString::fromStdString(texture.name);
        auto* item = new QListWidgetItem();
        item->setText(list ? QStringLiteral("%1    %2 x %3").arg(name).arg(texture.width).arg(texture.height) : name);
        item->setData(Qt::UserRole, static_cast<uint>(i));
        item->setToolTip(QString("%1\n%2×%3").arg(name).arg(texture.width).arg(texture.height));

        // centred in a square, so every item has the same size and room for its name; small ones keep sharp pixels
        const QImage image = m_wadMaker->textureImage(static_cast<size_t>(i));
        if (!image.isNull())
        {
            const bool sharp = image.width() < iconSize && image.height() < iconSize;
            const QImage thumbnail = image.scaled(iconSize, iconSize, Qt::KeepAspectRatio,
                                                  sharp ? Qt::FastTransformation : Qt::SmoothTransformation);
            QPixmap square(iconSize, iconSize);
            square.fill(Qt::transparent);
            QPainter painter(&square);
            painter.drawImage((iconSize - thumbnail.width()) / 2, (iconSize - thumbnail.height()) / 2, thumbnail);
            painter.end();
            item->setIcon(QIcon(square));
        }
        m_textureGrid->addItem(item);
    }
}

void WadMakerDialog::updateSummary()
{
    const auto& textures = m_wadMaker->textures();
    qint64 bytes = 12;  // WAD header
    for (const WadTexture& texture : textures)
        bytes += diskSizeOf(static_cast<int>(texture.width), static_cast<int>(texture.height)) + 32;  // + directory entry
    m_statusLabel->setText(textures.empty() ? tr("No textures yet. Add images from the ribbon.")
                                            : tr("%n texture(s)", "", static_cast<int>(textures.size())) +
                                                  QStringLiteral("  ·  ") + UiUtils::formatSize(bytes));

    // each warning selects its texture
    const std::vector<Warning> warnings = findWarnings();
    QMenu* menu = m_warningsButton->menu();
    menu->clear();
    for (const Warning& warning : warnings)
    {
        const int index = warning.index;
        menu->addAction(warning.text, this, [this, index]() { selectTexture(index); });
    }
    m_warningsButton->setText(QStringLiteral("⚠ ") + tr("%n warning(s)", "", static_cast<int>(warnings.size())));
    m_warningsButton->setVisible(!warnings.empty());
}

void WadMakerDialog::updatePreview(int index)
{
    m_animationTimer->stop();
    m_animation.clear();
    const auto& textures = m_wadMaker->textures();
    const bool valid = index >= 0 && static_cast<size_t>(index) < textures.size();
    m_propertiesPanel->setVisible(valid);
    m_noSelectionLabel->setVisible(!valid);
    if (!valid)
    {
        const int selected = m_textureGrid->selectedItems().size();
        m_noSelectionLabel->setText(selected > 1 ? tr("%1 textures selected").arg(selected)
                                                 : tr("Select a texture to see its properties."));
        m_view->setImage(QImage(), false);
        m_animateCheck->setVisible(false);
        return;
    }

    const WadTexture& texture = textures[index];
    const QString name = QString::fromStdString(texture.name);
    m_view->setImage(m_wadMaker->textureImage(static_cast<size_t>(index)), false);

    // the name split into its type and the rest, without telling applyName
    m_updatingProperties = true;
    int frame = 0;
    const Kind kind = kindOf(name, &frame);
    const QString prefix = prefixOf(kind, frame);
    m_titleLabel->setText(name);
    m_nameEdit->setText(name.mid(prefix.size()));
    m_nameEdit->setMaxLength(MAX_NAME_LENGTH);
    m_kindCombo->setCurrentIndex(m_kindCombo->findData(static_cast<int>(kind)));
    m_frameSpin->setValue(frame);
    const bool framed = kind == Kind::Animated || kind == Kind::Toggle;
    m_frameLabel->setVisible(framed);
    m_frameSpin->setVisible(framed);
    m_nameCounter->setText(QStringLiteral("%1/%2").arg(name.size()).arg(MAX_NAME_LENGTH));
    m_nameProblem->setVisible(false);
    m_updatingProperties = false;

    QStringList size;
    size << QStringLiteral("<b>%1 x %2</b>").arg(texture.width).arg(texture.height);
    // only what GoldSrc would not take is told
    const QString bad = QStringLiteral("<span style='color:#ff6b6b;'>⚠</span> ");
    if (texture.width % 16 != 0 || texture.height % 16 != 0)
        size << bad + tr("Sides must be multiples of 16");
    if (texture.width > 512 || texture.height > 512)
        size << bad + tr("Larger than 512 x 512");
    size << tr("In the WAD: %1").arg(UiUtils::formatSize(diskSizeOf(static_cast<int>(texture.width), static_cast<int>(texture.height))));
    m_sizeLabel->setText(size.join(QStringLiteral("<br>")));

    QVector<QRgb> colors;
    for (int i = 0; i < 256; ++i)
        colors.append(qRgb(texture.palette[i * 3], texture.palette[i * 3 + 1], texture.palette[i * 3 + 2]));
    m_palette->setPalette(colors);
    m_paletteInfo->setText(kind == Kind::Masked ? tr("Color 255 is see-through in this texture.") : tr("Click a color to see RGB values"));

    for (int mip = 0; mip < 4; ++mip)
    {
        const QImage image = m_wadMaker->textureImage(static_cast<size_t>(index), mip);
        QPixmap pixmap;
        if (!image.isNull())
            pixmap = QPixmap::fromImage(image.scaled(kMipPreviewSize, kMipPreviewSize, Qt::KeepAspectRatio, Qt::FastTransformation));
        m_mipLabels[mip]->setPixmap(pixmap);
        m_mipLabels[mip]->setToolTip(image.isNull() ? QString() : QStringLiteral("%1 x %2").arg(image.width()).arg(image.height()));
    }

    m_animation = animationFrames(index);
    m_animateCheck->setVisible(m_animation.size() > 1);
    if (m_animateCheck->isChecked() && m_animation.size() > 1)
    {
        m_animationFrame = static_cast<size_t>(std::find(m_animation.begin(), m_animation.end(), index) - m_animation.begin());
        m_animationTimer->start();
    }
}

bool WadMakerDialog::maybeSave()
{
    if (!m_modified || m_wadMaker->textureCount() == 0)
        return true;

    int ret = QMessageBox::question(this, tr("Unsaved Changes"),
        tr("Save changes before proceeding?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

    if (ret == QMessageBox::Save)
    {
        onSaveWad();
        return !m_modified;
    }
    else if (ret == QMessageBox::Discard)
    {
        return true;
    }

    return false;
}
