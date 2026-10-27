// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#include "RibbonBar.h"

#include <QAction>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QStackedWidget>
#include <QTabBar>
#include <QToolButton>
#include <QVBoxLayout>

RibbonBar::RibbonBar(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* tabRow = new QHBoxLayout();
    tabRow->setContentsMargins(6, 4, 6, 0);
    m_tabs = new QTabBar(this);
    m_tabs->setDrawBase(false);
    m_tabs->setExpanding(false);
    tabRow->addWidget(m_tabs);
    tabRow->addStretch();
    m_corner = new QHBoxLayout();
    m_corner->setSpacing(4);
    tabRow->addLayout(m_corner);
    layout->addLayout(tabRow);

    auto* top = new QFrame(this);
    top->setFrameShape(QFrame::HLine);
    top->setFrameShadow(QFrame::Plain);
    top->setForegroundRole(QPalette::Mid);
    layout->addWidget(top);

    m_pages = new QStackedWidget(this);
    layout->addWidget(m_pages);

    auto* bottom = new QFrame(this);
    bottom->setFrameShape(QFrame::HLine);
    bottom->setFrameShadow(QFrame::Plain);
    bottom->setForegroundRole(QPalette::Mid);
    layout->addWidget(bottom);

    connect(m_tabs, &QTabBar::currentChanged, m_pages, &QStackedWidget::setCurrentIndex);
    connect(m_tabs, &QTabBar::currentChanged, this, &RibbonBar::currentTabChanged);
}

int RibbonBar::currentTab() const
{
    return m_tabs->currentIndex();
}

void RibbonBar::setCurrentTab(int tab)
{
    m_tabs->setCurrentIndex(tab);
}

int RibbonBar::addTab(const QString& title)
{
    auto* page = new QWidget(m_pages);
    auto* row = new QHBoxLayout(page);
    row->setContentsMargins(4, 4, 4, 2);
    row->setSpacing(0);
    row->addStretch();
    m_pages->addWidget(page);
    return m_tabs->addTab(title);
}

QWidget* RibbonBar::addGroup(int tab, const QString& title)
{
    QWidget* page = m_pages->widget(tab);
    auto* row = static_cast<QHBoxLayout*>(page->layout());
    const int stretchAt = row->count() - 1;

    int insertAt = stretchAt;
    if (stretchAt > 0)
    {
        auto* line = new QFrame(page);
        line->setFrameShape(QFrame::VLine);
        line->setFrameShadow(QFrame::Plain);
        line->setForegroundRole(QPalette::Mid);
        row->insertWidget(insertAt++, line);
    }

    auto* group = new QWidget(page);
    auto* column = new QVBoxLayout(group);
    column->setContentsMargins(6, 0, 6, 0);
    column->setSpacing(2);
    auto* buttons = new QHBoxLayout();
    buttons->setSpacing(2);
    column->addLayout(buttons, 1);
    auto* label = new QLabel(title, group);
    label->setAlignment(Qt::AlignCenter);
    label->setForegroundRole(QPalette::PlaceholderText);
    QFont font = label->font();
    font.setPointSizeF(font.pointSizeF() - 1.0);
    label->setFont(font);
    column->addWidget(label);

    row->insertWidget(insertAt, group);
    return group;
}

QToolButton* RibbonBar::addButton(QWidget* group, QAction* action, bool primary)
{
    auto* buttons = static_cast<QHBoxLayout*>(group->layout()->itemAt(0)->layout());
    auto* button = new QToolButton(group);
    button->setDefaultAction(action);
    button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    button->setAutoRaise(true);
    button->setIconSize(QSize(26, 26));
    button->setMinimumWidth(64);
    button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    if (primary)
    {
        button->setStyleSheet("QToolButton { background: palette(highlight); color: palette(highlighted-text);"
                              " border: 1px solid palette(highlight); border-radius: 3px; padding: 2px 6px; }"
                              "QToolButton:hover { border-color: palette(highlighted-text); }"
                              "QToolButton:disabled { background: palette(button); color: palette(mid); border-color: palette(mid); }");
    }
    buttons->addWidget(button);
    return button;
}
