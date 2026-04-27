#pragma once

#include <QDialog>
#include <QComboBox>
#include <QLabel>

class SpriteFixDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SpriteFixDialog(int currentSpriteType, int currentTextureFormat, QWidget* parent = nullptr);
    ~SpriteFixDialog() override = default;

    int getSelectedSpriteType() const { return m_spriteTypeCombo->currentData().toInt(); }
    int getSelectedTextureFormat() const { return m_textureFormatCombo->currentData().toInt(); }

private:
    QComboBox* m_spriteTypeCombo;
    QComboBox* m_textureFormatCombo;
    
    QString getSpriteTypeName(int type);
    QString getTextureFormatName(int format);
};

