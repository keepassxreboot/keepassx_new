/*
 *  Copyright (C) 2018 KeePassXC Team <team@keepassxc.org>
 *  Copyright (C) 2010 Felix Geyer <debfx@fobos.de>
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

#ifndef KEEPASSX_DATABASEWIDGET_H
#define KEEPASSX_DATABASEWIDGET_H

#include <QFileSystemWatcher>
#include <QStackedWidget>

#include "DatabaseOpenDialog.h"
#include "config-keepassx.h"
#include "gui/MessageWidget.h"
#include "gui/csvImport/CsvImportWizard.h"
#include "gui/entry/EntryModel.h"

class DatabaseOpenWidget;
class KeePass1OpenWidget;
class OpVaultOpenWidget;
class DatabaseSettingsDialog;
class ReportsDialog;
class Database;
class FileWatcher;
class EditEntryWidget;
class EditGroupWidget;
class Entry;
class EntryView;
class EntrySearcher;
class Group;
class GroupView;
class QFile;
class QMenu;
class QSplitter;
class QLabel;
class MessageWidget;
class EntryPreviewWidget;

namespace Ui
{
    class SearchWidget;
}

class DatabaseWidget : public QStackedWidget
{
    Q_OBJECT

public:
    friend class DatabaseOpenDialog;

    enum class Mode
    {
        None,
        ImportMode,
        ViewMode,
        EditMode,
        LockedMode
    };

    explicit DatabaseWidget(QSharedPointer<Database> db, QWidget* parent = nullptr);
    explicit DatabaseWidget(const QString& filePath, QWidget* parent = nullptr);
    ~DatabaseWidget() override;

    void setFocus(Qt::FocusReason reason);

    QSharedPointer<Database> database() const;

    DatabaseWidget::Mode currentMode() const;
    bool isLocked() const;
    bool isSaving() const;
    bool isSorted() const;
    bool isSearchActive() const;
    bool isEntryViewActive() const;
    bool isEntryEditActive() const;
    bool isGroupEditActive() const;

    QString getCurrentSearch();
    void refreshSearch();

    GroupView* groupView();
    EntryView* entryView();

    Group* currentGroup() const;
    bool canDeleteCurrentGroup() const;
    bool isGroupSelected() const;
    bool isRecycleBinSelected() const;
    int numberOfSelectedEntries() const;
    int currentEntryIndex() const;

    QStringList customEntryAttributes() const;
    bool isEditWidgetModified() const;
    void clearAllWidgets();
    Entry* currentSelectedEntry();
    bool currentEntryHasTitle();
    bool currentEntryHasUsername();
    bool currentEntryHasPassword();
    bool currentEntryHasUrl();
    bool currentEntryHasNotes();
    bool currentEntryHasTotp();
#ifdef WITH_XC_SSHAGENT
    bool currentEntryHasSshKey();
#endif

    QByteArray entryViewState() const;
    bool setEntryViewState(const QByteArray& state) const;
    QList<int> mainSplitterSizes() const;
    void setMainSplitterSizes(const QList<int>& sizes);
    QList<int> previewSplitterSizes() const;
    void setPreviewSplitterSizes(const QList<int>& sizes);

signals:
    // relayed Database signals
    void databaseFilePathChanged(const QString& oldPath, const QString& newPath);
    void databaseModified();
    void databaseSaved();
    void databaseUnlocked();
    void databaseLockRequested();
    void databaseLocked();

    // Emitted in replaceDatabase, may be caused by lock, reload, unlock, load.
    void databaseReplaced(const QSharedPointer<Database>& oldDb, const QSharedPointer<Database>& newDb);

    void closeRequest();
    void currentModeChanged(DatabaseWidget::Mode mode);
    void groupChanged();
    void entrySelectionChanged();
    void
    requestOpenDatabase(const QString& filePath, bool inBackground, const QString& password, const QString& keyFile);
    void databaseMerged(QSharedPointer<Database> mergedDb);
    void groupContextMenuRequested(const QPoint& globalPos);
    void entryContextMenuRequested(const QPoint& globalPos);
    void listModeAboutToActivate();
    void listModeActivated();
    void searchModeAboutToActivate();
    void searchModeActivated();
    void mainSplitterSizesChanged();
    void previewSplitterSizesChanged();
    void entryViewStateChanged();
    void clearSearch();

public slots:
    bool lock();
    bool save();
    bool saveAs();
    bool saveBackup();

    void replaceDatabase(QSharedPointer<Database> db);
    void createEntry();
    void cloneEntry();
    void deleteSelectedEntries();
    void deleteEntries(QList<Entry*> entries, bool confirm = true);
    void focusOnEntries(bool editIfFocused = false);
    void focusOnGroups(bool editIfFocused = false);
    void moveEntryUp();
    void moveEntryDown();
    void copyTitle();
    void copyUsername();
    void copyPassword();
    void copyURL();
    void copyNotes();
    void copyAttribute(QAction* action);
    void showTotp();
    void showTotpKeyQrCode();
    void copyTotp();
    void setupTotp();
#ifdef WITH_XC_SSHAGENT
    void addToAgent();
    void removeFromAgent();
#endif
    void performAutoType(const QString& sequence = {});
    void performAutoTypeUsername();
    void performAutoTypeUsernameEnter();
    void performAutoTypePassword();
    void performAutoTypePasswordEnter();
    void openUrl();
    void downloadSelectedFavicons();
    void downloadAllFavicons();
    void openUrlForEntry(Entry* entry);
    void createGroup();
    void deleteGroup();
    void switchToMainView(bool previousDialogAccepted = false);
    void switchToEntryEdit();
    void switchToGroupEdit();
    void sortGroupsAsc();
    void sortGroupsDesc();
    void switchToDatabaseSecurity();
    void switchToDatabaseReports();
    void switchToDatabaseSettings();
    void switchToOpenDatabase();
    void switchToOpenDatabase(const QString& filePath);
    void switchToOpenDatabase(const QString& filePath, const QString& password, const QString& keyFile);
    void switchToCsvImport(const QString& filePath);
    void performUnlockDatabase(const QString& password, const QString& keyfile = {});
    void csvImportFinished(bool accepted);
    void switchToImportKeepass1(const QString& filePath);
    void switchToImportOpVault(const QString& fileName);
    void emptyRecycleBin();

    // Search related slots
    void search(const QString& searchtext);
    void setSearchCaseSensitive(bool state);
    void setSearchLimitGroup(bool state);
    void endSearch();

    void showMessage(const QString& text,
                     MessageWidget::MessageType type,
                     bool showClosebutton = true,
                     int autoHideTimeout = MessageWidget::DefaultAutoHideTimeout);
    void showErrorMessage(const QString& errorMessage);
    void hideMessage();

protected:
    void closeEvent(QCloseEvent* event) override;
    void showEvent(QShowEvent* event) override;
    bool focusNextPrevChild(bool next) override;

private slots:
    void entryActivationSignalReceived(Entry* entry, EntryModel::ModelColumn column);
    void switchBackToEntryEdit();
    void switchToHistoryView(Entry* entry);
    void switchToEntryEdit(Entry*);
    void switchToEntryEdit(Entry* entry, bool create);
    void switchToGroupEdit(Group* entry, bool create);
    void emitGroupContextMenuRequested(const QPoint& pos);
    void emitEntryContextMenuRequested(const QPoint& pos);
    void onEntryChanged(Entry* entry);
    void onGroupChanged();
    void onDatabaseModified();
    void connectDatabaseSignals();
    void loadDatabase(bool accepted);
    void unlockDatabase(bool accepted);
    void mergeDatabase(bool accepted);
    void emitCurrentModeChanged();
    // Database autoreload slots
    void reloadDatabaseFile();
    void restoreGroupEntryFocus(const QUuid& groupUuid, const QUuid& EntryUuid);
    void doAutoTypePasswordGenerated(QString password);

private:
    int addChildWidget(QWidget* w);
    void setClipboardTextAndMinimize(const QString& text);
    void processAutoOpen();
    void openDatabaseFromEntry(const Entry* entry, bool inBackground = true);
    void performIconDownloads(const QList<Entry*>& entries, bool force = false);
    bool performSave(QString& errorMessage, const QString& fileName = {});

    QSharedPointer<Database> m_db;

    QPointer<QWidget> m_mainWidget;
    QPointer<QSplitter> m_mainSplitter;
    QPointer<MessageWidget> m_messageWidget;
    QPointer<EntryPreviewWidget> m_previewView;
    QPointer<QSplitter> m_previewSplitter;
    QPointer<QLabel> m_searchingLabel;
    QPointer<QLabel> m_shareLabel;
    QPointer<CsvImportWizard> m_csvImportWizard;
    QPointer<EditEntryWidget> m_editEntryWidget;
    QPointer<EditGroupWidget> m_editGroupWidget;
    QPointer<EditEntryWidget> m_historyEditEntryWidget;
    QPointer<ReportsDialog> m_reportsDialog;
    QPointer<DatabaseSettingsDialog> m_databaseSettingDialog;
    QPointer<DatabaseOpenWidget> m_databaseOpenWidget;
    QPointer<KeePass1OpenWidget> m_keepass1OpenWidget;
    QPointer<OpVaultOpenWidget> m_opVaultOpenWidget;
    QPointer<GroupView> m_groupView;
    QPointer<EntryView> m_entryView;

    QScopedPointer<Group> m_newGroup;
    QScopedPointer<Entry> m_newEntry;
    QPointer<Group> m_newParent;

    QUuid m_groupBeforeLock;
    QUuid m_entryBeforeLock;

    int m_saveAttempts;

    // Search state
    QScopedPointer<EntrySearcher> m_entrySearcher;
    QString m_lastSearchText;
    bool m_searchLimitGroup;

    // Autoreload
    bool m_blockAutoSave;
};

#endif // KEEPASSX_DATABASEWIDGET_H
