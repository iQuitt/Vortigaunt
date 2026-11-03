// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#include "HudIcon.h"

#include "core/SpriteLoader.h"

#include <QDir>
#include <QPainter>
#include <QPainterPath>
#include <QTemporaryDir>
#include <algorithm>
#include <cmath>

namespace
{

// panel greys
constexpr int kStripeDark = 21;
constexpr int kStripeLight = 29;
constexpr int kTopEdge = 15;
constexpr int kBottomEdge = 19;
constexpr int kOutline = 8;         // the dark line about the weapon
constexpr int kLabelGrey = 157;     // a slot's name, not selected
constexpr int kWeaponLow = 70;      // weapon grey range
constexpr int kWeaponHigh = 236;
constexpr float kSelectedGain = 1.1f;
constexpr int kGlow = 210;          // the selected one's halo at its brightest
// 640hud glyph coverage (0-f), 8 rows; missing glyphs built from parts
struct Letter
{
    char ch;
    int advance;
    const char* rows;
};
constexpr Letter kLetters[] = {
    { '-', 7, "0 0 0 03ffffb 07ffff8 0 0 0" },
    { '.', 4, "0 0 0 0 0 0 0af6 0ef3" },
    { '0', 9, "09fffffc 2fd889fd 7fa004fc 09f6007f9 0df000af6 0fd000df 05ff99afd 03ffffff7" },
    { '1', 6, "006d7 07ff6 02adf2 000ff 005fe 007fb 000bf7 000ef4" },
    { '2', 9, "09fffffc 08d889fd 000004fc 00086bff3 00dffff8 0fd4 05ff98886 03ffffff9" },
    { '3', 9, "09fffffc 088889fd 000004fc 00488bfa 00cfffd 000000df 06888afd 07ffffff7" },
    { '4', 9, "000bdd 00afff 05fffd 00ffffa 0efedf6 8ffeffe 0dfffffd 00008fd" },
    { '5', 9, "07efffff 0df98988 2fc 06feccc9 00bdddff 000000ee 009999afc 03fffffe5" },
    { '6', 9, "09fffffc 2fd88884 7fa 09f98883 0dffffd2 0fd000df 05ff99afd 03ffffff7" },
    { '7', 9, "0cccccc9 0ffffffa 000005fc 000009f6 0003fa 0006f6 000af3 000df" },
    { '8', 9, "09fffffc 2fd889fd 7fa004fc 08fd77ef3 0cfffffd 0fd000df 05ff99afd 03ffffff7" },
    { '9', 9, "09fffffc 2fd889fd 7fa004fc 09f6007f9 07fffffff6 000000df 088888afd 09ffffff7" },
    { 'A', 8, "002ddb 00affd 02ffff 009ffff 00ff9ff 08ffcff 00ffffff5 08fe00ff4" },
    { 'B', 9, "0cccccc4 0ffffff8 48600cf8 08fd77ef3 0cfffffd 0ff005fb 02ff77cf9 06ffffff4" },
    { 'C', 9, "03dfffff6 0af98888 0ff 00fc 06fa 00ff2 00cfb9999 00afffffd" },
    { 'D', 9, "07fffffd 058888ee 0bd002fd 00fc006f9 06fa009f6 08f700cf 00cfb99ff 00efffff8" },
    { 'E', 9, "09cccccc 0cfffffe 044 05fe88 08fffb 0bf743 00ff98886 02ffffff9" },
    { 'F', 8, "0cccccc9 0ffffffa 243 08fc76 0cfff8 0ff54 02fd 06fa" },
    { 'G', 9, "08ccccc6 6ffffff7 9f9 0bf77777 0ffdfffc 4fd349f8 06fd77df4 07ffffff" },
    { 'H', 9, "0cc003c9 0fe007fa 6fc00af7 08fb77df4 0cffffff 0ff547fc 02fd007f9 06fa00af6" },
    { 'I', 4, "06d7 09f6 0df2 00ff 05fe 07fb 00bf7 00ef4" },
    { 'J', 9, "000003c9 000007fa 000004fc 0000007f9 000000af6 0fd000df 05ff99afd 03ffffff7" },
    { 'K', 9, "00aa008 05fc0ae 07f7bed 00bfeec3 00ffed6 04fdcec4 006f907cc 00af8005d" },
    { 'L', 8, "0bb 2fb 8f8 0bf4 0ff 4ff 07ff885 09ffff9" },
    { 'M', 10, "03dd0008da 06ff602ff9 09ff80cff6 00cffa6fff2 00fffedfff 04fdfffefc 008f9fffbfa 00af7efbaf7" },
    { 'N', 9, "09c800cc 0dfe03fc 0fff58fa 04fffaaf5 08ffffff3 0bf7ffff 00ef2bffc 00fe05ff7" },
    { 'O', 9, "09fffffc 2fd889fd 7fa004fc 09f6007f9 0df000af6 0fd000df 05ff99afd 03ffffff7" },
    { 'P', 9, "00dddddc4 03ffffff5 05a600ff4 00bf856ff 00ffffffa 02fd6664 006fb 009f7" },
    { 'Q', 9, "09fffffc 2fd889fd 7fa004fc 09f6007f9 0df000af6 0fd04adf 05ff9cffd 03fffffff9" },
    { 'R', 9, "06dddddb 08ffffff 09a002fe 00ff449fa 04ffffff4 08fb66ff2 00cf500ff 00ef003fd" },
    { 'S', 9, "07efffff 0df98988 2fc 06feccc9 00bdddff 000000ee 009999afc 03fffffe5" },
    { 'T', 8, "0cccccc9 0ffffffa 000bf8 0000ff 0002ff 0004fa 00006f5 0000bf5" },
    { 'U', 9, "0cc003c9 0fe007fa 7fa004fc 09f6007f9 0df000af6 0fd000df 05ff99afd 03ffffff7" },
    { 'V', 9, "0aa2006a6 2ff502ff8 0ff70eff3 00ffa4fff 00ffffff4 006ffff9 0000fff9 0000ffa4" },
    { 'W', 9, "0c70009a 0fb002ef 5fd000ff 09f6005fb 0fd3bc9f8 0fcbfebf4 40fffbdff2 00ef808fb" },
    { 'X', 9, "0df302ee5 09fa0cf8 01efcf8 00bfee3 00cfee3 05fdcec4 09f7007fa 0df2003ec" },
    { 'Y', 8, "0df32ee5 09f8cf8 07fffb 000efe 000df5 000ed 0005fc 0007f9" },
    { 'Z', 9, "09cccccc 0cfffffe 000005fe 00004fd2 0004fc2 004fc2 02ff98886 02ffffff9" },
};
// kerning pairs
struct LetterPair
{
    const char* pair;
    int change;
};
constexpr LetterPair kPairs[] = { { "AN", 1 }, { "C4", 1 }, { "DA", 1 }, { "EC", -1 }, { "EE", -1 }, { "HE", 1 }, { "LA", 1 }, { "LE", -1 }, { "MA", 1 }, { "RY", 1 }, { "SK", -1 } };
constexpr int kSpace = 3;
constexpr int kLean[] = { 2, 2, 2, 1, 1, 1, 0, 0 };  // italic lean per row
constexpr int kNameLeft = 10, kNameTop = 33;

// the name's coverage and its box
std::vector<float> nameCover(const QString& label, QRect& box)
{
    const int w = hudicon::kCellWidth;
    std::vector<float> cover(w * hudicon::kCellHeight, 0.0f);
    box = QRect();
    int pen = kNameLeft;
    for (qsizetype i = 0; i < label.size(); ++i)
    {
        const char ch = label[i].toUpper().toLatin1();
        const Letter* letter = std::find_if(std::begin(kLetters), std::end(kLetters), [ch](const Letter& l) { return l.ch == ch; });
        if (letter == std::end(kLetters))
        {
            pen += kSpace;
            continue;
        }
        int row = 0, column = 0;
        for (const char* p = letter->rows; *p; ++p)
        {
            if (*p == ' ')
            {
                ++row;
                column = 0;
                continue;
            }
            const int x = pen - 1 + kLean[row] + column++;
            const float v = (*p <= '9' ? *p - '0' : *p - 'a' + 10) / 15.0f;
            if (v <= 0 || x < 0 || x >= w) continue;
            float& pixel = cover[(kNameTop + row) * w + x];
            pixel = std::max(pixel, v);
            box |= QRect(x, kNameTop + row, 1, 1);
        }
        pen += letter->advance;
        if (i + 1 < label.size())
        {
            const char next = label[i + 1].toUpper().toLatin1();
            for (const LetterPair& pair : kPairs)
                if (pair.pair[0] == ch && pair.pair[1] == next) pen += pair.change;
        }
    }
    return cover;
}
// the weapon's area in a cell
constexpr int kWeaponLeft = 6, kWeaponRight = 167, kWeaponTop = 2, kWeaponBottom = 42;

int grey(QRgb c)
{
    return (qRed(c) * 299 + qGreen(c) * 587 + qBlue(c) * 114) / 1000;
}

// the striped panel with rounded corners
std::vector<float> panel()
{
    QImage shape(hudicon::kCellWidth, hudicon::kCellHeight, QImage::Format_Grayscale8);
    shape.fill(0);
    QPainter painter(&shape);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addRoundedRect(QRectF(0, 0, hudicon::kCellWidth, hudicon::kCellHeight), 3.0, 3.0);
    painter.fillPath(path, QColor(255, 255, 255));
    painter.end();
    std::vector<float> values(hudicon::kCellWidth * hudicon::kCellHeight);
    for (int y = 0; y < hudicon::kCellHeight; ++y)
    {
        const int stripe = y == 0 ? kTopEdge : y == hudicon::kCellHeight - 1 ? kBottomEdge : (y % 2 ? kStripeDark : kStripeLight);
        for (int x = 0; x < hudicon::kCellWidth; ++x)
            values[y * hudicon::kCellWidth + x] = stripe * shape.pixelColor(x, y).red() / 255.0f;
    }
    return values;
}

// box blur for the halo
std::vector<float> blurred(std::vector<float> values, int radius, int passes)
{
    const int w = hudicon::kCellWidth, h = hudicon::kCellHeight;
    std::vector<float> next(values.size());
    for (int pass = 0; pass < passes; ++pass)
    {
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
            {
                float sum = 0;
                int count = 0;
                for (int k = -radius; k <= radius; ++k)
                    if (x + k >= 0 && x + k < w)
                    {
                        sum += values[y * w + x + k];
                        count++;
                    }
                next[y * w + x] = sum / count;
            }
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
            {
                float sum = 0;
                int count = 0;
                for (int k = -radius; k <= radius; ++k)
                    if (y + k >= 0 && y + k < h)
                    {
                        sum += next[(y + k) * w + x];
                        count++;
                    }
                values[y * w + x] = sum / count;
            }
    }
    return values;
}

}  // namespace

namespace hudicon
{

QImage Separate(const QImage& overBlack, const QImage& overWhite)
{
    const QImage black = overBlack.convertToFormat(QImage::Format_RGB32), white = overWhite.convertToFormat(QImage::Format_RGB32);
    if (black.size() != white.size() || black.isNull()) return {};
    QImage weapon(black.size(), QImage::Format_ARGB32);
    QRect bounds;
    for (int y = 0; y < black.height(); ++y)
    {
        const QRgb* b = reinterpret_cast<const QRgb*>(black.constScanLine(y));
        const QRgb* w = reinterpret_cast<const QRgb*>(white.constScanLine(y));
        QRgb* out = reinterpret_cast<QRgb*>(weapon.scanLine(y));
        for (int x = 0; x < black.width(); ++x)
        {
            // coverage from the black and white renders
            const int uncovered = std::clamp(((qRed(w[x]) - qRed(b[x])) + (qGreen(w[x]) - qGreen(b[x])) + (qBlue(w[x]) - qBlue(b[x]))) / 3, 0, 255);
            const int alpha = 255 - uncovered;
            if (alpha < 4)
            {
                out[x] = qRgba(0, 0, 0, 0);
                continue;
            }
            auto unmix = [alpha](int c) { return std::clamp(c * 255 / alpha, 0, 255); };
            out[x] = qRgba(unmix(qRed(b[x])), unmix(qGreen(b[x])), unmix(qBlue(b[x])), alpha);
            bounds |= QRect(x, y, 1, 1);
        }
    }
    return bounds.isEmpty() ? QImage() : weapon.copy(bounds);
}

QImage Level(const QImage& weapon)
{
    // the turn with the tightest bounding box
    std::vector<QPointF> points;
    for (int y = 0; y < weapon.height(); y += 2)
        for (int x = 0; x < weapon.width(); x += 2)
            if (qAlpha(weapon.pixel(x, y)) > 128) points.emplace_back(x, y);
    if (points.size() < 2) return weapon;
    std::vector<std::pair<double, double>> areas;  // turn, area
    for (double turn = -45.0; turn <= 45.0; turn += 0.5)
    {
        const double c = std::cos(turn * 3.14159265358979 / 180.0), s = std::sin(turn * 3.14159265358979 / 180.0);
        double lowX = 1e9, highX = -1e9, lowY = 1e9, highY = -1e9;
        for (const QPointF& p : points)
        {
            const double x = p.x() * c - p.y() * s, y = p.x() * s + p.y() * c;
            lowX = std::min(lowX, x);
            highX = std::max(highX, x);
            lowY = std::min(lowY, y);
            highY = std::max(highY, y);
        }
        areas.emplace_back(turn, (highX - lowX) * (highY - lowY));
    }
    // prefer the straightest of the near-tightest turns
    double least = areas.front().second;
    for (const auto& [turn, area] : areas) least = std::min(least, area);
    double angle = 45.0;
    for (const auto& [turn, area] : areas)
        if (area <= least * 1.03 && std::abs(turn) < std::abs(angle)) angle = turn;
    angle = -angle;
    if (std::abs(angle) < 0.5) return weapon;
    const QImage turned = weapon.convertToFormat(QImage::Format_ARGB32_Premultiplied)
                              .transformed(QTransform().rotate(-angle), Qt::SmoothTransformation)
                              .convertToFormat(QImage::Format_ARGB32);
    QRect bounds;
    for (int y = 0; y < turned.height(); ++y)
        for (int x = 0; x < turned.width(); ++x)
            if (qAlpha(turned.pixel(x, y)) >= 4) bounds |= QRect(x, y, 1, 1);
    return bounds.isEmpty() ? weapon : turned.copy(bounds);
}

bool MuzzleLeft(const QImage& weapon)
{
    // coverage at each end
    auto thickness = [&](int from, int to) {
        double covered = 0;
        for (int x = from; x < to; ++x)
            for (int y = 0; y < weapon.height(); ++y) covered += qAlpha(weapon.pixel(x, y)) > 128 ? 1 : 0;
        return covered / std::max(1, to - from);
    };
    const int end = std::max(1, weapon.width() / 6);
    return thickness(0, end) <= thickness(weapon.width() - end, weapon.width());
}

QImage MakeCell(const QImage& weapon, const HudIconLook& look, bool selected)
{
    const int w = kCellWidth, h = kCellHeight;
    std::vector<float> value = panel();
    std::vector<float> cover(w * h, 0.0f), shade(w * h, 0.0f);
    QRect nameBox;
    const std::vector<float> name = nameCover(look.label, nameBox);
    nameBox.adjust(-1, -1, 1, 1);
    if (!weapon.isNull())
    {
        // largest fit, beside or above the name
        QImage fitted;
        int left = 0, top = 0;
        for (const int bottom : { kWeaponBottom, look.nameOverWeapon ? kWeaponBottom : nameBox.top() - 1 })
        {
            const QSize room(kWeaponRight - kWeaponLeft, bottom - kWeaponTop);
            fitted = weapon.convertToFormat(QImage::Format_ARGB32_Premultiplied)
                         .scaled(room, Qt::KeepAspectRatio, Qt::SmoothTransformation)
                         .convertToFormat(QImage::Format_ARGB32);
            left = look.alignRight ? kWeaponRight - fitted.width() : kWeaponLeft + (room.width() - fitted.width()) / 2;
            top = kWeaponTop + (room.height() - fitted.height()) / 2;
            int over = 0;
            for (int y = std::max(nameBox.top(), top); y < std::min(nameBox.bottom() + 1, top + fitted.height()); ++y)
                for (int x = std::max(nameBox.left(), left); x < std::min(nameBox.right() + 1, left + fitted.width()); ++x)
                    over += qAlpha(fitted.pixel(x - left, y - top)) > 64 ? 1 : 0;
            if (over == 0) break;
        }

        // stretch greys to the icon range
        std::vector<int> greys;
        for (int y = 0; y < fitted.height(); ++y)
            for (int x = 0; x < fitted.width(); ++x)
                if (qAlpha(fitted.pixel(x, y)) > 128) greys.push_back(grey(fitted.pixel(x, y)));
        std::sort(greys.begin(), greys.end());
        const float low = greys.empty() ? 0 : greys[greys.size() / 20], high = greys.empty() ? 255 : greys[greys.size() * 19 / 20];
        for (int y = 0; y < fitted.height(); ++y)
            for (int x = 0; x < fitted.width(); ++x)
            {
                const QRgb c = fitted.pixel(x, y);
                const int cx = left + x, cy = top + y;
                if (cx < 0 || cy < 0 || cx >= w || cy >= h) continue;
                float g = kWeaponLow + (grey(c) - low) / std::max(high - low, 1.0f) * (kWeaponHigh - kWeaponLow);
                g = (g - 128.0f) * look.contrast + 128.0f + look.brightness;
                if (selected) g *= kSelectedGain;
                cover[cy * w + cx] = qAlpha(c) / 255.0f;
                shade[cy * w + cx] = std::clamp(g, 0.0f, 255.0f);
            }
    }

    // dark outline
    std::vector<float> ring(w * h, 0.0f);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
        {
            float most = 0;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx)
                    if (x + dx >= 0 && y + dy >= 0 && x + dx < w && y + dy < h) most = std::max(most, cover[(y + dy) * w + x + dx]);
            ring[y * w + x] = most;
        }
    // selected halo
    if (selected)
    {
        const std::vector<float> glow = blurred(ring, 2, 3);
        for (int i = 0; i < w * h; ++i) value[i] = std::max(value[i], std::min(glow[i] * 1.6f, 1.0f) * kGlow * (value[i] > 0 ? 1.0f : 0.0f));
    }
    for (int i = 0; i < w * h; ++i)
    {
        value[i] = value[i] + (std::min(value[i], static_cast<float>(kOutline)) - value[i]) * ring[i];
        value[i] = value[i] * (1.0f - cover[i]) + shade[i] * cover[i];
    }

    // the name
    const float shadeOfName = selected ? 255.0f : kLabelGrey;
    for (int i = 0; i < w * h; ++i) value[i] += (shadeOfName - value[i]) * name[i];

    QImage cell(w, h, QImage::Format_RGB32);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
        {
            const int g = std::clamp(static_cast<int>(std::lround(value[y * w + x])), 0, 255);
            cell.setPixel(x, y, qRgb(g, g, g));
        }
    return cell;
}

QImage MakeSheet(const std::vector<QImage>& cells)
{
    QImage sheet(kSheetSize, kSheetSize, QImage::Format_RGB32);
    sheet.fill(Qt::black);
    QPainter painter(&sheet);
    for (size_t i = 0; i < cells.size(); ++i) painter.drawImage(0, static_cast<int>(i) * kCellHeight, cells[i]);
    return sheet;
}

bool WriteSprite(const QImage& sheet, const std::string& path)
{
    QTemporaryDir folder;
    if (!folder.isValid()) return false;
    const QString frame = folder.filePath(QStringLiteral("sheet.png"));
    if (!sheet.save(frame)) return false;
    SpriteLoader sprite;
    return sprite.createSpriteV2(path, { frame.toStdString() }, 2, 1, { 0, 0, 0 });
}

std::vector<bool> UsedCells(const QImage& sheet)
{
    std::vector<bool> used;
    for (int top = 0; top + kCellHeight <= sheet.height(); top += kCellHeight)
    {
        int bright = 0;
        for (int y = top; y < top + kCellHeight; ++y)
            for (int x = 0; x < std::min(kCellWidth, sheet.width()); ++x) bright += grey(sheet.pixel(x, y)) > 60 ? 1 : 0;
        used.push_back(bright > 20);
    }
    return used;
}

QString HudText(const QString& spriteName, int row)
{
    QString text = QStringLiteral("10\n");
    for (const char* resolution : { "320", "640" })
    {
        const QString r = QString::fromLatin1(resolution);
        text += QStringLiteral("weapon\t\t%1 %2\t0\t%3\t%4\t%5\n").arg(r, spriteName).arg(row * kCellHeight).arg(kCellWidth).arg(kCellHeight);
        text += QStringLiteral("weapon_s\t%1 %2\t0\t%3\t%4\t%5\n").arg(r, spriteName).arg((row + 1) * kCellHeight).arg(kCellWidth).arg(kCellHeight);
        text += QStringLiteral("ammo\t\t%1 640hud7\t72\t72\t24\t24\n").arg(r);
        // those are useless ik
        text += QStringLiteral("crosshair\t%1 crosshairs\t24\t0\t24\t24\n").arg(r);
        text += QStringLiteral("autoaim\t\t%1 crosshairs\t0\t72\t24\t24\n").arg(r);
    }
    return text;
}

}  // namespace hudicon
