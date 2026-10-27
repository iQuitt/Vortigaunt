// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#pragma once

#include <QWidget>

class QAction;
class QHBoxLayout;
class QStackedWidget;
class QTabBar;
class QToolButton;

// Tabs of command groups with large icon buttons, in place of a menu bar
class RibbonBar : public QWidget
{
    Q_OBJECT
public:
    explicit RibbonBar(QWidget* parent = nullptr);

    int addTab(const QString& title);
    QWidget* addGroup(int tab, const QString& title);
    // the action's iconText is the button label, its text the tooltip
    QToolButton* addButton(QWidget* group, QAction* action, bool primary = false);

    // the right end of the tab row
    QHBoxLayout* corner() const { return m_corner; }

    int currentTab() const;
    void setCurrentTab(int tab);

signals:
    void currentTabChanged(int tab);

private:
    QTabBar* m_tabs = nullptr;
    QStackedWidget* m_pages = nullptr;
    QHBoxLayout* m_corner = nullptr;
};
