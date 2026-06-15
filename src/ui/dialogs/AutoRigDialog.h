#pragma once

#include <QDialog>
#include <QString>
#include <QPixmap>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QGroupBox>
#include <unordered_map>
#include "core/smd/SmdParser.h"

class QLineEdit;
class QPushButton;
class QDoubleSpinBox;
class QPlainTextEdit;
class QProgressBar;
class QCheckBox;
class QLabel;
class QEvent;

// Dialog for GoldSrc Auto-Rig (Beta) feature
// Uses hardcoded Counter-Strike 1.6 skeleton reference
// Supports A-pose to T-pose conversion and auto-scaling
class AutoRigDialog : public QDialog {
    Q_OBJECT

public:
    explicit AutoRigDialog(QWidget* parent = nullptr);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onBrowseMesh();
    void onBrowseOutput();
    void onRig();
    void onMeshPathChanged(const QString& path);
    void onSearchBones(const QString& text);
    void onSelectAllBones();
    void onDeselectAllBones();

private:
    // UI Elements
    QLabel* m_skeletonRefLabel;  // Shows "Counter-Strike 1.6 Default Player Model Bone"
    
    QLineEdit* m_meshEdit;
    QPushButton* m_browseMeshButton;
    
    QLineEdit* m_outputEdit;
    QPushButton* m_browseOutputButton;
    
    QDoubleSpinBox* m_scaleSpinBox;
    QCheckBox* m_flipYZCheck;
    QCheckBox* m_depthPenaltyCheck;
    QCheckBox* m_autoScaleCheck;
    
    QPushButton* m_rigButton;
    QProgressBar* m_progressBar;
    QPlainTextEdit* m_logEdit;

    QLabel* m_tipImageLabel;
    QPixmap m_tipPixmap;              // Original full-size pixmap for preview
    
    // Bones Exclusion UI
    QGroupBox* m_bonesGroupBox;
    QLineEdit* m_boneSearchEdit;
    QTreeWidget* m_boneTreeWidget;
    QPushButton* m_selectAllBonesButton;
    QPushButton* m_deselectAllBonesButton;
    QLabel* m_boneCountLabel;
    
    // Skeleton Data
    std::vector<SmdBone> m_loadedBones;
    std::unordered_map<int, QTreeWidgetItem*> m_boneItemMap;
    
    void setProgress(int value);
    void populateBoneTree(const std::vector<SmdBone>& bones);
    void updateBoneCountLabel();
};
