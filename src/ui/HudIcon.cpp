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
// the game's 640hud letters, upright: coverage 0-f in nine rows
struct Letter
{
    char ch;
    float advance;
    const char* rows;
};
constexpr Letter kLetters[] = {
    { '-', 6.5f, "0000000 0000000 0000000 07bbbb4 09ffff6 0244442 0000000 0000000 0000000" },
    { '.', 3.5f, "0000 0000 0000 0000 0000 0000 07b4 09f6 0242" },
    { '0', 8.5f, "007bbbb40 07dffffd4 09f84bff6 09f67dff6 09fdd8bf6 09ff829f6 09ffdbdf6 02bffff82 002444420" },
    { '1', 5.5f, "0007b4 007df6 07dff6 024bf6 0009f6 0009f6 0009f6 0009f6 000242" },
    { '2', 8.5f, "07bbbbb40 09fffffd4 024444bf6 007bbbdf6 07dffff82 09f844420 09fdbbbb4 09ffffff6 024444442" },
    { '3', 8.5f, "07bbbbb40 09fffffd4 024444bf6 0007bbd82 0009fffd4 000244bf6 07bbbbdf6 09fffff82 024444420" },
    { '4', 8.5f, "07b407b40 09f609f60 09f609f60 09fdbdfd4 09ffffff6 02444bf82 000009f60 000009f60 000002420" },
    { '5', 8.5f, "07bbbbbb4 09ffffff6 09f844442 09fdbbb40 02bffffd4 002444bf6 07bbbbdf6 09fffff82 024444420" },
    { '6', 8.5f, "007bbbbb4 07dfffff6 09f844442 09fdbbb40 09fffffd4 09f844bf6 09fdbbdf6 02bffff82 002444420" },
    { '7', 8.5f, "07bbbbbb4 09ffffff6 02444bf82 00007d820 0007d8200 0009f6000 0009f6000 0009f6000 000242000" },
    { '8', 8.5f, "007bbbb40 07dffffd4 09f844bf6 02bdbbd82 07dffffd4 09f844bf6 09fdbbdf6 02bffff82 002444420" },
    { '9', 8.5f, "007bbbb40 07dffffd4 09f844bf6 09fdbbdf6 02bfffff6 002444bf6 07bbbbdf6 09fffff82 024444420" },
    { 'A', 8.33f, "008ba300 01cff810 05cffb40 04fffd60 08eebf80 09feeeb3 2affffe8 0ef808fb 03420133" },
    { 'B', 8.5f, "07bbbbb40 09fffffd4 024444bf6 07b4009f6 09fdbbd82 09f844bd4 09fdbbdf6 09fffff82 024444420" },
    { 'C', 9.0f, "02abbbbb5 17efffff7 06ea20000 04fc00000 03ec10000 05cc40000 13cea8886 20affffff 100344444" },
    { 'D', 10.0f, "06bbbbb80 27efffff3 3375029f7 24fc018e9 13ec108e8 45cc407d9 53cea8afb 50dfffffb 203444441" },
    { 'E', 8.5f, "4bbbbbb5 5dfffff8 13300000 2dea7300 0dfff500 2be84200 29eb8886 09ffffff 02344444" },
    { 'F', 8.5f, "07bbbbbb4 09ffffff6 024444442 07bbb4000 09fff6000 09f842000 09f600000 09f600000 024200000" },
    { 'G', 8.0f, "07bbbbb5 3dfffff8 3bb30000 29d66664 0ef9eff9 2ab535ca 28d867bc 05ffffff 00344444" },
    { 'H', 8.5f, "07b4007b4 09f6009f6 09f6009f6 09fdbbdf6 09ffffff6 09f844bf6 09f6009f6 09f6009f6 024200242" },
    { 'I', 5.0f, "4b8 6ea 6da 3ec 3ee 3bd 3ad 0bf 023" },
    { 'J', 8.5f, "0000007b4 0000009f6 0000009f6 0000009f6 0000009f6 07b4009f6 09fdbbdf6 02bffff82 002444420" },
    { 'K', 8.5f, "07b4007b4 09f607d82 09f67d820 09fdd8200 09fff6000 09f8bd400 09f62bd40 09f602bd4 024200242" },
    { 'L', 7.0f, "1a80000 3dc2000 4bb3000 2cd3000 0df2000 2ae5000 29eb863 09ffff7 0234442" },
    { 'M', 9.5f, "ba2009b80 df923dfb2 cfc56efb3 dfea8ffd3 dffedfff2 beeffefd5 9dbffecd7 9f7efbaf7 232232332" },
    { 'N', 8.5f, "1bb405b80 1cfb26da2 2aff86da3 2affd8ec2 0afefefe1 09facffd4 06e94efd5 06fa09ff5 013200342" },
    { 'O', 10.0f, "05bbbbb80 2cffffff3 2ae6029e7 2ad6018e8 0af6008e7 09f7008e8 26fd88afb 03ffffffa 102344441" },
    { 'P', 8.0f, "4bbbbb92 5dfffff6 266208f9 2dd648fc 0efffffb 2bea8873 29d70000 09f70000 02320000" },
    { 'Q', 8.5f, "007bbbb40 07dffffd4 09f844bf6 09f6009f6 09f6009f6 09f67bdf6 09fddfff6 02bfffff6 002444442" },
    { 'R', 9.25f, "09bbbbb60 0bfffffc2 048404bd5 07ea45bd6 07efffff3 07dc89df6 04eb117e9 02fe007f9 003300132" },
    { 'S', 9.0f, "05bbbbba0 1cfffffc2 2ae600000 29febba50 04dfffff1 000002be6 048888ce7 06ffffff5 013444420" },
    { 'T', 8.5f, "07bbbbbb4 09ffffff6 024bf8442 0009f6000 0009f6000 0009f6000 0009f6000 0009f6000 000242000" },
    { 'U', 8.5f, "07b4007b4 09f6009f6 09f6009f6 09f6009f6 09f6009f6 09f6009f6 09fdbbdf6 02bffff82 002444420" },
    { 'V', 8.5f, "07b4007b4 09f6009f6 09f6009f6 09f6009f6 02bd47d82 009f69f60 002bdd820 0009ff600 000244200" },
    { 'W', 10.5f, "07b400007b4 09f600009f6 09f600009f6 09f67b409f6 09f69f609f6 09fddfdbdf6 09ffffffff6 02bf84bff82 00242024420" },
    { 'X', 8.5f, "07b4007b4 02bd47d82 002bdd820 0009ff600 0009ff600 007d8bd40 07d822bd4 09f6009f6 024200242" },
    { 'Y', 8.0f, "09b72ab 05debed 019fff9 003efd6 0008ec1 0007d92 0004eb2 0002fe0 0000330" },
    { 'Z', 8.5f, "07bbbbbb4 09ffffff6 02444bf82 00007d820 0007d8200 007d82000 07dfdbbb4 09ffffff6 024444442" },
};
constexpr float kSpace = 5.0f;
constexpr float kLean = 0.3f;       // italic lean per row
constexpr int kNameLeft = 10, kNameTop = 33, kNameRows = 9, kNameBaseRow = 7;

// the name's coverage and its box
std::vector<float> nameCover(const QString& label, QRect& box)
{
    const int w = hudicon::kCellWidth;
    std::vector<float> upright(kNameRows * w, 0.0f);
    float pen = kNameLeft;
    for (const QChar c : label)
    {
        const char ch = c.toUpper().toLatin1();
        const Letter* letter = std::find_if(std::begin(kLetters), std::end(kLetters), [ch](const Letter& l) { return l.ch == ch; });
        if (letter == std::end(kLetters))
        {
            pen += kSpace;
            continue;
        }
        const int left = static_cast<int>(std::lround(pen)) - 1;
        int row = 0, column = 0;
        for (const char* p = letter->rows; *p; ++p)
        {
            if (*p == ' ')
            {
                ++row;
                column = 0;
                continue;
            }
            const int x = left + column++;
            const float v = (*p <= '9' ? *p - '0' : *p - 'a' + 10) / 15.0f;
            if (x >= 0 && x < w) upright[row * w + x] = std::max(upright[row * w + x], v);
        }
        pen += letter->advance;
    }
    std::vector<float> cover(w * hudicon::kCellHeight, 0.0f);
    box = QRect();
    for (int row = 0; row < kNameRows; ++row)
    {
        const float shift = (kNameBaseRow - row) * kLean;
        for (int x = 0; x < w; ++x)
        {
            const float u = x - shift;
            const int u0 = static_cast<int>(std::floor(u));
            const float f = u - u0;
            auto at = [&](int i) { return i >= 0 && i < w ? upright[row * w + i] : 0.0f; };
            const float v = at(u0) * (1 - f) + at(u0 + 1) * f;
            if (v <= 0) continue;
            cover[(kNameTop + row) * w + x] = v;
            box |= QRect(x, kNameTop + row, 1, 1);
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
