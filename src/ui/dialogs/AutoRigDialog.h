#pragma once

#include <QDialog>
#include <QString>
#include <QPixmap>

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
    
    void setProgress(int value);
};
