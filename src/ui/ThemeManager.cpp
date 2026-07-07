#include "ThemeManager.h"

#include <QApplication>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleHints>
#include <QPalette>
#include <QColor>

void ThemeManager::apply(bool dark)
{

    static const QString defaultStyleName = QApplication::style()->objectName();
    static const QPalette defaultPalette = QApplication::palette();
    static bool darkActive = false;

    if (dark)
    {
        darkActive = true;

#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Dark);
#endif

        qApp->setStyle(QStyleFactory::create("Fusion"));

        const QColor window(45, 45, 45);
        const QColor base(30, 30, 30);
        const QColor alternateBase(38, 38, 38);
        const QColor text(220, 220, 220);
        const QColor disabledText(110, 110, 110);
        const QColor button(50, 50, 50);
        const QColor highlight(0, 120, 212);
        const QColor link(86, 156, 214);

        QPalette palette;
        palette.setColor(QPalette::Window, window);
        palette.setColor(QPalette::WindowText, text);
        palette.setColor(QPalette::Base, base);
        palette.setColor(QPalette::AlternateBase, alternateBase);
        palette.setColor(QPalette::ToolTipBase, alternateBase);
        palette.setColor(QPalette::ToolTipText, text);
        palette.setColor(QPalette::Text, text);
        palette.setColor(QPalette::PlaceholderText, disabledText);
        palette.setColor(QPalette::Button, button);
        palette.setColor(QPalette::ButtonText, text);
        palette.setColor(QPalette::BrightText, QColor(255, 85, 85));
        palette.setColor(QPalette::Link, link);
        palette.setColor(QPalette::Highlight, highlight);
        palette.setColor(QPalette::HighlightedText, Qt::white);

        palette.setColor(QPalette::Light, QColor(70, 70, 70));
        palette.setColor(QPalette::Midlight, QColor(60, 60, 60));
        palette.setColor(QPalette::Mid, QColor(50, 50, 50));
        palette.setColor(QPalette::Dark, QColor(25, 25, 25));
        palette.setColor(QPalette::Shadow, QColor(15, 15, 15));

        palette.setColor(QPalette::Disabled, QPalette::WindowText, disabledText);
        palette.setColor(QPalette::Disabled, QPalette::Text, disabledText);
        palette.setColor(QPalette::Disabled, QPalette::ButtonText, disabledText);
        palette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(60, 60, 60));
        palette.setColor(QPalette::Disabled, QPalette::HighlightedText, disabledText);

        qApp->setPalette(palette);

        qApp->setStyleSheet(
            "QToolTip { color: #dcdcdc; background-color: #262626; border: 1px solid #3c3c3c; }"
        );
    }
    else
    {

        if (!darkActive)
            return;

        darkActive = false;

#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
        // Back to following the system color scheme.
        QGuiApplication::styleHints()->unsetColorScheme();
#endif

        qApp->setStyleSheet(QString());
        qApp->setStyle(QStyleFactory::create(defaultStyleName));
        qApp->setPalette(defaultPalette);
    }
}
