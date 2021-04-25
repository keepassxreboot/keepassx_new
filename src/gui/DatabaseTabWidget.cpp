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

#include "DatabaseTabWidget.h"

#include <QFileInfo>
#include <QPushButton>
#include <QTabWidget>

#include "autotype/AutoType.h"
#include "core/AsyncTask.h"
#include "core/Config.h"
#include "core/Database.h"
#include "core/Global.h"
#include "core/Group.h"
#include "core/Metadata.h"
#include "core/Tools.h"
#include "format/CsvExporter.h"
#include "gui/Clipboard.h"
#include "gui/DatabaseOpenDialog.h"
#include "gui/DatabaseWidget.h"
#include "gui/DatabaseWidgetStateSync.h"
#include "gui/DragTabBar.h"
#include "gui/FileDialog.h"
#include "gui/HtmlExporter.h"
#include "gui/MessageBox.h"
#include "gui/entry/EntryView.h"
#include "gui/group/GroupView.h"
#ifdef Q_OS_MACOS
#include "gui/osutils/macutils/MacUtils.h"
#endif
#include "gui/wizard/NewDatabaseWizard.h"

DatabaseTabWidget::DatabaseTabWidget(QWidget* parent)
    : QTabWidget(parent)
    , m_dbWidgetStateSync(new DatabaseWidgetStateSync(this))
    , m_dbWidgetPendingLock(nullptr)
    , m_databaseOpenDialog(new DatabaseOpenDialog(this))
{
    auto* tabBar = new DragTabBar(this);
    setTabBar(tabBar);
    setDocumentMode(true);

    // clang-format off
    connect(this, SIGNAL(tabCloseRequested(int)), SLOT(closeDatabaseTab(int)));
    connect(this, SIGNAL(currentChanged(int)), SLOT(emitActiveDatabaseChanged()));
    connect(this, SIGNAL(activeDatabaseChanged(DatabaseWidget*)),
            m_dbWidgetStateSync, SLOT(setActive(DatabaseWidget*)));
    connect(autoType(), SIGNAL(globalAutoTypeTriggered()), SLOT(performGlobalAutoType()));
    connect(autoType(), SIGNAL(autotypePerformed()), SLOT(relockPendingDatabase()));
    connect(autoType(), SIGNAL(autotypeRejected()), SLOT(relockPendingDatabase()));
    connect(m_databaseOpenDialog.data(), &DatabaseOpenDialog::dialogFinished,
            this, &DatabaseTabWidget::databaseUnlockDialogFinished);
    // clang-format on

#ifdef Q_OS_MACOS
    connect(macUtils(), SIGNAL(lockDatabases()), SLOT(lockDatabases()));
#endif
}

DatabaseTabWidget::~DatabaseTabWidget()
{
}

void DatabaseTabWidget::toggleTabbar()
{
    if (count() > 1) {
        tabBar()->show();
        emit tabVisibilityChanged(true);
    } else {
        tabBar()->hide();
        emit tabVisibilityChanged(false);
    }
}

/**
 * Helper method for invoking the new database wizard.
 * The user of this method MUST take ownership of the returned pointer.
 *
 * @return pointer to the configured new database, nullptr on failure
 */
QSharedPointer<Database> DatabaseTabWidget::execNewDatabaseWizard()
{
    // use QScopedPointer to ensure deletion after scope ends, but still parent
    // it to this to make it modal and allow easier access in unit tests
    QScopedPointer<NewDatabaseWizard> wizard(new NewDatabaseWizard(this));
    if (!wizard->exec()) {
        return {};
    }

    auto db = wizard->takeDatabase();
    if (!db) {
        return {};
    }
    Q_ASSERT(db->key());
    Q_ASSERT(db->kdf());
    if (!db->key() || !db->kdf()) {
        MessageBox::critical(this,
                             tr("Database creation error"),
                             tr("The created database has no key or KDF, refusing to save it.\n"
                                "This is definitely a bug, please report it to the developers."),
                             MessageBox::Ok,
                             MessageBox::Ok);
        return {};
    }

    return db;
}

DatabaseWidget* DatabaseTabWidget::newDatabase()
{
    auto db = execNewDatabaseWizard();
    if (!db) {
        return nullptr;
    }

    auto dbWidget = new DatabaseWidget(db, this);
    addDatabaseTab(dbWidget);
    db->markAsModified();
    return dbWidget;
}

void DatabaseTabWidget::openDatabase()
{
    QString filter = QString("%1 (*.kdbx);;%2 (*)").arg(tr("KeePass 2 Database"), tr("All files"));
    QString fileName = fileDialog()->getOpenFileName(this, tr("Open database"), "", filter);
    if (!fileName.isEmpty()) {
        addDatabaseTab(fileName);
    }
}

/**
 * Add a new database tab or switch to an existing one if the
 * database has been opened already.
 *
 * @param filePath database file path
 * @param inBackground optional, don't focus tab after opening
 * @param password optional, password to unlock database
 * @param keyfile optional, path to keyfile to unlock database
 *
 */
void DatabaseTabWidget::addDatabaseTab(const QString& filePath,
                                       bool inBackground,
                                       const QString& password,
                                       const QString& keyfile)
{
    QFileInfo fileInfo(filePath);
    QString canonicalFilePath = fileInfo.canonicalFilePath();

    if (canonicalFilePath.isEmpty()) {
        emit messageGlobal(tr("Failed to open %1. It either does not exist or is not accessible.").arg(filePath),
                           MessageWidget::Error);
        return;
    }

    for (int i = 0, c = count(); i < c; ++i) {
        auto* dbWidget = databaseWidgetFromIndex(i);
        Q_ASSERT(dbWidget);
        if (dbWidget
            && dbWidget->database()->canonicalFilePath().compare(canonicalFilePath, FILE_CASE_SENSITIVE) == 0) {
            dbWidget->performUnlockDatabase(password, keyfile);
            if (!inBackground) {
                // switch to existing tab if file is already open
                setCurrentIndex(indexOf(dbWidget));
            }
            return;
        }
    }

    auto* dbWidget = new DatabaseWidget(QSharedPointer<Database>::create(filePath), this);
    addDatabaseTab(dbWidget, inBackground);
    dbWidget->performUnlockDatabase(password, keyfile);
    updateLastDatabases(filePath);
}

/**
 * Add a new database tab containing the given DatabaseWidget
 * @param filePath
 * @param inBackground optional, don't focus tab after opening
 */
void DatabaseTabWidget::addDatabaseTab(DatabaseWidget* dbWidget, bool inBackground)
{
    Q_ASSERT(dbWidget->database());

    // emit before index change
    emit databaseOpened(dbWidget);

    int index = addTab(dbWidget, "");
    updateTabName(index);
    toggleTabbar();
    if (!inBackground) {
        setCurrentIndex(index);
    }

    connect(dbWidget,
            SIGNAL(requestOpenDatabase(QString, bool, QString, QString)),
            SLOT(addDatabaseTab(QString, bool, QString, QString)));
    connect(dbWidget, SIGNAL(databaseFilePathChanged(QString, QString)), SLOT(updateTabName()));
    connect(dbWidget, SIGNAL(closeRequest()), SLOT(closeDatabaseTabFromSender()));
    connect(dbWidget,
            SIGNAL(databaseReplaced(const QSharedPointer<Database>&, const QSharedPointer<Database>&)),
            SLOT(updateTabName()));
    connect(dbWidget, SIGNAL(databaseModified()), SLOT(updateTabName()));
    connect(dbWidget, SIGNAL(databaseSaved()), SLOT(updateTabName()));
    connect(dbWidget, SIGNAL(databaseUnlocked()), SLOT(updateTabName()));
    connect(dbWidget, SIGNAL(databaseUnlocked()), SLOT(emitDatabaseLockChanged()));
    connect(dbWidget, SIGNAL(databaseLocked()), SLOT(updateTabName()));
    connect(dbWidget, SIGNAL(databaseLocked()), SLOT(emitDatabaseLockChanged()));
}

void DatabaseTabWidget::importCsv()
{
    QString filter = QString("%1 (*.csv);;%2 (*)").arg(tr("CSV file"), tr("All files"));
    QString fileName = fileDialog()->getOpenFileName(this, tr("Select CSV file"), "", filter);

    if (fileName.isEmpty()) {
        return;
    }

    auto db = execNewDatabaseWizard();
    if (!db) {
        return;
    }

    auto* dbWidget = new DatabaseWidget(db, this);
    addDatabaseTab(dbWidget);
    dbWidget->switchToCsvImport(fileName);
}

void DatabaseTabWidget::mergeDatabase()
{
    auto dbWidget = currentDatabaseWidget();
    if (dbWidget && !dbWidget->isLocked()) {
        QString filter = QString("%1 (*.kdbx);;%2 (*)").arg(tr("KeePass 2 Database"), tr("All files"));
        const QString fileName = fileDialog()->getOpenFileName(this, tr("Merge database"), QString(), filter);
        if (!fileName.isEmpty()) {
            mergeDatabase(fileName);
        }
    }
}

void DatabaseTabWidget::mergeDatabase(const QString& filePath)
{
    unlockDatabaseInDialog(currentDatabaseWidget(), DatabaseOpenDialog::Intent::Merge, filePath);
}

void DatabaseTabWidget::importKeePass1Database()
{
    QString filter = QString("%1 (*.kdb);;%2 (*)").arg(tr("KeePass 1 database"), tr("All files"));
    QString fileName = fileDialog()->getOpenFileName(this, tr("Open KeePass 1 database"), QString(), filter);

    if (fileName.isEmpty()) {
        return;
    }

    auto db = QSharedPointer<Database>::create();
    auto* dbWidget = new DatabaseWidget(db, this);
    addDatabaseTab(dbWidget);
    dbWidget->switchToImportKeepass1(fileName);
}

void DatabaseTabWidget::importOpVaultDatabase()
{
#ifdef Q_OS_MACOS
    QString fileName = fileDialog()->getOpenFileName(this, tr("Open OPVault"), {}, "OPVault (*.opvault)");
#else
    QString fileName = fileDialog()->getExistingDirectory(this, tr("Open OPVault"));
#endif

    if (fileName.isEmpty()) {
        return;
    }

    auto db = QSharedPointer<Database>::create();
    auto* dbWidget = new DatabaseWidget(db, this);
    addDatabaseTab(dbWidget);
    dbWidget->switchToImportOpVault(fileName);
}

/**
 * Attempt to close the current database and remove its tab afterwards.
 *
 * @param index index of the database tab to close
 * @return true if database was closed successully
 */
bool DatabaseTabWidget::closeCurrentDatabaseTab()
{
    return closeDatabaseTab(currentIndex());
}

/**
 * Attempt to close the database tab that sent the close request.
 *
 * @param index index of the database tab to close
 * @return true if database was closed successully
 */
bool DatabaseTabWidget::closeDatabaseTabFromSender()
{
    return closeDatabaseTab(qobject_cast<DatabaseWidget*>(sender()));
}

/**
 * Attempt to close a database and remove its tab afterwards.
 *
 * @param index index of the database tab to close
 * @return true if database was closed successully
 */
bool DatabaseTabWidget::closeDatabaseTab(int index)
{
    return closeDatabaseTab(qobject_cast<DatabaseWidget*>(widget(index)));
}

/**
 * Attempt to close a database and remove its tab afterwards.
 *
 * @param dbWidget \link DatabaseWidget to close
 * @return true if database was closed successully
 */
bool DatabaseTabWidget::closeDatabaseTab(DatabaseWidget* dbWidget)
{
    int tabIndex = indexOf(dbWidget);
    if (!dbWidget || tabIndex < 0) {
        return false;
    }

    QString filePath = dbWidget->database()->filePath();
    if (!dbWidget->close()) {
        return false;
    }

    removeTab(tabIndex);
    dbWidget->deleteLater();
    toggleTabbar();
    emit databaseClosed(filePath);
    return true;
}

/**
 * Attempt to close all opened databases.
 * The attempt will be aborted with the first database that cannot be closed.
 *
 * @return true if all databases could be closed.
 */
bool DatabaseTabWidget::closeAllDatabaseTabs()
{
    // Attempt to lock all databases first to prevent closing only a portion of tabs
    if (lockDatabases()) {
        while (count() > 0) {
            if (!closeDatabaseTab(0)) {
                return false;
            }
        }
        return true;
    }

    return false;
}

bool DatabaseTabWidget::saveDatabase(int index)
{
    if (index == -1) {
        index = currentIndex();
    }

    return databaseWidgetFromIndex(index)->save();
}

bool DatabaseTabWidget::saveDatabaseAs(int index)
{
    if (index == -1) {
        index = currentIndex();
    }

    auto* dbWidget = databaseWidgetFromIndex(index);
    bool ok = dbWidget->saveAs();
    if (ok) {
        updateLastDatabases(dbWidget->database()->filePath());
    }
    return ok;
}

bool DatabaseTabWidget::saveDatabaseBackup(int index)
{
    if (index == -1) {
        index = currentIndex();
    }

    auto* dbWidget = databaseWidgetFromIndex(index);
    bool ok = dbWidget->saveBackup();
    if (ok) {
        updateLastDatabases(dbWidget->database()->filePath());
    }
    return ok;
}

void DatabaseTabWidget::closeDatabaseFromSender()
{
    auto* dbWidget = qobject_cast<DatabaseWidget*>(sender());
    Q_ASSERT(dbWidget);
    closeDatabaseTab(dbWidget);
}

void DatabaseTabWidget::exportToCsv()
{
    auto db = databaseWidgetFromIndex(currentIndex())->database();
    if (!db) {
        Q_ASSERT(false);
        return;
    }

    if (!warnOnExport()) {
        return;
    }

    const QString fileName = fileDialog()->getSaveFileName(
        this, tr("Export database to CSV file"), QString(), tr("CSV file").append(" (*.csv)"), nullptr, nullptr);
    if (fileName.isEmpty()) {
        return;
    }

    CsvExporter csvExporter;
    if (!csvExporter.exportDatabase(fileName, db)) {
        emit messageGlobal(tr("Writing the CSV file failed.").append("\n").append(csvExporter.errorString()),
                           MessageWidget::Error);
    }
}

void DatabaseTabWidget::exportToHtml()
{
    auto db = databaseWidgetFromIndex(currentIndex())->database();
    if (!db) {
        Q_ASSERT(false);
        return;
    }

    if (!warnOnExport()) {
        return;
    }

    const QString fileName = fileDialog()->getSaveFileName(
        this, tr("Export database to HTML file"), QString(), tr("HTML file").append(" (*.html)"), nullptr, nullptr);
    if (fileName.isEmpty()) {
        return;
    }

    HtmlExporter htmlExporter;
    if (!htmlExporter.exportDatabase(fileName, db)) {
        emit messageGlobal(tr("Writing the HTML file failed.").append("\n").append(htmlExporter.errorString()),
                           MessageWidget::Error);
    }
}

bool DatabaseTabWidget::warnOnExport()
{
    auto ans =
        MessageBox::question(this,
                             tr("Export Confirmation"),
                             tr("You are about to export your database to an unencrypted file. This will leave your "
                                "passwords and sensitive information vulnerable! Are you sure you want to continue?"),
                             MessageBox::Yes | MessageBox::No,
                             MessageBox::No);
    return ans == MessageBox::Yes;
}

void DatabaseTabWidget::showDatabaseSecurity()
{
    currentDatabaseWidget()->switchToDatabaseSecurity();
}

void DatabaseTabWidget::showDatabaseReports()
{
    currentDatabaseWidget()->switchToDatabaseReports();
}

void DatabaseTabWidget::showDatabaseSettings()
{
    currentDatabaseWidget()->switchToDatabaseSettings();
}

bool DatabaseTabWidget::isReadOnly(int index) const
{
    if (count() == 0) {
        return false;
    }

    if (index == -1) {
        index = currentIndex();
    }

    auto db = databaseWidgetFromIndex(index)->database();
    return db && db->isReadOnly();
}

bool DatabaseTabWidget::isModified(int index) const
{
    if (count() == 0) {
        return false;
    }

    if (index == -1) {
        index = currentIndex();
    }

    auto db = databaseWidgetFromIndex(index)->database();
    return db && db->isModified();
}

bool DatabaseTabWidget::canSave(int index) const
{
    return !isReadOnly(index) && isModified(index);
}

bool DatabaseTabWidget::hasLockableDatabases() const
{
    for (int i = 0, c = count(); i < c; ++i) {
        if (!databaseWidgetFromIndex(i)->isLocked()) {
            return true;
        }
    }
    return false;
}

/**
 * Get the tab's (original) display name without platform-specific
 * mangling that may occur when reading back the actual widget's \link tabText()
 *
 * @param index tab index
 * @return tab name
 */
QString DatabaseTabWidget::tabName(int index)
{
    if (index == -1 || index > count()) {
        return "";
    }

    auto* dbWidget = databaseWidgetFromIndex(index);

    auto db = dbWidget->database();
    Q_ASSERT(db);
    if (!db) {
        return "";
    }

    QString tabName;

    if (!db->filePath().isEmpty()) {
        QFileInfo fileInfo(db->filePath());

        if (db->metadata()->name().isEmpty()) {
            tabName = fileInfo.fileName();
        } else {
            tabName = db->metadata()->name();
        }

        setTabToolTip(index, fileInfo.absoluteFilePath());
    } else {
        if (db->metadata()->name().isEmpty()) {
            tabName = tr("New Database");
        } else {
            tabName = tr("%1 [New Database]", "Database tab name modifier").arg(db->metadata()->name());
        }
    }

    if (dbWidget->isLocked()) {
        tabName = tr("%1 [Locked]", "Database tab name modifier").arg(tabName);
    }

    if (db->isReadOnly()) {
        tabName = tr("%1 [Read-only]", "Database tab name modifier").arg(tabName);
    }

    if (db->isModified()) {
        tabName.append("*");
    }

    return tabName;
}

/**
 * Update of the given tab index or of the sending
 * DatabaseWidget if `index` == -1.
 */
void DatabaseTabWidget::updateTabName(int index)
{
    auto* dbWidget = databaseWidgetFromIndex(index);
    if (!dbWidget) {
        dbWidget = qobject_cast<DatabaseWidget*>(sender());
    }
    Q_ASSERT(dbWidget);
    if (!dbWidget) {
        return;
    }
    index = indexOf(dbWidget);
    setTabText(index, tabName(index));
    emit tabNameChanged();
}

DatabaseWidget* DatabaseTabWidget::databaseWidgetFromIndex(int index) const
{
    return qobject_cast<DatabaseWidget*>(widget(index));
}

DatabaseWidget* DatabaseTabWidget::currentDatabaseWidget()
{
    return qobject_cast<DatabaseWidget*>(currentWidget());
}

/**
 * Attempt to lock all open databases
 *
 * @return return true if all databases are locked
 */
bool DatabaseTabWidget::lockDatabases()
{
    int numLocked = 0;
    int c = count();
    for (int i = 0; i < c; ++i) {
        auto dbWidget = databaseWidgetFromIndex(i);
        if (dbWidget->lock()) {
            ++numLocked;
            if (dbWidget->database()->filePath().isEmpty()) {
                // If we locked a database without a file close the tab
                closeDatabaseTab(dbWidget);
            }
        }
    }

    return numLocked == c;
}

/**
 * Unlock a database with an unlock popup dialog.
 *
 * @param dbWidget DatabaseWidget which to connect signals to
 * @param intent intent for unlocking
 */
void DatabaseTabWidget::unlockDatabaseInDialog(DatabaseWidget* dbWidget, DatabaseOpenDialog::Intent intent)
{
    unlockDatabaseInDialog(dbWidget, intent, dbWidget->database()->filePath());
}

/**
 * Unlock a database with an unlock popup dialog.
 *
 * @param dbWidget DatabaseWidget which to connect signals to
 * @param intent intent for unlocking
 * @param file path of the database to be unlocked
 */
void DatabaseTabWidget::unlockDatabaseInDialog(DatabaseWidget* dbWidget,
                                               DatabaseOpenDialog::Intent intent,
                                               const QString& filePath)
{
    m_databaseOpenDialog->setTargetDatabaseWidget(dbWidget);
    m_databaseOpenDialog->setIntent(intent);
    m_databaseOpenDialog->setFilePath(filePath);

#ifdef Q_OS_MACOS
    if (intent == DatabaseOpenDialog::Intent::AutoType || intent == DatabaseOpenDialog::Intent::Browser) {
        macUtils()->raiseOwnWindow();
        Tools::wait(200);
    }
#endif

    m_databaseOpenDialog->show();
    m_databaseOpenDialog->raise();
    m_databaseOpenDialog->activateWindow();
}

/**
 * This function relock the pending database when autotype has been performed successfully
 * A database is marked as pending when it's unlocked after a global Auto-Type invocation
 */
void DatabaseTabWidget::relockPendingDatabase()
{
    if (!m_dbWidgetPendingLock || !config()->get(Config::Security_RelockAutoType).toBool()) {
        return;
    }

    if (m_dbWidgetPendingLock->isLocked() || !m_dbWidgetPendingLock->database()->isInitialized()) {
        m_dbWidgetPendingLock = nullptr;
        return;
    }

    m_dbWidgetPendingLock->lock();
    m_dbWidgetPendingLock = nullptr;
}

void DatabaseTabWidget::updateLastDatabases(const QString& filename)
{
    if (!config()->get(Config::RememberLastDatabases).toBool()) {
        config()->remove(Config::LastDatabases);
    } else {
        QStringList lastDatabases = config()->get(Config::LastDatabases).toStringList();
        lastDatabases.prepend(filename);
        lastDatabases.removeDuplicates();

        while (lastDatabases.count() > config()->get(Config::NumberOfRememberedLastDatabases).toInt()) {
            lastDatabases.removeLast();
        }
        config()->set(Config::LastDatabases, lastDatabases);
    }
}

void DatabaseTabWidget::emitActiveDatabaseChanged()
{
    emit activeDatabaseChanged(currentDatabaseWidget());
}

void DatabaseTabWidget::emitDatabaseLockChanged()
{
    auto* dbWidget = qobject_cast<DatabaseWidget*>(sender());
    Q_ASSERT(dbWidget);
    if (!dbWidget) {
        return;
    }

    if (dbWidget->isLocked()) {
        emit databaseLocked(dbWidget);
    } else {
        emit databaseUnlocked(dbWidget);
    }
}

void DatabaseTabWidget::performGlobalAutoType()
{
    QList<QSharedPointer<Database>> unlockedDatabases;

    for (int i = 0, c = count(); i < c; ++i) {
        auto* dbWidget = databaseWidgetFromIndex(i);
        if (!dbWidget->isLocked()) {
            unlockedDatabases.append(dbWidget->database());
        }
    }

    // TODO: allow for database selection during Auto-Type instead of using the current tab
    if (!unlockedDatabases.isEmpty()) {
        autoType()->performGlobalAutoType(unlockedDatabases);
    } else if (count() > 0) {
        if (config()->get(Config::Security_RelockAutoType).toBool()) {
            m_dbWidgetPendingLock = currentDatabaseWidget();
        }
        unlockDatabaseInDialog(currentDatabaseWidget(), DatabaseOpenDialog::Intent::AutoType);
    }
}

void DatabaseTabWidget::performBrowserUnlock()
{
    auto dbWidget = currentDatabaseWidget();
    if (dbWidget && dbWidget->isLocked()) {
        unlockDatabaseInDialog(dbWidget, DatabaseOpenDialog::Intent::Browser);
    }
}
