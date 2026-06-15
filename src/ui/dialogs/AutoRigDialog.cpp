#include "AutoRigDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QCheckBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>
#include <QPixmap>
#include <QFrame>
#include <QEvent>
#include <QMouseEvent>
#include <QScreen>
#include <QScrollArea>
#include <limits>
#include <algorithm>
#include <functional>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "core/smd/SmdParser.h"
#include "core/autorig/AutoRig.h"
#include "core/smd/SmdWriter.h" 
#include "LanguageManager.h"
#include "core/VortigauntLog.h"



AutoRigDialog::AutoRigDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("GoldSrc Auto-Rig (Beta)"));
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint | Qt::WindowMinimizeButtonHint);
    setMinimumSize(400, 300);
    
    QScreen* screen = QGuiApplication::primaryScreen();
    QSize screenSize = screen ? screen->availableGeometry().size() : QSize(1920, 1080);
    resize(screenSize.width() * 0.7, screenSize.height() * 0.7);

    // left (form) + right (tip image)
    QHBoxLayout* topLayout = new QHBoxLayout(this);

    // Left side
    QVBoxLayout* mainLayout = new QVBoxLayout();

    

    // Mesh input group
    QGroupBox* meshGroup = new QGroupBox(tr("Input Mesh (with skeleton)"));
    QHBoxLayout* meshLayout = new QHBoxLayout(meshGroup);
    m_meshEdit = new QLineEdit();
    m_meshEdit->setPlaceholderText(tr("Select SMD"));
    m_browseMeshButton = new QPushButton(tr("Browse..."));
    meshLayout->addWidget(m_meshEdit);
    meshLayout->addWidget(m_browseMeshButton);
    mainLayout->addWidget(meshGroup);

    // Output group
    QGroupBox* outputGroup = new QGroupBox(tr("Output SMD"));
    QHBoxLayout* outputLayout = new QHBoxLayout(outputGroup);
    m_outputEdit = new QLineEdit();
    m_outputEdit->setPlaceholderText(tr("Output rigged SMD path"));
    m_browseOutputButton = new QPushButton(tr("Browse..."));
    outputLayout->addWidget(m_outputEdit);
    outputLayout->addWidget(m_browseOutputButton);
    mainLayout->addWidget(outputGroup);

    // Options group
    QGroupBox* optionsGroup = new QGroupBox(tr("Options"));
    QVBoxLayout* optionsLayout = new QVBoxLayout(optionsGroup);
    
    QHBoxLayout* scaleLayout = new QHBoxLayout();
    QLabel* scaleLabel = new QLabel(tr("Manual Scale:"));
    m_scaleSpinBox = new QDoubleSpinBox();
    m_scaleSpinBox->setRange(0.001, 1000.0);
    m_scaleSpinBox->setValue(1.0);
    m_scaleSpinBox->setDecimals(3);
    m_scaleSpinBox->setSingleStep(0.1);
    scaleLayout->addWidget(scaleLabel);
    scaleLayout->addWidget(m_scaleSpinBox);
    scaleLayout->addStretch();
    optionsLayout->addLayout(scaleLayout);

    
    m_flipYZCheck = new QCheckBox(tr("Flip Y/Z axes"));
    m_flipYZCheck->setToolTip(tr("Enable if mesh is oriented differently (e.g., from Blender)"));
    optionsLayout->addWidget(m_flipYZCheck);
    
    m_depthPenaltyCheck = new QCheckBox(tr("Use Hierarchy Depth Penalty (Enable for Player Models)"));
    m_depthPenaltyCheck->setToolTip(tr("Prevents helper bones from stealing vertices. Uncheck this for Hand/Viewmodel meshes."));
    m_depthPenaltyCheck->setChecked(true);
    optionsLayout->addWidget(m_depthPenaltyCheck);
    
    mainLayout->addWidget(optionsGroup);

    // Rig button and progress
    QHBoxLayout* actionLayout = new QHBoxLayout();
    m_rigButton = new QPushButton(tr("Rig Mesh"));
    m_rigButton->setMinimumHeight(40);
    m_rigButton->setStyleSheet("font-weight: bold; font-size: 14px;");
    m_progressBar = new QProgressBar();
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setVisible(false);
    actionLayout->addWidget(m_rigButton);
    actionLayout->addWidget(m_progressBar);
    mainLayout->addLayout(actionLayout);

    // Log 
    QGroupBox* logGroup = new QGroupBox(tr("Log"));
    QVBoxLayout* logLayout = new QVBoxLayout(logGroup);
    m_logEdit = new QPlainTextEdit();
    m_logEdit->setReadOnly(true);
    m_logEdit->setMaximumBlockCount(1000);
    logLayout->addWidget(m_logEdit);
    mainLayout->addWidget(logGroup);

    // Add left side to top layout
    topLayout->addLayout(mainLayout, 1);

    // Middle column: Bones exclusion tree
    m_bonesGroupBox = new QGroupBox(tr("Bones Configuration"));
    QVBoxLayout* bonesLayout = new QVBoxLayout(m_bonesGroupBox);
    bonesLayout->setContentsMargins(10, 12, 10, 10);
    bonesLayout->setSpacing(8);

    // Search bar
    m_boneSearchEdit = new QLineEdit();
    m_boneSearchEdit->setPlaceholderText(tr("Search bone..."));
    m_boneSearchEdit->setClearButtonEnabled(true);
    bonesLayout->addWidget(m_boneSearchEdit);

    // Tree widget
    m_boneTreeWidget = new QTreeWidget();
    m_boneTreeWidget->setHeaderLabel(tr("Skeleton Hierarchy"));
    m_boneTreeWidget->setHeaderHidden(true);
    m_boneTreeWidget->setSelectionMode(QAbstractItemView::NoSelection);
    m_boneTreeWidget->setStyleSheet(
        "QTreeWidget { background-color: #1a1a1a; border: 1px solid #555; border-radius: 6px; color: #ccc; padding: 4px; }"
        "QTreeWidget::item { padding: 4px; }"
        "QTreeWidget::item:hover { background-color: #2a2a2a; border-radius: 4px; }"
    );
    bonesLayout->addWidget(m_boneTreeWidget);

    // Count label & controls
    QHBoxLayout* boneCtrlLayout = new QHBoxLayout();
    m_selectAllBonesButton = new QPushButton(tr("Select All"));
    m_deselectAllBonesButton = new QPushButton(tr("Deselect All"));
    m_selectAllBonesButton->setStyleSheet("font-size: 11px; padding: 4px 8px;");
    m_deselectAllBonesButton->setStyleSheet("font-size: 11px; padding: 4px 8px;");
    boneCtrlLayout->addWidget(m_selectAllBonesButton);
    boneCtrlLayout->addWidget(m_deselectAllBonesButton);
    bonesLayout->addLayout(boneCtrlLayout);

    m_boneCountLabel = new QLabel(tr("No skeleton loaded"));
    m_boneCountLabel->setStyleSheet("color: #888; font-size: 11px; font-style: italic;");
    m_boneCountLabel->setAlignment(Qt::AlignCenter);
    bonesLayout->addWidget(m_boneCountLabel);

    topLayout->addWidget(m_bonesGroupBox, 1);

    // Right side: Tip image + footnote in a GroupBox
    QGroupBox* tipGroup = new QGroupBox(tr("Tip"));
    tipGroup->setFixedWidth(420);
    QVBoxLayout* rightLayout = new QVBoxLayout(tipGroup);
    rightLayout->setContentsMargins(12, 16, 12, 12);
    rightLayout->setSpacing(10);

    // Image with a styled frame
    QFrame* imageFrame = new QFrame();
    imageFrame->setStyleSheet(
        "QFrame { border: 1px solid #555; border-radius: 6px; background: #1a1a1a; padding: 4px; }"
    );
    QVBoxLayout* imageFrameLayout = new QVBoxLayout(imageFrame);
    imageFrameLayout->setContentsMargins(4, 4, 4, 4);

    m_tipImageLabel = new QLabel();
    m_tipPixmap = QPixmap(":/autorigtip.png");
    if (!m_tipPixmap.isNull()) {
        QPixmap scaled = m_tipPixmap.scaled(380, 600, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_tipImageLabel->setPixmap(scaled);
    }
    m_tipImageLabel->setAlignment(Qt::AlignCenter);
    m_tipImageLabel->setCursor(Qt::PointingHandCursor);
    m_tipImageLabel->setToolTip(tr("Click to view full size"));
    m_tipImageLabel->installEventFilter(this);
    imageFrameLayout->addWidget(m_tipImageLabel);
    rightLayout->addWidget(imageFrame);

    // Footnote with warning icon
    QLabel* footnoteLabel = new QLabel(
        tr("Make sure your mesh is properly fitted to the bone like this image. "
           "Since Auto Rig is in beta, errors may occur.")
    );
    footnoteLabel->setWordWrap(true);
    footnoteLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    footnoteLabel->setStyleSheet(
        "color: #FFA726; font-size: 11px; padding: 6px 4px; line-height: 1.4;"
    );
    rightLayout->addWidget(footnoteLabel);

    rightLayout->addStretch();
    topLayout->addWidget(tipGroup, 0);

    connect(m_browseMeshButton, &QPushButton::clicked, this, &AutoRigDialog::onBrowseMesh);
    connect(m_browseOutputButton, &QPushButton::clicked, this, &AutoRigDialog::onBrowseOutput);
    connect(m_rigButton, &QPushButton::clicked, this, &AutoRigDialog::onRig);
    
    connect(m_meshEdit, &QLineEdit::textChanged, this, &AutoRigDialog::onMeshPathChanged);
    connect(m_boneSearchEdit, &QLineEdit::textChanged, this, &AutoRigDialog::onSearchBones);
    connect(m_selectAllBonesButton, &QPushButton::clicked, this, &AutoRigDialog::onSelectAllBones);
    connect(m_deselectAllBonesButton, &QPushButton::clicked, this, &AutoRigDialog::onDeselectAllBones);
    connect(m_boneTreeWidget, &QTreeWidget::itemChanged, this, &AutoRigDialog::updateBoneCountLabel);
    VortigauntLog::addLogWidget(m_logEdit);

    VortigauntLog::Vortigaunt_Printf("^2Auto-Rig ready.");

}

void AutoRigDialog::onBrowseMesh() {
    QString path = QFileDialog::getOpenFileName(
        this, "Select Mesh File", QString(),
        "SMD Files (*.smd);;All Files (*.*)"
    );
    if (!path.isEmpty()) {
        m_meshEdit->setText(path);
        VortigauntLog::Vortigaunt_Printf(QStringLiteral("^2Mesh: ^1%1").arg(path));
        
        QString outPath = path;
        int dotPos = outPath.lastIndexOf('.');
        if (dotPos > 0) {
            outPath = outPath.left(dotPos) + "_rigged.smd";
        } else {
            outPath += "_rigged.smd";
        }
        m_outputEdit->setText(outPath);
    }
}

void AutoRigDialog::onBrowseOutput() {
    QString path = QFileDialog::getSaveFileName(
        this, "Save Rigged SMD", m_outputEdit->text(),
        "SMD Files (*.smd);;All Files (*.*)"
    );
    if (!path.isEmpty()) {
        if (!path.endsWith(".smd", Qt::CaseInsensitive)) {
            path += ".smd";
        }
        m_outputEdit->setText(path);
    }
}

void AutoRigDialog::onRig() {
    QString meshPath = m_meshEdit->text();
    QString outPath = m_outputEdit->text();

    if (meshPath.isEmpty()) {
        QMessageBox::warning(this, "Error", "Please select a mesh file.");
        return;
    }
    if (outPath.isEmpty()) {
        QMessageBox::warning(this, "Error", "Please specify an output path.");
        return;
    }

    m_rigButton->setEnabled(false);
    m_progressBar->setVisible(true);
    setProgress(0);

    VortigauntLog::Vortigaunt_Printf("^3Initialize Auto-Rig");

    setProgress(20);

    VortigauntLog::Vortigaunt_Printf("^2Loading input SMD mesh and skeleton...");
    QApplication::processEvents();
    
    SmdParser meshParser;
    if (!meshParser.Parse(meshPath.toStdString())) {
        VortigauntLog::Vortigaunt_Printf(QStringLiteral("^7ERROR: ^9%1").arg(QString::fromStdString(meshParser.GetError())));
        m_rigButton->setEnabled(true);
        m_progressBar->setVisible(false);
        return;
    }
    
    std::vector<SmdTriangle> triangles = meshParser.GetTriangles();
    const std::vector<SmdBone>& inputBones = meshParser.GetBones();
    
    VortigauntLog::Vortigaunt_Printf(QStringLiteral("^2Loaded ^5%1 ^2triangles.").arg(triangles.size()));
    VortigauntLog::Vortigaunt_Printf(QStringLiteral("^2Loaded ^5%1 ^2bones from input mesh.").arg(inputBones.size()));
    setProgress(40);
    
    if (inputBones.empty()) {
        VortigauntLog::Vortigaunt_Printf("^7ERROR: ^9No skeleton found in the input SMD. AutoRig requires a reference skeleton within the input mesh file.");
        m_rigButton->setEnabled(true);
        m_progressBar->setVisible(false);
        return;
    }

    std::unordered_set<int> ignoredBones;
    for (const auto& [boneIdx, item] : m_boneItemMap) {
        if (item->checkState(0) == Qt::Unchecked) {
            ignoredBones.insert(boneIdx);
        }
    }

    AutoRig gsrcAutorig;
    gsrcAutorig.SetSkeleton(inputBones);
    gsrcAutorig.SetIgnoredBones(ignoredBones);

    setProgress(50);
    
    float scale = m_scaleSpinBox->value();
    bool flipYZ = m_flipYZCheck->isChecked();
    bool useDepthPenalty = m_depthPenaltyCheck->isChecked();

    setProgress(60);
    
    VortigauntLog::Vortigaunt_Printf("^2Assigning bones to vertices...");
    QApplication::processEvents();
    
    std::vector<float> vertexPositions;
    std::vector<float> vertexNormals;
    for (const auto& tri : triangles) {
        for (int v = 0; v < 3; v++) {
            float x = tri.vertices[v].x * scale;
            float y = tri.vertices[v].y * scale;
            float z = tri.vertices[v].z * scale;
            float nx = tri.vertices[v].nx;
            float ny = tri.vertices[v].ny;
            float nz = tri.vertices[v].nz;
            
            if (flipYZ) {
                float temp = y;
                y = z;
                z = -temp;
                
                float tempN = ny;
                ny = nz;
                nz = -tempN;
            }
            
            vertexPositions.push_back(x);
            vertexPositions.push_back(y);
            vertexPositions.push_back(z);
            
            vertexNormals.push_back(nx);
            vertexNormals.push_back(ny);
            vertexNormals.push_back(nz);
        }
    }
    
    // Use topology smoothing (RigTriangles) with 10 passes to fix joint boundaries
    std::vector<int> boneIndices = gsrcAutorig.RigTriangles(vertexPositions, vertexNormals, 10, useDepthPenalty);
    VortigauntLog::Vortigaunt_Printf(QStringLiteral("^2Rigged ^5%1 ^2vertices with topology smoothing.").arg(boneIndices.size()));
    setProgress(80);
    
    VortigauntLog::Vortigaunt_Printf("^2Writing output SMD...");
    QApplication::processEvents();
    
    SmdWriter writer;
    writer.SetSkeleton(gsrcAutorig.GetBones());
    
    // Apply scale/flip to triangles for output
    for (auto& tri : triangles) {
        for (int v = 0; v < 3; v++) {
            tri.vertices[v].x *= scale;
            tri.vertices[v].y *= scale;
            tri.vertices[v].z *= scale;
            
            if (flipYZ) {
                float tempPos = tri.vertices[v].y;
                tri.vertices[v].y = tri.vertices[v].z;
                tri.vertices[v].z = -tempPos;
                
                float tempNorm = tri.vertices[v].ny;
                tri.vertices[v].ny = tri.vertices[v].nz;
                tri.vertices[v].nz = -tempNorm;
            }
        }
    }
    
    if (!writer.ExportSmdTriangles(outPath.toStdString(), triangles, boneIndices)) {
        VortigauntLog::Vortigaunt_Printf(QStringLiteral("^7ERROR: ^9%1").arg(QString::fromStdString(writer.GetError())));
        m_rigButton->setEnabled(true);
        m_progressBar->setVisible(false);
        return;
    }

    setProgress(100);
    VortigauntLog::Vortigaunt_Printf(QStringLiteral("^2SUCCESS! ^9Output saved to: ^1%1").arg(outPath));

    m_rigButton->setEnabled(true);
    m_progressBar->setVisible(false);

    QMessageBox::information(this, "Success", 
        "Mesh rigged successfully!\n\nOutput: " + outPath);
}



void AutoRigDialog::setProgress(int value) {
    m_progressBar->setValue(value);
    QApplication::processEvents();
}

bool AutoRigDialog::eventFilter(QObject* obj, QEvent* event) {
    if (obj == m_tipImageLabel && event->type() == QEvent::MouseButtonPress) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton && !m_tipPixmap.isNull()) {
            // Create a preview dialog
            QDialog* previewDialog = new QDialog(this);
            previewDialog->setWindowTitle(tr("Auto-Rig Tip - Full Preview"));
            previewDialog->setAttribute(Qt::WA_DeleteOnClose);

            QVBoxLayout* layout = new QVBoxLayout(previewDialog);
            layout->setContentsMargins(0, 0, 0, 0);

            QScrollArea* scrollArea = new QScrollArea();
            scrollArea->setWidgetResizable(true);
            scrollArea->setStyleSheet("QScrollArea { border: none; background: #111; }");

            QLabel* imageLabel = new QLabel();
            // Scale to fit screen nicely (90% of screen size)
            QScreen* screen = this->screen();
            QSize screenSize = screen ? screen->availableSize() : QSize(1920, 1080);
            int maxW = static_cast<int>(screenSize.width() * 0.9);
            int maxH = static_cast<int>(screenSize.height() * 0.9);

            QPixmap displayPixmap = m_tipPixmap.scaled(maxW, maxH, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            imageLabel->setPixmap(displayPixmap);
            imageLabel->setAlignment(Qt::AlignCenter);

            scrollArea->setWidget(imageLabel);
            layout->addWidget(scrollArea);

            previewDialog->resize(displayPixmap.width() + 20, displayPixmap.height() + 20);
            previewDialog->exec();
            return true;
        }
    }
    return QDialog::eventFilter(obj, event);
}

void AutoRigDialog::onMeshPathChanged(const QString& path) {
    QString trimmedPath = path.trimmed();
    if (trimmedPath.isEmpty()) {
        m_loadedBones.clear();
        m_boneTreeWidget->clear();
        m_boneItemMap.clear();
        updateBoneCountLabel();
        return;
    }

    SmdParser parser;
    if (parser.Parse(trimmedPath.toStdString())) {
        m_loadedBones = parser.GetBones();
        populateBoneTree(m_loadedBones);
    } else {
        m_loadedBones.clear();
        m_boneTreeWidget->clear();
        m_boneItemMap.clear();
        updateBoneCountLabel();
    }
}

void AutoRigDialog::populateBoneTree(const std::vector<SmdBone>& bones) {
    m_boneTreeWidget->clear();
    m_boneItemMap.clear();

    if (bones.empty()) {
        updateBoneCountLabel();
        return;
    }

    std::vector<int> roots;
    std::vector<std::vector<int>> children(bones.size());
    for (size_t i = 0; i < bones.size(); i++) {
        int parentIdx = bones[i].parentIndex;
        if (parentIdx < 0 || parentIdx >= static_cast<int>(bones.size()) || parentIdx == static_cast<int>(i)) {
            roots.push_back(static_cast<int>(i));
        } else {
            children[parentIdx].push_back(static_cast<int>(i));
        }
    }

    std::function<void(QTreeWidgetItem*, int)> addBoneNode = [&](QTreeWidgetItem* parentItem, int boneIdx) {
        const auto& bone = bones[boneIdx];
        QTreeWidgetItem* item = new QTreeWidgetItem();
        item->setText(0, QString::fromStdString(bone.name));
        item->setData(0, Qt::UserRole, boneIdx);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        item->setCheckState(0, Qt::Checked);

        if (parentItem) {
            parentItem->addChild(item);
        } else {
            m_boneTreeWidget->addTopLevelItem(item);
        }

        m_boneItemMap[boneIdx] = item;

        for (int childIdx : children[boneIdx]) {
            addBoneNode(item, childIdx);
        }
    };

    for (int rootIdx : roots) {
        addBoneNode(nullptr, rootIdx);
    }

    m_boneTreeWidget->expandAll();
    updateBoneCountLabel();
}

void AutoRigDialog::onSearchBones(const QString& text) {
    QString query = text.trimmed();

    std::function<bool(QTreeWidgetItem*)> filterItem = [&](QTreeWidgetItem* item) -> bool {
        bool matches = item->text(0).contains(query, Qt::CaseInsensitive);
        bool hasVisibleChild = false;

        for (int i = 0; i < item->childCount(); ++i) {
            if (filterItem(item->child(i))) {
                hasVisibleChild = true;
            }
        }

        bool visible = matches || hasVisibleChild || query.isEmpty();
        item->setHidden(!visible);
        if (visible && !query.isEmpty()) {
            item->setExpanded(true);
        }
        return visible;
    };

    for (int i = 0; i < m_boneTreeWidget->topLevelItemCount(); ++i) {
        filterItem(m_boneTreeWidget->topLevelItem(i));
    }
}

void AutoRigDialog::onSelectAllBones() {
    m_boneTreeWidget->blockSignals(true);
    for (const auto& [idx, item] : m_boneItemMap) {
        item->setCheckState(0, Qt::Checked);
    }
    m_boneTreeWidget->blockSignals(false);
    updateBoneCountLabel();
}

void AutoRigDialog::onDeselectAllBones() {
    m_boneTreeWidget->blockSignals(true);
    for (const auto& [idx, item] : m_boneItemMap) {
        item->setCheckState(0, Qt::Unchecked);
    }
    m_boneTreeWidget->blockSignals(false);
    updateBoneCountLabel();
}

void AutoRigDialog::updateBoneCountLabel() {
    int total = static_cast<int>(m_boneItemMap.size());
    if (total == 0) {
        m_boneCountLabel->setText(tr("No skeleton loaded"));
        return;
    }

    int activeCount = 0;
    for (const auto& [idx, item] : m_boneItemMap) {
        if (item->checkState(0) == Qt::Checked) {
            activeCount++;
        }
    }

    m_boneCountLabel->setText(QString(tr("%1 / %2 bones active")).arg(activeCount).arg(total));
}
