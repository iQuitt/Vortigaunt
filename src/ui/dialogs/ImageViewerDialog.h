#pragma once

#include <QtWidgets/QDialog>
#include <QImage>
#include <QByteArray>

class QLabel;
class QPushButton;

class ImageViewerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ImageViewerDialog(QWidget* parent = nullptr);

    bool loadImageFile(const QString& filePath);
    bool loadImageFromMemory(const QByteArray& data, const QString& displayName = QString());

private slots:
    void onOpenImage();
    void onSaveAs();

private:
    void updatePreview();

    QLabel*      m_imageLabel;
    QLabel*      m_infoLabel;
    QPushButton* m_openButton;
    QPushButton* m_saveAsButton;

    QString m_currentImagePath;
    QString m_currentDisplayName;
    QImage  m_currentImage;
};
