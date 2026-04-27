#pragma once

#include <QtWidgets/QDialog>
#include <QImage>
#include <QByteArray>

#include <vector>

class QLabel;
class QPushButton;

class DtxViewerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DtxViewerDialog(QWidget* parent = nullptr);
    
    // Load a DTX file from a specific path (can be called externally)
    bool loadDtxFile(const QString& filePath);
    bool loadDtxFromMemory(const QByteArray& data, const QString& displayName = QString());

private slots:
    void onOpenDtx();
    void onSaveBmp();

private:
    void updatePreview();
    bool displayDecodedImage(const std::vector<unsigned int>& pixels,
                             int width,
                             int height,
                             const QString& label);

    QLabel*      m_imageLabel;
    QLabel*      m_infoLabel;
    QPushButton* m_openButton;
    QPushButton* m_saveBmpButton;

    QString m_currentDtxPath;
    QString m_currentDisplayName;
    QImage  m_currentImage;
};



