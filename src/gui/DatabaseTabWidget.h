/*
 * Copyright (C) 2018 KeePassXC Team <team@keepassxc.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 or (at your option)
 * version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef KEEPASSX_DATABASETABWIDGET_H
#define KEEPASSX_DATABASETABWIDGET_H

#include "DatabaseOpenDialog.h"
#include "gui/MessageWidget.h"

#include <QPointer>
#include <QTabWidget>

class Database;
class DatabaseWidget;
class DatabaseWidgetStateSync;
class DatabaseOpenWidget;

class DatabaseTabWidget : public QTabWidget
{
    Q_OBJECT

public:
    explicit DatabaseTabWidget(QWidget* parent = nullptr);
    ~DatabaseTabWidget() override;
    void mergeDatabase(const QString& filePath);

    QString tabName(int index);
    DatabaseWidget* currentDatabaseWidget();
    DatabaseWidget* databaseWidgetFromIndex(int index) const;

    bool isReadOnly(int index = -1) const;
    bool canSave(int index = -1) const;
    bool isModified(int index = -1) const;
    bool hasLockableDatabases() const;

public slots:
    void addDatabaseTab(const QString& filePath,
                        bool inBackground = false,
                        const QString& password = {},
                        const QString& keyfile = {});
    void addDatabaseTab(DatabaseWidget* dbWidget, bool inBackground = false);
    bool closeDatabaseTab(int index);
    bool closeDatabaseTab(DatabaseWidget* dbWidget);
    bool closeAllDatabaseTabs();
    bool closeCurrentDatabaseTab();
    bool closeDatabaseTabFromSender();
    void updateTabName(int index = -1);

    DatabaseWidget* newDatabase();
    void openDatabase();
    void mergeDatabase();
    void importCsv();
    void importKeePass1Database();
    void importOpVaultDatabase();
    bool saveDatabase(int index = -1);
    bool saveDatabaseAs(int index = -1);
    bool saveDatabaseBackup(int index = -1);
    void exportToCsv();
    void exportToHtml();

    bool lockDatabases();
    void closeDatabaseFromSender();
    void unlockDatabaseInDialog(DatabaseWidget* dbWidget, DatabaseOpenDialog::Intent intent);
    void unlockDatabaseInDialog(DatabaseWidget* dbWidget, DatabaseOpenDialog::Intent intent, const QString& filePath);
    void relockPendingDatabase();

    void showDatabaseSecurity();
    void showDatabaseReports();
    void showDatabaseSettings();
    void performGlobalAutoType(const QString& url);
    void performBrowserUnlock();

signals:
    void databaseOpened(DatabaseWidget* dbWidget);
    void databaseClosed(const QString& filePath);
    void databaseUnlocked(DatabaseWidget* dbWidget);
    void databaseLocked(DatabaseWidget* dbWidget);
    void activeDatabaseChanged(DatabaseWidget* dbWidget);
    void tabNameChanged();
    void tabVisibilityChanged(bool tabsVisible);
    void messageGlobal(const QString&, MessageWidget::MessageType type);
    void messageDismissGlobal();
    void databaseUnlockDialogFinished(bool accepted, DatabaseWidget* dbWidget);

private slots:
    void toggleTabbar();
    void emitActiveDatabaseChanged();
    void emitDatabaseLockChanged();

private:
    QSharedPointer<Database> execNewDatabaseWizard();
    void updateLastDatabases(const QString& filename);
    bool warnOnExport();

    QPointer<DatabaseWidgetStateSync> m_dbWidgetStateSync;
    QPointer<DatabaseWidget> m_dbWidgetPendingLock;
    QPointer<DatabaseOpenDialog> m_databaseOpenDialog;
};

#endif // KEEPASSX_DATABASETABWIDGET_H
