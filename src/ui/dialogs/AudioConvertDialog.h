#pragma once

#include <QDialog>
#include <QListWidget>
#include <QString>
#include <QStringList>

class QLineEdit;
class QPushButton;
class QLabel;
class QComboBox;

/**
 * @brief Dialog for converting MP3/OGG audio files to WAV format
 * 
 * Features:
 * - Import MP3/OGG files
 * - Configure output settings (sample rate, bit depth)
 * - Convert to WAV with 16-bit, 22050Hz, Mono output (default)
 * - Output to VortigauntOutput/AudioConvert folder
 */
class AudioConvertDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AudioConvertDialog(QWidget* parent = nullptr);
    ~AudioConvertDialog();

private slots:
    void onAddFiles();
    void onRemoveSelected();
    void onClearAll();
    void onBrowseOutput();
    void onConvert();
    void onConvertAll();

private:
    QString getOutputDirectory();
    void updateOutputInfoLabel();
    
    // File list
    QListWidget* m_fileList;
    
    // Output settings
    QLineEdit* m_outputPathEdit;
    QComboBox* m_sampleRateCombo;
    QComboBox* m_bitDepthCombo;
    
    // Buttons
    QPushButton* m_addButton;
    QPushButton* m_removeButton;
    QPushButton* m_clearButton;
    QPushButton* m_browseButton;
    QPushButton* m_convertButton;
    QPushButton* m_convertAllButton;
    QPushButton* m_closeButton;
    
    // Output info
    QLabel* m_outputInfoLabel;
};
