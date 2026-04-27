#include "SpriteFixDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QLabel>
#include <QDialogButtonBox>

#include "LanguageManager.h"

// This file just dialog of converting v3 sprite to v2

SpriteFixDialog::SpriteFixDialog(int currentSpriteType, int currentTextureFormat, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Fix Sprite - V3 to V2 "));
    resize(400, 200);
    
    auto* mainLayout = new QVBoxLayout(this);
    
    // Sprite Type selection
    auto* typeGroup = new QGroupBox(tr("Sprite Type"));
    auto* typeLayout = new QHBoxLayout();
    
    m_spriteTypeCombo = new QComboBox();
    m_spriteTypeCombo->addItem(tr("Parallel"), 2);
    m_spriteTypeCombo->addItem(tr("Parallel Upright"), 0);
    m_spriteTypeCombo->addItem(tr("Oriented"), 3);
    m_spriteTypeCombo->addItem(tr("Parallel Oriented"), 4);
    m_spriteTypeCombo->addItem(tr("Facing Upright"), 1);
    
    int typeIndex = m_spriteTypeCombo->findData(currentSpriteType);
    if (typeIndex >= 0)
        m_spriteTypeCombo->setCurrentIndex(typeIndex);
    
    QString currentTypeName = getSpriteTypeName(currentSpriteType);
    QLabel* typeCurrentLabel = new QLabel(tr("(Current: %1)").arg(currentTypeName));
    
    typeLayout->addWidget(m_spriteTypeCombo);
    typeLayout->addWidget(typeCurrentLabel);
    typeLayout->addStretch();
    typeGroup->setLayout(typeLayout);
    
    // Texture Format selection
    auto* formatGroup = new QGroupBox(tr("Texture Format"));
    auto* formatLayout = new QHBoxLayout();
    
    m_textureFormatCombo = new QComboBox();
    m_textureFormatCombo->addItem(tr("Normal"), 0);
    m_textureFormatCombo->addItem(tr("Additive"), 1);
    m_textureFormatCombo->addItem(tr("IndexAlpha"), 2);
    m_textureFormatCombo->addItem(tr("AlphaTest"), 3);
    
    // Set current selection
    int formatIndex = m_textureFormatCombo->findData(currentTextureFormat);
    if (formatIndex >= 0)
        m_textureFormatCombo->setCurrentIndex(formatIndex);
    
    QString currentFormatName = getTextureFormatName(currentTextureFormat);
    QLabel* formatCurrentLabel = new QLabel(tr("(Current: %1)").arg(currentFormatName));
    
    formatLayout->addWidget(m_textureFormatCombo);
    formatLayout->addWidget(formatCurrentLabel);
    formatLayout->addStretch();
    formatGroup->setLayout(formatLayout);
    
    // Buttons
    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    
    mainLayout->addWidget(typeGroup);
    mainLayout->addWidget(formatGroup);
    mainLayout->addWidget(buttonBox);
}

QString SpriteFixDialog::getSpriteTypeName(int type)
{
    switch (type) {
        case 0: return tr("Parallel Upright");
        case 1: return tr("Facing Upright");
        case 2: return tr("Parallel");
        case 3: return tr("Oriented");
        case 4: return tr("Parallel Oriented");
        default: return tr("Unknown (%1)").arg(type);
    }
}

QString SpriteFixDialog::getTextureFormatName(int format)
{
    switch (format) {
        case 0: return tr("Normal");
        case 1: return tr("Additive");
        case 2: return tr("IndexAlpha");
        case 3: return tr("AlphaTest");
        default: return tr("Unknown");
    }
}

