#pragma once

#include <QDialog>

class QCheckBox;
class QGroupBox;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QcPreview;

// Retargets Metin2 motions onto a reference model's QC, compiles and previews it.
class RetargetDialog : public QDialog
{
    Q_OBJECT
public:
    explicit RetargetDialog(QWidget* parent = nullptr);

    bool isBusy() const;

public slots:
    void reject() override;

private:
    void onRetarget();
    void setBusy(bool busy);

    QLineEdit* m_characterEdit = nullptr;
    QListWidget* m_motionList = nullptr;
    QLineEdit* m_qcEdit = nullptr;
    QLineEdit* m_animsEdit = nullptr;
    QCheckBox* m_loopCheck = nullptr;
    QGroupBox* m_compileGroup = nullptr;
    QLineEdit* m_studiomdlEdit = nullptr;
    QPlainTextEdit* m_report = nullptr;
    QPushButton* m_retargetButton = nullptr;
    QcPreview* m_preview = nullptr;
    bool m_busy = false;
};
