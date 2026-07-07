#include "AboutDialog.h"
#include <VortigauntVersion.h>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTabWidget>
#include <QApplication>
#include <QIcon>
#include <QFont>
#include <QTextBrowser>

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("About Vortigaunt"));
    setFixedSize(520, 480);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Header
    auto* header = new QWidget();
    header->setStyleSheet("background: palette(window);");
    auto* headerLayout = new QVBoxLayout();
    headerLayout->setContentsMargins(24, 24, 24, 16);
    headerLayout->setSpacing(8);

    auto* iconLabel = new QLabel();
    iconLabel->setPixmap(QApplication::windowIcon().pixmap(64, 64));
    iconLabel->setAlignment(Qt::AlignCenter);
    headerLayout->addWidget(iconLabel);

    auto* titleLabel = new QLabel("Vortigaunt");
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(18);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);
    headerLayout->addWidget(titleLabel);

    auto* descLabel = new QLabel(tr("A Porting Tool for GoldSrc Engine"));
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setStyleSheet("color: palette(placeholderText); font-size: 12px;");
    headerLayout->addWidget(descLabel);

    header->setLayout(headerLayout);
    mainLayout->addWidget(header);

    // Tab widget for details
    auto* tabs = new QTabWidget();
    tabs->setContentsMargins(12, 8, 12, 0);

    // --- Info tab ---
    auto* infoTab = new QWidget();
    auto* infoLayout = new QVBoxLayout(infoTab);
    infoLayout->setSpacing(6);

    auto addInfo = [&](const QString& label, const QString& value) {
        auto* row = new QHBoxLayout();
        auto* lbl = new QLabel(label);
        lbl->setStyleSheet("font-weight: bold;");
        lbl->setFixedWidth(90);
        auto* val = new QLabel(value);
        val->setTextInteractionFlags(Qt::TextSelectableByMouse);
        row->addWidget(lbl);
        row->addWidget(val, 1);
        infoLayout->addLayout(row);
    };

    addInfo(tr("Build:"), VORTIGAUNT_BUILD_TYPE);
    addInfo(tr("Branch:"), VORTIGAUNT_GIT_BRANCH);
    addInfo(tr("Qt:"), QT_VERSION_STR);
    addInfo(tr("Built:"), QString("%1 %2").arg(__DATE__).arg(__TIME__));

    infoLayout->addStretch();
    tabs->addTab(infoTab, tr("Info"));

    // --- Credits tab ---
    auto* creditsBrowser = new QTextBrowser();
    creditsBrowser->setOpenExternalLinks(true);
    creditsBrowser->setStyleSheet("QTextBrowser { border: none; }");
    creditsBrowser->setHtml(QString(
        "<h3>%1</h3>"
        "<ul>"
        "<li>Granny 3D SDK &mdash; 1999&ndash;2017 by RAD Game Tools, Inc.</li>"
        "<li>assimp &mdash; Copyright 2006&ndash;2026, assimp team</li>"
        "<li>dr_wav &mdash; Copyright 2017&ndash;2026, David Reid</li>"
        "<li>dr_mp3 &mdash; Copyright 2017&ndash;2026, David Reid</li>"
        "<li>Discord Social SDK &mdash; Copyright 2015&ndash;2026, Discord Inc.</li>"
        "<li>libimagequant &mdash; &copy; 2009&ndash;2018 by Kornel Lesiński</li>"
        "<li>Lithtech SDK &mdash; Copyright 1998&ndash;2005, Monolith Productions, Inc.</li>"
        "<li>Lzma SDK &mdash; Copyright 1999&ndash;2026, Igor Pavlov</li>"
        "</ul>"
        "<h3>%2</h3>"
        "<ul>"
        "<li>Lu&iacute;s Leite for Counter Strike Online PAK</li>"
        "<li>Kungfulon for Crossfire REZ</li>"
        "<li>Luigi Auriemma for XFS logic</li>"
        "<li>YoungFine0825 for LTB2FBX and DTX to TGA</li>"
        "<li>Facepunch for GMA (Garry's Mod Addon)"
        "</ul>"
    ).arg(tr("3rd Party Libraries"), tr("Special Thanks")));
    tabs->addTab(creditsBrowser, tr("Credits"));

    mainLayout->addWidget(tabs, 1);
}
