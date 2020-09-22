/*
 *  Copyright (C) 2018 KeePassXC Team <team@keepassxc.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 or (at your option)
 *  version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "DatabaseOpenDialog.h"
#include "DatabaseOpenWidget.h"
#include "DatabaseTabWidget.h"
#include "DatabaseWidget.h"
#include "core/Database.h"

#include <QFileInfo>
#include <QShortcut>

#ifdef Q_OS_WIN
#include <QtPlatformHeaders/QWindowsWindowFunctions>
#endif

DatabaseOpenDialog::DatabaseOpenDialog(QWidget* parent)
    : QDialog(parent)
    , m_view(new DatabaseOpenWidget(this))
    , m_tabBar(new QTabBar(this))
{
    setWindowTitle(tr("Unlock Database - KeePassXC"));
    setWindowFlags(Qt::Dialog | Qt::WindowStaysOnTopHint);
    // block input to the main window/application while the dialog is open
    setWindowModality(Qt::ApplicationModal);
#ifdef Q_OS_WIN
    QWindowsWindowFunctions::setWindowActivationBehavior(QWindowsWindowFunctions::AlwaysActivateWindow);
#endif
    connect(m_view, &DatabaseOpenWidget::dialogFinished, this, &DatabaseOpenDialog::complete);

    m_tabBar->setAutoHide(true);
    m_tabBar->setExpanding(false);
    connect(m_tabBar, &QTabBar::currentChanged, this, &DatabaseOpenDialog::tabChanged);

    auto* layout = new QVBoxLayout();
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_tabBar);
    layout->addWidget(m_view);
    setLayout(layout);
    setMinimumWidth(700);

    // set up Ctrl+PageUp and Ctrl+PageDown shortcuts to cycle tabs
    auto* shortcut = new QShortcut(Qt::CTRL + Qt::Key_PageUp, this);
    shortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(shortcut, &QShortcut::activated, this, [this]() { selectTabOffset(-1); });
    shortcut = new QShortcut(Qt::CTRL + Qt::Key_PageDown, this);
    shortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(shortcut, &QShortcut::activated, this, [this]() { selectTabOffset(1); });
}

void DatabaseOpenDialog::selectTabOffset(int offset)
{
    if (offset == 0 || m_tabBar->count() <= 1) {
        return;
    }
    int tab = m_tabBar->currentIndex() + offset;
    int last = m_tabBar->count() - 1;
    if (tab < 0) {
        tab = last;
    } else if (tab > last) {
        tab = 0;
    }
    m_tabBar->setCurrentIndex(tab);
}

void DatabaseOpenDialog::addDatabaseTab(DatabaseWidget* dbWidget)
{
    Q_ASSERT(dbWidget);
    if (!dbWidget) {
        return;
    }

    // important - we must add the DB widget first, because addTab will fire
    // tabChanged immediately which will look for a dbWidget in the list
    m_tabDbWidgets.append(dbWidget);
    QFileInfo fileInfo(dbWidget->database()->filePath());
    m_tabBar->addTab(fileInfo.fileName());
    Q_ASSERT(m_tabDbWidgets.count() == m_tabBar->count());
}

void DatabaseOpenDialog::setActiveDatabaseTab(DatabaseWidget* dbWidget)
{
    if (!dbWidget) {
        return;
    }
    int index = m_tabDbWidgets.indexOf(dbWidget);
    if (index != -1) {
        m_tabBar->setCurrentIndex(index);
    }
}

void DatabaseOpenDialog::tabChanged(int index)
{
    if (index < 0 || index >= m_tabDbWidgets.count()) {
        return;
    }

    if (m_tabDbWidgets.count() == m_tabBar->count()) {
        DatabaseWidget* dbWidget = m_tabDbWidgets[index];
        setTarget(dbWidget, dbWidget->database()->filePath());
    } else {
        // if these list sizes don't match, there's a bug somewhere nearby
        qWarning("DatabaseOpenDialog: mismatch between tab count %d and DB count %d",
                 m_tabBar->count(),
                 m_tabDbWidgets.count());
    }
}

/**
 * Sets the target DB and reloads the UI.
 */
void DatabaseOpenDialog::setTarget(DatabaseWidget* dbWidget, const QString& filePath)
{
    // reconnect finished signal to new dbWidget, then reload the UI
    if (m_currentDbWidget) {
        disconnect(this, &DatabaseOpenDialog::dialogFinished, m_currentDbWidget, nullptr);
    }
    connect(this, &DatabaseOpenDialog::dialogFinished, dbWidget, &DatabaseWidget::unlockDatabase);

    m_currentDbWidget = dbWidget;
    m_view->load(filePath);
}

void DatabaseOpenDialog::setIntent(DatabaseOpenDialog::Intent intent)
{
    m_intent = intent;
}

DatabaseOpenDialog::Intent DatabaseOpenDialog::intent() const
{
    return m_intent;
}

void DatabaseOpenDialog::clearForms()
{
    m_view->clearForms();
    m_db.reset();
    m_intent = Intent::None;
    if (m_currentDbWidget) {
        disconnect(this, &DatabaseOpenDialog::dialogFinished, m_currentDbWidget, nullptr);
    }
    m_currentDbWidget.clear();
    m_tabDbWidgets.clear();

    // block signals while removing tabs so that tabChanged doesn't get called
    m_tabBar->blockSignals(true);
    while (m_tabBar->count() > 0) {
        m_tabBar->removeTab(0);
    }
    m_tabBar->blockSignals(false);
}

QSharedPointer<Database> DatabaseOpenDialog::database() const
{
    return m_db;
}

void DatabaseOpenDialog::complete(bool accepted)
{
    // save DB, since DatabaseOpenWidget will reset its data after accept() is called
    m_db = m_view->database();

    if (accepted) {
        accept();
    } else {
        reject();
    }

    if (m_intent != Intent::Merge) {
        // Update the current database in the main UI to match what we just unlocked
        auto* tabWidget = qobject_cast<DatabaseTabWidget*>(parentWidget());
        tabWidget->setCurrentIndex(tabWidget->indexOf(m_currentDbWidget));
    }

    emit dialogFinished(accepted, m_currentDbWidget);
    clearForms();
}
