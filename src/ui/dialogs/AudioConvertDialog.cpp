#include "AudioConvertDialog.h"
#include "AudioConvert.h"
#include "SettingsDialog.h"
#include "ui/UiUtils.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QComboBox>
#include <QScreen>
#include <QApplication>

AudioConvertDialog::AudioConvertDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("WAV Convert"));
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint | Qt::WindowMinimizeButtonHint);
    setMinimumSize(400, 300);
    
    UiUtils::resizeToScreen(this, 0.5);

    auto* mainLayout = new QVBoxLayout(this);
    
    // Instructions
    auto* instructionLabel = new QLabel(tr(
        "Convert MP3/OGG/WAV audio files to WAV format.\n"
        " Recommended Values: 16-bit, 22050 Hz, Mono\n"
    ));
    instructionLabel->setWordWrap(true);
    mainLayout->addWidget(instructionLabel);
    
    // File list group
    auto* filesGroup = new QGroupBox(tr("Audio Files"));
    auto* filesLayout = new QVBoxLayout(filesGroup);
    
    m_fileList = new QListWidget();
    m_fileList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_fileList->setMinimumHeight(150);
    filesLayout->addWidget(m_fileList);
    
    // File buttons
    auto* fileButtonLayout = new QHBoxLayout();
    m_addButton = new QPushButton(tr("Add Audio Files..."));
    m_removeButton = new QPushButton(tr("Remove Selected"));
    m_clearButton = new QPushButton(tr("Clear All"));
    fileButtonLayout->addWidget(m_addButton);
    fileButtonLayout->addWidget(m_removeButton);
    fileButtonLayout->addWidget(m_clearButton);
    fileButtonLayout->addStretch();
    filesLayout->addLayout(fileButtonLayout);
    
    mainLayout->addWidget(filesGroup);
    
    auto* settingsGroup = new QGroupBox(tr("Output Settings"));
    auto* settingsLayout = new QHBoxLayout(settingsGroup);
    
    settingsLayout->addWidget(new QLabel(tr("Sample Rate:")));
    m_sampleRateCombo = new QComboBox();
    m_sampleRateCombo->addItem("11025 Hz", 11025);
    m_sampleRateCombo->addItem("22050 Hz", 22050);
    m_sampleRateCombo->setCurrentIndex(1); // Set to  22050
    settingsLayout->addWidget(m_sampleRateCombo);
    
    settingsLayout->addSpacing(20);
    
    settingsLayout->addWidget(new QLabel(tr("Bit Depth:")));
    m_bitDepthCombo = new QComboBox();
    m_bitDepthCombo->addItem("8-bit", 8);
    m_bitDepthCombo->addItem("16-bit", 16);
    m_bitDepthCombo->setCurrentIndex(1); // Set to 16-bit
    settingsLayout->addWidget(m_bitDepthCombo);
    
    
    
    settingsLayout->addStretch();
    mainLayout->addWidget(settingsGroup);
    
    // Output group
    auto* outputGroup = new QGroupBox(tr("Output Directory"));
    auto* outputLayout = new QHBoxLayout(outputGroup);
    outputLayout->addWidget(new QLabel(tr("Folder:")));
    m_outputPathEdit = new QLineEdit();
    m_outputPathEdit->setPlaceholderText(tr("Currently Output Dir: VortigauntOutput/AudioConvert..."));
    m_browseButton = new QPushButton(tr("Browse..."));
    outputLayout->addWidget(m_outputPathEdit, 1);
    outputLayout->addWidget(m_browseButton);
    mainLayout->addWidget(outputGroup);
    
    m_outputInfoLabel = new QLabel();
    m_outputInfoLabel->setStyleSheet("color: gray; font-style: italic;");
    updateOutputInfoLabel();
    mainLayout->addWidget(m_outputInfoLabel);
    
    // Dialog buttons
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    m_convertButton = new QPushButton(tr("Convert Selected"));
    m_convertAllButton = new QPushButton(tr("Convert All"));
    m_convertAllButton->setDefault(true);
    m_closeButton = new QPushButton(tr("Close"));
    buttonLayout->addWidget(m_convertButton);
    buttonLayout->addWidget(m_convertAllButton);
    buttonLayout->addWidget(m_closeButton);
    mainLayout->addLayout(buttonLayout);
    
    // Connections - File operations
    connect(m_addButton, &QPushButton::clicked, this, &AudioConvertDialog::onAddFiles);
    connect(m_removeButton, &QPushButton::clicked, this, &AudioConvertDialog::onRemoveSelected);
    connect(m_clearButton, &QPushButton::clicked, this, &AudioConvertDialog::onClearAll);
    connect(m_browseButton, &QPushButton::clicked, this, &AudioConvertDialog::onBrowseOutput);
    connect(m_convertButton, &QPushButton::clicked, this, &AudioConvertDialog::onConvert);
    connect(m_convertAllButton, &QPushButton::clicked, this, &AudioConvertDialog::onConvertAll);
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::close);
    
    // Update output info when path changes
    connect(m_outputPathEdit, &QLineEdit::textChanged, this, &AudioConvertDialog::updateOutputInfoLabel);
}

AudioConvertDialog::~AudioConvertDialog()
{
}

QString AudioConvertDialog::getOutputDirectory()
{
    // Use custom path if set, otherwise default to VortigauntOutput/AudioConvert
    QString customPath = m_outputPathEdit->text().trimmed();
    
    QString outputDir;
    if (!customPath.isEmpty())
    {
        outputDir = customPath;
    }
    else
    {
        outputDir = QDir(SettingsDialog::getOutputRootDir()).filePath("AudioConvert");
    }
    
    // Create directory if it doesn't exist
    QDir dir(outputDir);
    if (!dir.exists())
    {
        dir.mkpath(".");
    }
    
    return outputDir;
}

void AudioConvertDialog::updateOutputInfoLabel()
{
    QString customPath = m_outputPathEdit->text().trimmed();
    QString outputDir;
    
    if (!customPath.isEmpty())
    {
        outputDir = customPath;
    }
    else
    {
        outputDir = QDir(SettingsDialog::getOutputRootDir()).filePath("AudioConvert");
    }
    
}

void AudioConvertDialog::onBrowseOutput()
{
    QString path = QFileDialog::getExistingDirectory(
        this,
        tr("Select Output Directory"),
        m_outputPathEdit->text()
    );
    
    if (!path.isEmpty())
    {
        m_outputPathEdit->setText(path);
    }
}

void AudioConvertDialog::onAddFiles()
{
    QStringList files = QFileDialog::getOpenFileNames(
        this,
        tr("Select Audio Files"),
        QString(),
        tr("Audio Files (*.mp3 *.ogg *.wav);;MP3 Files (*.mp3);;OGG Files (*.ogg);;WAV Files (*.wav);;All Files (*.*)")
    );
    
    for (const QString& file : files)
    {
        QFileInfo info(file);
        QString displayName = info.fileName();
        
        // Check if already in list
        bool found = false;
        for (int i = 0; i < m_fileList->count(); ++i)
        {
            if (m_fileList->item(i)->data(Qt::UserRole).toString() == file)
            {
                found = true;
                break;
            }
        }
        
        if (!found)
        {
            QListWidgetItem* item = new QListWidgetItem(displayName);
            item->setData(Qt::UserRole, file);
            item->setToolTip(file);
            m_fileList->addItem(item);
        }
    }
}

void AudioConvertDialog::onRemoveSelected()
{
    QList<QListWidgetItem*> selected = m_fileList->selectedItems();
    for (QListWidgetItem* item : selected)
    {
        delete m_fileList->takeItem(m_fileList->row(item));
    }
}

void AudioConvertDialog::onClearAll()
{
    m_fileList->clear();
}

void AudioConvertDialog::onConvert()
{
    QList<QListWidgetItem*> selected = m_fileList->selectedItems();
    if (selected.isEmpty())
    {
        QMessageBox::warning(this, tr("No Selection"), tr("Please select files to convert."));
        return;
    }
    
    AudioConverter converter;
    AudioConvertSettings settings;
    settings.sampleRate = m_sampleRateCombo->currentData().toUInt();
    settings.bitDepth = m_bitDepthCombo->currentData().toUInt();
    settings.channels = 1; // Always mono (GoldSrc requirement)
    
    QString outputDir = getOutputDirectory();
    int successCount = 0;
    int failCount = 0;
    
    for (QListWidgetItem* item : selected)
    {
        QString inputPath = item->data(Qt::UserRole).toString();
        QFileInfo info(inputPath);
        
        QString outputPath = QDir(outputDir).filePath(info.baseName() + ".wav");
        
        AudioConvertResult result = converter.convertToWav(
            inputPath.toStdString(),
            outputPath.toStdString(),
            settings
        );
        
        if (result == AUDIO_CONVERT_OK)
        {
            successCount++;
        }
        else
        {
            failCount++;
        }
    }
    
    QString message = tr("Conversion complete!\n\nSuccessful: %1").arg(successCount);
    if (failCount > 0)
    {
        message += tr("\nFailed: %1").arg(failCount);
    }
    message += tr("\n\nOutput folder: %1").arg(outputDir);
    
    QMessageBox::information(this, tr("Conversion Result"), message);
}

void AudioConvertDialog::onConvertAll()
{
    if (m_fileList->count() == 0)
    {
        QMessageBox::warning(this, tr("No Files"), tr("Please add audio files to convert."));
        return;
    }
    
    AudioConverter converter;
    AudioConvertSettings settings;
    settings.sampleRate = m_sampleRateCombo->currentData().toUInt();
    settings.bitDepth = m_bitDepthCombo->currentData().toUInt();
    settings.channels = 1; // Always mono (GoldSrc requirement)
    
    QString outputDir = getOutputDirectory();
    int successCount = 0;
    int failCount = 0;
    
    for (int i = 0; i < m_fileList->count(); ++i)
    {
        QString inputPath = m_fileList->item(i)->data(Qt::UserRole).toString();
        QFileInfo info(inputPath);
        
        QString outputPath = QDir(outputDir).filePath(info.baseName() + ".wav");
        
        AudioConvertResult result = converter.convertToWav(
            inputPath.toStdString(),
            outputPath.toStdString(),
            settings
        );
        
        if (result == AUDIO_CONVERT_OK)
        {
            successCount++;
        }
        else
        {
            failCount++;
        }
    }
    
    QString message = tr("Conversion complete!\n\nSuccessful: %1 of %2").arg(successCount).arg(m_fileList->count());
    if (failCount > 0)
    {
        message += tr("\nFailed: %1").arg(failCount);
    }
    message += tr("\n\nOutput folder: %1").arg(outputDir);
    
    QMessageBox::information(this, tr("Conversion Result"), message);
}
