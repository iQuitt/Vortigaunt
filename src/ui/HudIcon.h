// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#pragma once

#include <QImage>
#include <QString>

#include <string>
#include <vector>

// 640hud weapon list icons drawn in the game's look; the sprite is additive, so black is clear.

struct HudIconLook
{
    QString label = QStringLiteral("PRIMARY");
    int brightness = 0;    // added to the weapon's greys
    float contrast = 1.0f; // about the middle grey
    bool alignRight = true;
    bool nameOverWeapon = false;  // as in CSO
};

namespace hudicon
{
constexpr int kCellWidth = 170;
constexpr int kCellHeight = 45;
constexpr int kSheetSize = 256;

// the weapon with alpha, from renders over black and over white
QImage Separate(const QImage& overBlack, const QImage& overWhite);

// turned to lie level
QImage Level(const QImage& weapon);

// whether the muzzle is on the left
bool MuzzleLeft(const QImage& weapon);

// one cell of the sprite
QImage MakeCell(const QImage& weapon, const HudIconLook& look, bool selected);

// cells down the left of a sheet
QImage MakeSheet(const std::vector<QImage>& cells);

// the sheet as an additive GoldSrc sprite
bool WriteSprite(const QImage& sheet, const std::string& path);

// which cells of a sheet are used
std::vector<bool> UsedCells(const QImage& sheet);

// the weapon's hud .txt lines for the cells at row and row + 1
QString HudText(const QString& spriteName, int row = 0);
}
