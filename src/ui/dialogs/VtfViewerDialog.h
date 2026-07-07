#pragma once

#include <QtWidgets/QDialog>
#include <QImage>

#include <memory>

class QLabel;
class QPushButton;
class QSpinBox;

namespace VTFLib { class CVTFFile; }

// Viewer for Valve Texture Format (.vtf) files using VTFLib.
class VtfViewerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit VtfViewerDialog(QWidget* parent = nullptr);
    ~VtfViewerDialog() override;

    // Load a VTF file from a specific path (can be called externally)
    bool loadVtfFile(const QString& filePath);

private slots:
    void onOpenVtf();
    void onSaveBmp();
    void onSubImageChanged();

private:
    void updatePreview();
    bool decodeCurrentImage();
    void updateInfoLabel();

    QLabel*      m_imageLabel;
    QLabel*      m_infoLabel;
    QPushButton* m_openButton;
    QPushButton* m_saveBmpButton;

    QLabel*      m_frameLabel;
    QSpinBox*    m_frameSpin;
    QLabel*      m_faceLabel;
    QSpinBox*    m_faceSpin;
    QLabel*      m_mipLabel;
    QSpinBox*    m_mipSpin;

    std::unique_ptr<VTFLib::CVTFFile> m_vtfFile;
    QString m_currentVtfPath;
    QImage  m_currentImage;
};
