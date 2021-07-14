/*
 * Copyright (C) 2018 KeePassXC Team <team@keepassxc.org>
 * Copyright (C) 2010 Felix Geyer <debfx@fobos.de>
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

#include "DatabaseWidget.h"

#include <QApplication>
#include <QBoxLayout>
#include <QCheckBox>
#include <QDesktopServices>
#include <QHostInfo>
#include <QKeyEvent>
#include <QProcess>
#include <QSplitter>
#include <QTextEdit>

#include "autotype/AutoType.h"
#include "core/EntrySearcher.h"
#include "core/Merger.h"
#include "gui/Clipboard.h"
#include "gui/CloneDialog.h"
#include "gui/EntryPreviewWidget.h"
#include "gui/FileDialog.h"
#include "gui/GuiTools.h"
#include "gui/KeePass1OpenWidget.h"
#include "gui/MainWindow.h"
#include "gui/MessageBox.h"
#include "gui/OpVaultOpenWidget.h"
#include "gui/TotpDialog.h"
#include "gui/TotpExportSettingsDialog.h"
#include "gui/TotpSetupDialog.h"
#include "gui/dbsettings/DatabaseSettingsDialog.h"
#include "gui/entry/EntryView.h"
#include "gui/group/EditGroupWidget.h"
#include "gui/group/GroupView.h"
#include "gui/reports/ReportsDialog.h"
#include "keeshare/KeeShare.h"

#ifdef WITH_XC_NETWORKING
#include "gui/IconDownloaderDialog.h"
#endif

#ifdef WITH_XC_SSHAGENT
#include "sshagent/SSHAgent.h"
#endif

DatabaseWidget::DatabaseWidget(QSharedPointer<Database> db, QWidget* parent)
    : QStackedWidget(parent)
    , m_db(std::move(db))
    , m_mainWidget(new QWidget(this))
    , m_mainSplitter(new QSplitter(m_mainWidget))
    , m_messageWidget(new MessageWidget(this))
    , m_previewView(new EntryPreviewWidget(this))
    , m_previewSplitter(new QSplitter(m_mainWidget))
    , m_searchingLabel(new QLabel(this))
    , m_shareLabel(new QLabel(this))
    , m_csvImportWizard(new CsvImportWizard(this))
    , m_editEntryWidget(new EditEntryWidget(this))
    , m_editGroupWidget(new EditGroupWidget(this))
    , m_historyEditEntryWidget(new EditEntryWidget(this))
    , m_reportsDialog(new ReportsDialog(this))
    , m_databaseSettingDialog(new DatabaseSettingsDialog(this))
    , m_databaseOpenWidget(new DatabaseOpenWidget(this))
    , m_keepass1OpenWidget(new KeePass1OpenWidget(this))
    , m_opVaultOpenWidget(new OpVaultOpenWidget(this))
    , m_groupView(new GroupView(m_db.data(), m_mainSplitter))
    , m_saveAttempts(0)
    , m_entrySearcher(new EntrySearcher(false))
{
    Q_ASSERT(m_db);

    m_messageWidget->setHidden(true);

    auto* mainLayout = new QVBoxLayout();
    mainLayout->addWidget(m_messageWidget);
    auto* hbox = new QHBoxLayout();
    mainLayout->addLayout(hbox);
    hbox->addWidget(m_mainSplitter);
    m_mainWidget->setLayout(mainLayout);

    auto* rightHandSideWidget = new QWidget(m_mainSplitter);
    auto* vbox = new QVBoxLayout();
    vbox->setMargin(0);
    vbox->addWidget(m_searchingLabel);
#ifdef WITH_XC_KEESHARE
    vbox->addWidget(m_shareLabel);
#endif
    vbox->addWidget(m_previewSplitter);
    rightHandSideWidget->setLayout(vbox);
    m_entryView = new EntryView(rightHandSideWidget);

    m_mainSplitter->setChildrenCollapsible(false);
    m_mainSplitter->addWidget(m_groupView);
    m_mainSplitter->addWidget(rightHandSideWidget);
    m_mainSplitter->setStretchFactor(0, 30);
    m_mainSplitter->setStretchFactor(1, 70);

    m_previewSplitter->setOrientation(Qt::Vertical);
    m_previewSplitter->setChildrenCollapsible(true);

    m_groupView->setObjectName("groupView");
    m_groupView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_groupView, SIGNAL(customContextMenuRequested(QPoint)), SLOT(emitGroupContextMenuRequested(QPoint)));

    m_entryView->setObjectName("entryView");
    m_entryView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_entryView->displayGroup(m_db->rootGroup());
    connect(m_entryView, SIGNAL(customContextMenuRequested(QPoint)), SLOT(emitEntryContextMenuRequested(QPoint)));

    // Add a notification for when we are searching
    m_searchingLabel->setObjectName("SearchBanner");
    m_searchingLabel->setText(tr("Searching…"));
    m_searchingLabel->setAlignment(Qt::AlignCenter);
    m_searchingLabel->setVisible(false);

#ifdef WITH_XC_KEESHARE
    m_shareLabel->setObjectName("KeeShareBanner");
    m_shareLabel->setText(tr("Shared group…"));
    m_shareLabel->setAlignment(Qt::AlignCenter);
    m_shareLabel->setVisible(false);
#endif

    m_previewView->setObjectName("previewWidget");
    m_previewView->hide();
    m_previewSplitter->addWidget(m_entryView);
    m_previewSplitter->addWidget(m_previewView);
    m_previewSplitter->setStretchFactor(0, 100);
    m_previewSplitter->setStretchFactor(1, 0);
    m_previewSplitter->setSizes({1, 1});

    m_editEntryWidget->setObjectName("editEntryWidget");
    m_editGroupWidget->setObjectName("editGroupWidget");
    m_csvImportWizard->setObjectName("csvImportWizard");
    m_reportsDialog->setObjectName("reportsDialog");
    m_databaseSettingDialog->setObjectName("databaseSettingsDialog");
    m_databaseOpenWidget->setObjectName("databaseOpenWidget");
    m_keepass1OpenWidget->setObjectName("keepass1OpenWidget");
    m_opVaultOpenWidget->setObjectName("opVaultOpenWidget");

    addChildWidget(m_mainWidget);
    addChildWidget(m_editEntryWidget);
    addChildWidget(m_editGroupWidget);
    addChildWidget(m_reportsDialog);
    addChildWidget(m_databaseSettingDialog);
    addChildWidget(m_historyEditEntryWidget);
    addChildWidget(m_databaseOpenWidget);
    addChildWidget(m_csvImportWizard);
    addChildWidget(m_keepass1OpenWidget);
    addChildWidget(m_opVaultOpenWidget);

    // clang-format off
    connect(m_mainSplitter, SIGNAL(splitterMoved(int,int)), SIGNAL(mainSplitterSizesChanged()));
    connect(m_previewSplitter, SIGNAL(splitterMoved(int,int)), SIGNAL(previewSplitterSizesChanged()));
    connect(this, SIGNAL(currentModeChanged(DatabaseWidget::Mode)), m_previewView, SLOT(setDatabaseMode(DatabaseWidget::Mode)));
    connect(m_previewView, SIGNAL(errorOccurred(QString)), SLOT(showErrorMessage(QString)));
    connect(m_previewView, SIGNAL(entryUrlActivated(Entry*)), SLOT(openUrlForEntry(Entry*)));
    connect(m_entryView, SIGNAL(viewStateChanged()), SIGNAL(entryViewStateChanged()));
    connect(m_groupView, SIGNAL(groupSelectionChanged()), SLOT(onGroupChanged()));
    connect(m_groupView, SIGNAL(groupSelectionChanged()), SIGNAL(groupChanged()));
    connect(m_groupView, &GroupView::groupFocused, this, [this] { m_previewView->setGroup(currentGroup()); });
    connect(m_entryView, SIGNAL(entryActivated(Entry*,EntryModel::ModelColumn)),
        SLOT(entryActivationSignalReceived(Entry*,EntryModel::ModelColumn)));
    connect(m_entryView, SIGNAL(entrySelectionChanged(Entry*)), SLOT(onEntryChanged(Entry*)));
    connect(m_editEntryWidget, SIGNAL(editFinished(bool)), SLOT(switchToMainView(bool)));
    connect(m_editEntryWidget, SIGNAL(historyEntryActivated(Entry*)), SLOT(switchToHistoryView(Entry*)));
    connect(m_historyEditEntryWidget, SIGNAL(editFinished(bool)), SLOT(switchBackToEntryEdit()));
    connect(m_editGroupWidget, SIGNAL(editFinished(bool)), SLOT(switchToMainView(bool)));
    connect(m_reportsDialog, SIGNAL(editFinished(bool)), SLOT(switchToMainView(bool)));
    connect(m_databaseSettingDialog, SIGNAL(editFinished(bool)), SLOT(switchToMainView(bool)));
    connect(m_databaseOpenWidget, SIGNAL(dialogFinished(bool)), SLOT(loadDatabase(bool)));
    connect(m_keepass1OpenWidget, SIGNAL(dialogFinished(bool)), SLOT(loadDatabase(bool)));
    connect(m_opVaultOpenWidget, SIGNAL(dialogFinished(bool)), SLOT(loadDatabase(bool)));
    connect(m_csvImportWizard, SIGNAL(importFinished(bool)), SLOT(csvImportFinished(bool)));
    connect(this, SIGNAL(currentChanged(int)), SLOT(emitCurrentModeChanged()));
    // clang-format on

    connectDatabaseSignals();

    m_blockAutoSave = false;

    m_searchLimitGroup = config()->get(Config::SearchLimitGroup).toBool();

#ifdef WITH_XC_KEESHARE
    // We need to reregister the database to allow exports
    // from a newly created database
    KeeShare::instance()->connectDatabase(m_db, {});
#endif

    if (m_db->isInitialized()) {
        switchToMainView();
    } else {
        switchToOpenDatabase();
    }
}

DatabaseWidget::DatabaseWidget(const QString& filePath, QWidget* parent)
    : DatabaseWidget(QSharedPointer<Database>::create(filePath), parent)
{
}

DatabaseWidget::~DatabaseWidget()
{
    // Trigger any Database deletion related signals manually by
    // explicitly clearing the Database pointer, instead of leaving it to ~QSharedPointer.
    // QSharedPointer may behave differently depending on whether it is cleared by the `clear` method
    // or by its destructor. In the latter case, the ref counter may not be correctly maintained
    // if a copy of the QSharedPointer is created in any slots activated by the Database destructor.
    // More details: https://github.com/keepassxreboot/keepassxc/issues/6393.
    m_db.clear();
}

QSharedPointer<Database> DatabaseWidget::database() const
{
    return m_db;
}

DatabaseWidget::Mode DatabaseWidget::currentMode() const
{
    if (currentWidget() == nullptr) {
        return Mode::None;
    } else if (currentWidget() == m_mainWidget) {
        return Mode::ViewMode;
    } else if (currentWidget() == m_databaseOpenWidget || currentWidget() == m_keepass1OpenWidget) {
        return Mode::LockedMode;
    } else if (currentWidget() == m_csvImportWizard) {
        return Mode::ImportMode;
    } else {
        return Mode::EditMode;
    }
}

bool DatabaseWidget::isLocked() const
{
    return currentMode() == Mode::LockedMode;
}

bool DatabaseWidget::isSaving() const
{
    return m_db->isSaving();
}

bool DatabaseWidget::isSorted() const
{
    return m_entryView->isSorted();
}

bool DatabaseWidget::isSearchActive() const
{
    return m_entryView->inSearchMode();
}

bool DatabaseWidget::isEntryViewActive() const
{
    return currentWidget() == m_mainWidget;
}

bool DatabaseWidget::isEntryEditActive() const
{
    return currentWidget() == m_editEntryWidget;
}

bool DatabaseWidget::isGroupEditActive() const
{
    return currentWidget() == m_editGroupWidget;
}

bool DatabaseWidget::isEditWidgetModified() const
{
    if (currentWidget() == m_editEntryWidget) {
        return m_editEntryWidget->isModified();
    } else if (currentWidget() == m_editGroupWidget) {
        return m_editGroupWidget->isModified();
    }
    return false;
}

QList<int> DatabaseWidget::mainSplitterSizes() const
{
    return m_mainSplitter->sizes();
}

void DatabaseWidget::setMainSplitterSizes(const QList<int>& sizes)
{
    m_mainSplitter->setSizes(sizes);
}

QList<int> DatabaseWidget::previewSplitterSizes() const
{
    return m_previewSplitter->sizes();
}

void DatabaseWidget::setPreviewSplitterSizes(const QList<int>& sizes)
{
    m_previewSplitter->setSizes(sizes);
}

/**
 * Get current view state of entry view
 */
QByteArray DatabaseWidget::entryViewState() const
{
    return m_entryView->viewState();
}

/**
 * Set view state of entry view
 */
bool DatabaseWidget::setEntryViewState(const QByteArray& state) const
{
    return m_entryView->setViewState(state);
}

void DatabaseWidget::clearAllWidgets()
{
    m_editEntryWidget->clear();
    m_historyEditEntryWidget->clear();
    m_editGroupWidget->clear();
    m_previewView->clear();
}

void DatabaseWidget::emitCurrentModeChanged()
{
    emit currentModeChanged(currentMode());
}

void DatabaseWidget::createEntry()
{
    Q_ASSERT(m_groupView->currentGroup());
    if (!m_groupView->currentGroup()) {
        return;
    }

    m_newEntry.reset(new Entry());

    if (isSearchActive()) {
        m_newEntry->setTitle(getCurrentSearch());
        endSearch();
    }
    m_newEntry->setUuid(QUuid::createUuid());
    m_newEntry->setUsername(m_db->metadata()->defaultUserName());
    m_newParent = m_groupView->currentGroup();
    m_newParent->applyGroupIconOnCreateTo(m_newEntry.data());
    switchToEntryEdit(m_newEntry.data(), true);
}

void DatabaseWidget::replaceDatabase(QSharedPointer<Database> db)
{
    Q_ASSERT(!isEntryEditActive() && !isGroupEditActive());

    // Save off new parent UUID which will be valid when creating a new entry
    QUuid newParentUuid;
    if (m_newParent) {
        newParentUuid = m_newParent->uuid();
    }

    // TODO: instead of increasing the ref count temporarily, there should be a clean
    // break from the old database. Without this crashes occur due to the change
    // signals triggering dangling pointers.
    auto oldDb = m_db;
    m_db = std::move(db);
    connectDatabaseSignals();
    m_groupView->changeDatabase(m_db);

    // Restore the new parent group pointer, if not found default to the root group
    // this prevents data loss when merging a database while creating a new entry
    if (!newParentUuid.isNull()) {
        m_newParent = m_db->rootGroup()->findGroupByUuid(newParentUuid);
        if (!m_newParent) {
            m_newParent = m_db->rootGroup();
        }
    }

    emit databaseReplaced(oldDb, m_db);

#if defined(WITH_XC_KEESHARE)
    KeeShare::instance()->connectDatabase(m_db, oldDb);
#else
    // Keep the instance active till the end of this function
    Q_UNUSED(oldDb);
#endif

    oldDb->releaseData();
}

void DatabaseWidget::cloneEntry()
{
    auto currentEntry = currentSelectedEntry();
    Q_ASSERT(currentEntry);
    if (!currentEntry) {
        return;
    }

    auto cloneDialog = new CloneDialog(this, m_db.data(), currentEntry);
    cloneDialog->show();
}

void DatabaseWidget::showTotp()
{
    auto currentEntry = currentSelectedEntry();
    Q_ASSERT(currentEntry);
    if (!currentEntry) {
        return;
    }

    auto totpDialog = new TotpDialog(this, currentEntry);
    connect(this, &DatabaseWidget::databaseLockRequested, totpDialog, &TotpDialog::close);
    totpDialog->open();
}

void DatabaseWidget::copyTotp()
{
    auto currentEntry = currentSelectedEntry();
    Q_ASSERT(currentEntry);
    if (!currentEntry) {
        return;
    }
    setClipboardTextAndMinimize(currentEntry->totp());
}

void DatabaseWidget::setupTotp()
{
    auto currentEntry = currentSelectedEntry();
    Q_ASSERT(currentEntry);
    if (!currentEntry) {
        return;
    }

    auto setupTotpDialog = new TotpSetupDialog(this, currentEntry);
    connect(setupTotpDialog, SIGNAL(totpUpdated()), SIGNAL(entrySelectionChanged()));
    connect(this, &DatabaseWidget::databaseLockRequested, setupTotpDialog, &TotpSetupDialog::close);
    setupTotpDialog->open();
}

void DatabaseWidget::deleteSelectedEntries()
{
    const QModelIndexList selected = m_entryView->selectionModel()->selectedRows();
    if (selected.isEmpty()) {
        return;
    }

    // Resolve entries from the selection model
    QList<Entry*> selectedEntries;
    for (const QModelIndex& index : selected) {
        selectedEntries.append(m_entryView->entryFromIndex(index));
    }

    deleteEntries(std::move(selectedEntries));
}

void DatabaseWidget::deleteEntries(QList<Entry*> selectedEntries, bool confirm)
{
    // Confirm entry removal before moving forward
    auto* recycleBin = m_db->metadata()->recycleBin();
    bool permanent = (recycleBin && recycleBin->findEntryByUuid(selectedEntries.first()->uuid()))
                     || !m_db->metadata()->recycleBinEnabled();

    if (confirm && !GuiTools::confirmDeleteEntries(this, selectedEntries, permanent)) {
        return;
    }

    GuiTools::deleteEntriesResolveReferences(this, selectedEntries, permanent);

    refreshSearch();

    m_entryView->setFirstEntryActive();
    auto* currentEntry = currentSelectedEntry();
    if (currentEntry) {
        m_previewView->setEntry(currentEntry);
    } else {
        m_previewView->setGroup(groupView()->currentGroup());
    }
}

void DatabaseWidget::setFocus(Qt::FocusReason reason)
{
    if (reason == Qt::BacktabFocusReason) {
        m_previewView->setFocus();
    } else {
        m_groupView->setFocus();
    }
}

void DatabaseWidget::focusOnEntries(bool editIfFocused)
{
    if (isEntryViewActive()) {
        if (editIfFocused && m_entryView->hasFocus()) {
            switchToEntryEdit();
        } else {
            m_entryView->setFocus();
        }
    }
}

void DatabaseWidget::focusOnGroups(bool editIfFocused)
{
    if (isEntryViewActive()) {
        if (editIfFocused && m_groupView->hasFocus()) {
            switchToGroupEdit();
        } else {
            m_groupView->setFocus();
        }
    }
}

void DatabaseWidget::moveEntryUp()
{
    auto currentEntry = currentSelectedEntry();
    if (currentEntry) {
        currentEntry->moveUp();
        m_entryView->setCurrentEntry(currentEntry);
    }
}

void DatabaseWidget::moveEntryDown()
{
    auto currentEntry = currentSelectedEntry();
    if (currentEntry) {
        currentEntry->moveDown();
        m_entryView->setCurrentEntry(currentEntry);
    }
}

void DatabaseWidget::copyTitle()
{
    auto currentEntry = currentSelectedEntry();
    if (currentEntry) {
        setClipboardTextAndMinimize(currentEntry->resolveMultiplePlaceholders(currentEntry->title()));
    }
}

void DatabaseWidget::copyUsername()
{
    auto currentEntry = currentSelectedEntry();
    if (currentEntry) {
        setClipboardTextAndMinimize(currentEntry->resolveMultiplePlaceholders(currentEntry->username()));
    }
}

void DatabaseWidget::copyPassword()
{
    // QTextEdit does not properly trap Ctrl+C copy shortcut
    // if a text edit has focus pass the copy operation to it
    auto textEdit = qobject_cast<QTextEdit*>(focusWidget());
    if (textEdit) {
        textEdit->copy();
        return;
    }

    auto currentEntry = currentSelectedEntry();
    if (currentEntry) {
        setClipboardTextAndMinimize(currentEntry->resolveMultiplePlaceholders(currentEntry->password()));
    }
}

void DatabaseWidget::copyURL()
{
    auto currentEntry = currentSelectedEntry();
    if (currentEntry) {
        setClipboardTextAndMinimize(currentEntry->resolveMultiplePlaceholders(currentEntry->url()));
    }
}

void DatabaseWidget::copyNotes()
{
    auto currentEntry = currentSelectedEntry();
    if (currentEntry) {
        setClipboardTextAndMinimize(currentEntry->resolveMultiplePlaceholders(currentEntry->notes()));
    }
}

void DatabaseWidget::copyAttribute(QAction* action)
{
    auto currentEntry = currentSelectedEntry();
    if (currentEntry) {
        setClipboardTextAndMinimize(
            currentEntry->resolveMultiplePlaceholders(currentEntry->attributes()->value(action->data().toString())));
    }
}

void DatabaseWidget::showTotpKeyQrCode()
{
    auto currentEntry = currentSelectedEntry();
    if (currentEntry) {
        auto totpDisplayDialog = new TotpExportSettingsDialog(this, currentEntry);
        connect(this, &DatabaseWidget::databaseLockRequested, totpDisplayDialog, &TotpExportSettingsDialog::close);
        totpDisplayDialog->open();
    }
}

void DatabaseWidget::setClipboardTextAndMinimize(const QString& text)
{
    clipboard()->setText(text);
    if (config()->get(Config::HideWindowOnCopy).toBool()) {
        if (config()->get(Config::MinimizeOnCopy).toBool()) {
            getMainWindow()->minimizeOrHide();
        } else if (config()->get(Config::DropToBackgroundOnCopy).toBool()) {
            window()->lower();
        }
    }
}

#ifdef WITH_XC_SSHAGENT
void DatabaseWidget::addToAgent()
{
    Entry* currentEntry = m_entryView->currentEntry();
    Q_ASSERT(currentEntry);
    if (!currentEntry) {
        return;
    }

    KeeAgentSettings settings;
    if (!settings.fromEntry(currentEntry)) {
        return;
    }

    OpenSSHKey key;
    if (settings.toOpenSSHKey(currentEntry, key, true)) {
        SSHAgent::instance()->addIdentity(key, settings, database()->uuid());
    } else {
        m_messageWidget->showMessage(key.errorString(), MessageWidget::Error);
    }
}

void DatabaseWidget::removeFromAgent()
{
    Entry* currentEntry = m_entryView->currentEntry();
    Q_ASSERT(currentEntry);
    if (!currentEntry) {
        return;
    }

    KeeAgentSettings settings;
    if (!settings.fromEntry(currentEntry)) {
        return;
    }

    OpenSSHKey key;
    if (settings.toOpenSSHKey(currentEntry, key, false)) {
        SSHAgent::instance()->removeIdentity(key);
    } else {
        m_messageWidget->showMessage(key.errorString(), MessageWidget::Error);
    }
}
#endif

void DatabaseWidget::performAutoType(const QString& sequence)
{
    auto currentEntry = currentSelectedEntry();
    if (currentEntry) {
        // TODO: Include name of previously active window in confirmation question
        if (config()->get(Config::Security_AutoTypeAsk).toBool()
            && MessageBox::question(
                   this, tr("Confirm Auto-Type"), tr("Perform Auto-Type into the previously active window?"))
                   != MessageBox::Yes) {
            return;
        }

        if (sequence.isEmpty()) {
            autoType()->performAutoType(currentEntry, window());
        } else {
            autoType()->performAutoTypeWithSequence(currentEntry, sequence, window());
        }
    }
}

void DatabaseWidget::performAutoTypeUsername()
{
    performAutoType(QStringLiteral("{USERNAME}"));
}

void DatabaseWidget::performAutoTypeUsernameEnter()
{
    performAutoType(QStringLiteral("{USERNAME}{ENTER}"));
}

void DatabaseWidget::performAutoTypePassword()
{
    performAutoType(QStringLiteral("{PASSWORD}"));
}

void DatabaseWidget::performAutoTypePasswordEnter()
{
    performAutoType(QStringLiteral("{PASSWORD}{ENTER}"));
}

void DatabaseWidget::openUrl()
{
    auto currentEntry = currentSelectedEntry();
    if (currentEntry) {
        openUrlForEntry(currentEntry);
    }
}

void DatabaseWidget::downloadSelectedFavicons()
{
#ifdef WITH_XC_NETWORKING
    QList<Entry*> selectedEntries;
    for (const auto& index : m_entryView->selectionModel()->selectedRows()) {
        selectedEntries.append(m_entryView->entryFromIndex(index));
    }

    // Force download even if icon already exists
    performIconDownloads(selectedEntries, true);
#endif
}

void DatabaseWidget::downloadAllFavicons()
{
#ifdef WITH_XC_NETWORKING
    auto currentGroup = m_groupView->currentGroup();
    if (currentGroup) {
        performIconDownloads(currentGroup->entries());
    }
#endif
}

void DatabaseWidget::performIconDownloads(const QList<Entry*>& entries, bool force)
{
#ifdef WITH_XC_NETWORKING
    auto* iconDownloaderDialog = new IconDownloaderDialog(this);
    connect(this, SIGNAL(databaseLockRequested()), iconDownloaderDialog, SLOT(close()));
    iconDownloaderDialog->downloadFavicons(m_db, entries, force);
#else
    Q_UNUSED(entries);
    Q_UNUSED(force);
#endif
}

void DatabaseWidget::openUrlForEntry(Entry* entry)
{
    Q_ASSERT(entry);
    if (!entry) {
        return;
    }

    QString cmdString = entry->resolveMultiplePlaceholders(entry->url());
    if (cmdString.startsWith("cmd://")) {
        // check if decision to execute command was stored
        bool launch = (entry->attributes()->value(EntryAttributes::RememberCmdExecAttr) == "1");

        // otherwise ask user
        if (!launch && cmdString.length() > 6) {
            QString cmdTruncated = entry->resolveMultiplePlaceholders(entry->maskPasswordPlaceholders(entry->url()));
            cmdTruncated = cmdTruncated.mid(6);
            if (cmdTruncated.length() > 400) {
                cmdTruncated = cmdTruncated.left(400) + " […]";
            }
            QMessageBox msgbox(QMessageBox::Icon::Question,
                               tr("Execute command?"),
                               tr("Do you really want to execute the following command?<br><br>%1<br>")
                                   .arg(cmdTruncated.toHtmlEscaped()),
                               QMessageBox::Yes | QMessageBox::No,
                               this);
            msgbox.setDefaultButton(QMessageBox::No);

            QCheckBox* checkbox = new QCheckBox(tr("Remember my choice"), &msgbox);
            msgbox.setCheckBox(checkbox);
            bool remember = false;
            QObject::connect(checkbox, &QCheckBox::stateChanged, [&](int state) {
                if (static_cast<Qt::CheckState>(state) == Qt::CheckState::Checked) {
                    remember = true;
                }
            });

            int result = msgbox.exec();
            launch = (result == QMessageBox::Yes);

            if (remember) {
                entry->attributes()->set(EntryAttributes::RememberCmdExecAttr, result == QMessageBox::Yes ? "1" : "0");
            }
        }

        if (launch) {
            QProcess::startDetached(cmdString.mid(6));

            if (config()->get(Config::MinimizeOnOpenUrl).toBool()) {
                getMainWindow()->minimizeOrHide();
            }
        }
    } else if (cmdString.startsWith("kdbx://")) {
        openDatabaseFromEntry(entry, false);
    } else {
        QUrl url = QUrl::fromUserInput(entry->resolveMultiplePlaceholders(entry->url()));
        if (!url.isEmpty()) {
            QDesktopServices::openUrl(url);

            if (config()->get(Config::MinimizeOnOpenUrl).toBool()) {
                getMainWindow()->minimizeOrHide();
            }
        }
    }
}

Entry* DatabaseWidget::currentSelectedEntry()
{
    if (currentWidget() == m_editEntryWidget) {
        return m_editEntryWidget->currentEntry();
    }

    return m_entryView->currentEntry();
}

void DatabaseWidget::createGroup()
{
    Q_ASSERT(m_groupView->currentGroup());
    if (!m_groupView->currentGroup()) {
        return;
    }

    m_newGroup.reset(new Group());
    m_newGroup->setUuid(QUuid::createUuid());
    m_newParent = m_groupView->currentGroup();
    switchToGroupEdit(m_newGroup.data(), true);
}

void DatabaseWidget::deleteGroup()
{
    Group* currentGroup = m_groupView->currentGroup();
    Q_ASSERT(currentGroup && canDeleteCurrentGroup());
    if (!currentGroup || !canDeleteCurrentGroup()) {
        return;
    }

    auto* recycleBin = m_db->metadata()->recycleBin();
    bool inRecycleBin = recycleBin && recycleBin->findGroupByUuid(currentGroup->uuid());
    bool isRecycleBin = recycleBin && (currentGroup == recycleBin);
    bool isRecycleBinSubgroup = recycleBin && currentGroup->findGroupByUuid(recycleBin->uuid());
    if (inRecycleBin || isRecycleBin || isRecycleBinSubgroup || !m_db->metadata()->recycleBinEnabled()) {
        auto result = MessageBox::question(
            this,
            tr("Delete group"),
            tr("Do you really want to delete the group \"%1\" for good?").arg(currentGroup->name().toHtmlEscaped()),
            MessageBox::Delete | MessageBox::Cancel,
            MessageBox::Cancel);

        if (result == MessageBox::Delete) {
            delete currentGroup;
        }
    } else {
        auto result = MessageBox::question(this,
                                           tr("Move group to recycle bin?"),
                                           tr("Do you really want to move the group "
                                              "\"%1\" to the recycle bin?")
                                               .arg(currentGroup->name().toHtmlEscaped()),
                                           MessageBox::Move | MessageBox::Cancel,
                                           MessageBox::Cancel);
        if (result == MessageBox::Move) {
            m_db->recycleGroup(currentGroup);
        }
    }
}

int DatabaseWidget::addChildWidget(QWidget* w)
{
    w->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    int index = QStackedWidget::addWidget(w);
    adjustSize();
    return index;
}

void DatabaseWidget::switchToMainView(bool previousDialogAccepted)
{
    setCurrentWidget(m_mainWidget);

    if (m_newGroup) {
        if (previousDialogAccepted) {
            m_newGroup->setParent(m_newParent);
            m_groupView->setCurrentGroup(m_newGroup.take());
            m_groupView->expandGroup(m_newParent);
        } else {
            m_newGroup.reset();
        }

        m_newParent = nullptr;
    } else if (m_newEntry) {
        if (previousDialogAccepted) {
            m_newEntry->setGroup(m_newParent);
            m_entryView->setFocus();
            m_entryView->setCurrentEntry(m_newEntry.take());
        } else {
            m_newEntry.reset();
        }

        m_newParent = nullptr;
    } else {
        // Workaround: ensure entries are focused so search doesn't reset
        m_entryView->setFocus();
    }

    if (sender() == m_entryView || sender() == m_editEntryWidget) {
        onEntryChanged(m_entryView->currentEntry());
    } else if (sender() == m_groupView || sender() == m_editGroupWidget) {
        onGroupChanged();
    }
}

void DatabaseWidget::switchToHistoryView(Entry* entry)
{
    auto entryTitle = m_editEntryWidget->currentEntry() ? m_editEntryWidget->currentEntry()->title() : "";
    m_historyEditEntryWidget->loadEntry(entry, false, true, entryTitle, m_db);
    setCurrentWidget(m_historyEditEntryWidget);
}

void DatabaseWidget::switchBackToEntryEdit()
{
    setCurrentWidget(m_editEntryWidget);
}

void DatabaseWidget::switchToEntryEdit(Entry* entry)
{
    switchToEntryEdit(entry, false);
}

void DatabaseWidget::switchToEntryEdit(Entry* entry, bool create)
{
    // If creating an entry, it will be in `currentGroup()` so it's
    // okay to use but when editing, the entry may not be in
    // `currentGroup()` so we get the entry's group.
    Group* group;
    if (create) {
        group = currentGroup();
    } else {
        group = entry->group();
        // Ensure we have only this entry selected
        m_entryView->setCurrentEntry(entry);
    }

    Q_ASSERT(group);

    // Setup the entry edit widget and display
    m_editEntryWidget->loadEntry(entry, create, false, group->name(), m_db);
    setCurrentWidget(m_editEntryWidget);
}

void DatabaseWidget::switchToGroupEdit(Group* group, bool create)
{
    m_editGroupWidget->loadGroup(group, create, m_db);
    setCurrentWidget(m_editGroupWidget);
}

void DatabaseWidget::connectDatabaseSignals()
{
    // relayed Database events
    connect(m_db.data(),
            SIGNAL(filePathChanged(QString, QString)),

            SIGNAL(databaseFilePathChanged(QString, QString)));
    connect(m_db.data(), &Database::modified, this, &DatabaseWidget::databaseModified);
    connect(m_db.data(), &Database::modified, this, &DatabaseWidget::onDatabaseModified);
    connect(m_db.data(), &Database::databaseSaved, this, &DatabaseWidget::databaseSaved);
    connect(m_db.data(), &Database::databaseFileChanged, this, &DatabaseWidget::reloadDatabaseFile);
}

void DatabaseWidget::loadDatabase(bool accepted)
{
    auto* openWidget = qobject_cast<DatabaseOpenWidget*>(sender());
    Q_ASSERT(openWidget);
    if (!openWidget) {
        return;
    }

    if (accepted) {
        replaceDatabase(openWidget->database());
        switchToMainView();
        processAutoOpen();
        restoreGroupEntryFocus(m_groupBeforeLock, m_entryBeforeLock);
        m_groupBeforeLock = QUuid();
        m_entryBeforeLock = QUuid();
        m_saveAttempts = 0;
        emit databaseUnlocked();
#ifdef WITH_XC_SSHAGENT
        sshAgent()->databaseUnlocked(m_db);
#endif
        if (config()->get(Config::MinimizeAfterUnlock).toBool()) {
            getMainWindow()->minimizeOrHide();
        }
    } else {
        if (m_databaseOpenWidget->database()) {
            m_databaseOpenWidget->database().reset();
        }
        emit closeRequest();
    }
}

void DatabaseWidget::mergeDatabase(bool accepted)
{
    if (accepted) {
        if (!m_db) {
            showMessage(tr("No current database."), MessageWidget::Error);
            return;
        }

        auto* senderDialog = qobject_cast<DatabaseOpenDialog*>(sender());

        Q_ASSERT(senderDialog);
        if (!senderDialog) {
            return;
        }
        auto srcDb = senderDialog->database();

        if (!srcDb) {
            showMessage(tr("No source database, nothing to do."), MessageWidget::Error);
            return;
        }

        Merger merger(srcDb.data(), m_db.data());
        QStringList changeList = merger.merge();

        if (!changeList.isEmpty()) {
            showMessage(tr("Successfully merged the database files."), MessageWidget::Information);
        } else {
            showMessage(tr("Database was not modified by merge operation."), MessageWidget::Information);
        }
    }

    switchToMainView();
    emit databaseMerged(m_db);
}

/**
 * Unlock the database.
 *
 * @param accepted true if the unlock dialog or widget was confirmed with OK
 */
void DatabaseWidget::unlockDatabase(bool accepted)
{
    auto* senderDialog = qobject_cast<DatabaseOpenDialog*>(sender());

    if (!accepted) {
        if (!senderDialog && (!m_db || !m_db->isInitialized())) {
            emit closeRequest();
        }
        return;
    }

    if (senderDialog && senderDialog->intent() == DatabaseOpenDialog::Intent::Merge) {
        mergeDatabase(accepted);
        return;
    }

    QSharedPointer<Database> db;
    if (senderDialog) {
        db = senderDialog->database();
    } else {
        db = m_databaseOpenWidget->database();
    }
    replaceDatabase(db);
    if (db->isReadOnly()) {
        showMessage(
            tr("This database is opened in read-only mode. Autosave is disabled."), MessageWidget::Warning, false, -1);
    }

    restoreGroupEntryFocus(m_groupBeforeLock, m_entryBeforeLock);
    m_groupBeforeLock = QUuid();
    m_entryBeforeLock = QUuid();

    switchToMainView();
    processAutoOpen();
    emit databaseUnlocked();

#ifdef WITH_XC_SSHAGENT
    sshAgent()->databaseUnlocked(m_db);
#endif

    if (config()->get(Config::MinimizeAfterUnlock).toBool()) {
        getMainWindow()->minimizeOrHide();
    }

    if (senderDialog && senderDialog->intent() == DatabaseOpenDialog::Intent::AutoType) {
        QList<QSharedPointer<Database>> dbList;
        dbList.append(m_db);
        autoType()->performGlobalAutoType(dbList);
    }
}

void DatabaseWidget::entryActivationSignalReceived(Entry* entry, EntryModel::ModelColumn column)
{
    Q_ASSERT(entry);
    if (!entry) {
        return;
    }

    // Implement 'copy-on-doubleclick' functionality for certain columns
    switch (column) {
    case EntryModel::Username:
        if (config()->get(Config::Security_EnableCopyOnDoubleClick).toBool()) {
            setClipboardTextAndMinimize(entry->resolveMultiplePlaceholders(entry->username()));
        } else {
            switchToEntryEdit(entry);
        }
        break;
    case EntryModel::Password:
        if (config()->get(Config::Security_EnableCopyOnDoubleClick).toBool()) {
            setClipboardTextAndMinimize(entry->resolveMultiplePlaceholders(entry->password()));
        } else {
            switchToEntryEdit(entry);
        }
        break;
    case EntryModel::Url:
        if (!entry->url().isEmpty()) {
            openUrlForEntry(entry);
        }
        break;
    case EntryModel::Totp:
        if (entry->hasTotp()) {
            setClipboardTextAndMinimize(entry->totp());
        } else {
            setupTotp();
        }
        break;
    case EntryModel::ParentGroup:
        // Call this first to clear out of search mode, otherwise
        // the desired entry is not properly selected
        endSearch();
        m_groupView->setCurrentGroup(entry->group());
        m_entryView->setCurrentEntry(entry);
        break;
    // TODO: switch to 'Notes' tab in details view/pane
    // case EntryModel::Notes:
    //    break;
    // TODO: switch to 'Attachments' tab in details view/pane
    // case EntryModel::Attachments:
    //    break;
    default:
        switchToEntryEdit(entry);
    }
}

void DatabaseWidget::switchToDatabaseReports()
{
    m_reportsDialog->load(m_db);
    setCurrentWidget(m_reportsDialog);
}

void DatabaseWidget::switchToDatabaseSettings()
{
    m_databaseSettingDialog->load(m_db);
    setCurrentWidget(m_databaseSettingDialog);
}

void DatabaseWidget::switchToOpenDatabase()
{
    if (currentWidget() != m_databaseOpenWidget || m_databaseOpenWidget->filename() != m_db->filePath()) {
        switchToOpenDatabase(m_db->filePath());
    }
}

void DatabaseWidget::switchToOpenDatabase(const QString& filePath)
{
    m_databaseOpenWidget->load(filePath);
    setCurrentWidget(m_databaseOpenWidget);
}

void DatabaseWidget::switchToOpenDatabase(const QString& filePath, const QString& password, const QString& keyFile)
{
    switchToOpenDatabase(filePath);
    m_databaseOpenWidget->enterKey(password, keyFile);
}

void DatabaseWidget::switchToCsvImport(const QString& filePath)
{
    setCurrentWidget(m_csvImportWizard);
    m_csvImportWizard->load(filePath, m_db.data());
}

void DatabaseWidget::csvImportFinished(bool accepted)
{
    if (!accepted) {
        emit closeRequest();
    } else {
        switchToMainView();
    }
}

void DatabaseWidget::switchToImportKeepass1(const QString& filePath)
{
    m_keepass1OpenWidget->load(filePath);
    setCurrentWidget(m_keepass1OpenWidget);
}

void DatabaseWidget::switchToImportOpVault(const QString& fileName)
{
    m_opVaultOpenWidget->load(fileName);
    setCurrentWidget(m_opVaultOpenWidget);
}

void DatabaseWidget::switchToEntryEdit()
{
    auto entry = m_entryView->currentEntry();
    if (!entry) {
        return;
    }

    switchToEntryEdit(entry, false);
}

void DatabaseWidget::switchToGroupEdit()
{
    auto group = m_groupView->currentGroup();
    if (!group) {
        return;
    }

    switchToGroupEdit(group, false);
}

void DatabaseWidget::sortGroupsAsc()
{
    m_groupView->sortGroups();
}

void DatabaseWidget::sortGroupsDesc()
{
    m_groupView->sortGroups(true);
}

void DatabaseWidget::switchToDatabaseSecurity()
{
    switchToDatabaseSettings();
    m_databaseSettingDialog->showDatabaseKeySettings();
}

void DatabaseWidget::performUnlockDatabase(const QString& password, const QString& keyfile)
{
    if (password.isEmpty() && keyfile.isEmpty()) {
        return;
    }

    if (!m_db->isInitialized() || isLocked()) {
        switchToOpenDatabase();
        m_databaseOpenWidget->enterKey(password, keyfile);
    }
}

void DatabaseWidget::refreshSearch()
{
    if (isSearchActive()) {
        search(m_lastSearchText);
    }
}

void DatabaseWidget::search(const QString& searchtext)
{
    if (searchtext.isEmpty()) {
        endSearch();
        return;
    }

    emit searchModeAboutToActivate();

    Group* searchGroup = m_searchLimitGroup ? currentGroup() : m_db->rootGroup();

    QList<Entry*> searchResult = m_entrySearcher->search(searchtext, searchGroup);

    m_entryView->displaySearch(searchResult);
    m_lastSearchText = searchtext;

    // Display a label detailing our search results
    if (!searchResult.isEmpty()) {
        m_searchingLabel->setText(tr("Search Results (%1)").arg(searchResult.size()));
    } else {
        m_searchingLabel->setText(tr("No Results"));
    }

    m_searchingLabel->setVisible(true);
#ifdef WITH_XC_KEESHARE
    m_shareLabel->setVisible(false);
#endif

    emit searchModeActivated();
}

void DatabaseWidget::setSearchCaseSensitive(bool state)
{
    m_entrySearcher->setCaseSensitive(state);
    refreshSearch();
}

void DatabaseWidget::setSearchLimitGroup(bool state)
{
    m_searchLimitGroup = state;
    refreshSearch();
}

void DatabaseWidget::onGroupChanged()
{
    auto group = m_groupView->currentGroup();

    // Intercept group changes if in search mode
    if (isSearchActive() && m_searchLimitGroup) {
        search(m_lastSearchText);
    } else {
        endSearch();
        m_entryView->displayGroup(group);
    }

    m_previewView->setGroup(group);

#ifdef WITH_XC_KEESHARE
    auto shareLabel = KeeShare::sharingLabel(group);
    if (!shareLabel.isEmpty()) {
        m_shareLabel->setText(shareLabel);
        m_shareLabel->setVisible(true);
    } else {
        m_shareLabel->setVisible(false);
    }
#endif
}

void DatabaseWidget::onDatabaseModified()
{
    if (!m_blockAutoSave && config()->get(Config::AutoSaveAfterEveryChange).toBool() && !m_db->isReadOnly()) {
        save();
    } else {
        // Only block once, then reset
        m_blockAutoSave = false;
    }
}

QString DatabaseWidget::getCurrentSearch()
{
    return m_lastSearchText;
}

void DatabaseWidget::endSearch()
{
    if (isSearchActive()) {
        // Show the normal entry view of the current group
        emit listModeAboutToActivate();
        m_entryView->displayGroup(currentGroup());
        emit listModeActivated();
        m_entryView->setFirstEntryActive();
        // Enforce preview view update (prevents stale information if focus group is empty)
        m_previewView->setEntry(currentSelectedEntry());
    }

    m_searchingLabel->setVisible(false);
    m_searchingLabel->setText(tr("Searching…"));

    m_lastSearchText.clear();

    // Tell the search widget to clear
    emit clearSearch();
}

void DatabaseWidget::emitGroupContextMenuRequested(const QPoint& pos)
{
    emit groupContextMenuRequested(m_groupView->viewport()->mapToGlobal(pos));
}

void DatabaseWidget::emitEntryContextMenuRequested(const QPoint& pos)
{
    emit entryContextMenuRequested(m_entryView->viewport()->mapToGlobal(pos));
}

void DatabaseWidget::onEntryChanged(Entry* entry)
{
    if (entry) {
        m_previewView->setEntry(entry);
    }

    emit entrySelectionChanged();
}

bool DatabaseWidget::canDeleteCurrentGroup() const
{
    bool isRootGroup = m_db->rootGroup() == m_groupView->currentGroup();
    return !isRootGroup;
}

Group* DatabaseWidget::currentGroup() const
{
    return m_groupView->currentGroup();
}

void DatabaseWidget::closeEvent(QCloseEvent* event)
{
    if (!isLocked() && !lock()) {
        event->ignore();
        return;
    }

    event->accept();
}

void DatabaseWidget::showEvent(QShowEvent* event)
{
    if (!m_db->isInitialized() || isLocked()) {
        switchToOpenDatabase();
    }

    event->accept();
}

bool DatabaseWidget::focusNextPrevChild(bool next)
{
    // [parent] <-> GroupView <-> EntryView <-> EntryPreview <-> [parent]
    if (next) {
        if (m_groupView->hasFocus()) {
            m_entryView->setFocus();
            return true;
        } else if (m_entryView->hasFocus()) {
            m_previewView->setFocus();
            return true;
        }
    } else {
        if (m_previewView->hasFocus()) {
            m_entryView->setFocus();
            return true;
        } else if (m_entryView->hasFocus()) {
            m_groupView->setFocus();
            return true;
        }
    }

    // Defer to the parent widget to make a decision
    return QStackedWidget::focusNextPrevChild(next);
}

bool DatabaseWidget::lock()
{
    if (isLocked()) {
        return true;
    }

    // Don't try to lock the database while saving, this will cause a deadlock
    if (m_db->isSaving()) {
        QTimer::singleShot(200, this, SLOT(lock()));
        return false;
    }

    emit databaseLockRequested();

    // ignore event if we are active and a modal dialog is still open (such as a message box or file dialog)
    if (isVisible() && QApplication::activeModalWidget()) {
        return false;
    }

    clipboard()->clearCopiedText();

    if (isEditWidgetModified()) {
        auto result = MessageBox::question(this,
                                           tr("Lock Database?"),
                                           tr("You are editing an entry. Discard changes and lock anyway?"),
                                           MessageBox::Discard | MessageBox::Cancel,
                                           MessageBox::Cancel);
        if (result == MessageBox::Cancel) {
            return false;
        }
    }

    if (m_db->isModified()) {
        bool saved = false;
        // Attempt to save on exit, but don't block locking if it fails
        if (config()->get(Config::AutoSaveOnExit).toBool()
            || config()->get(Config::AutoSaveAfterEveryChange).toBool()) {
            saved = save();
        }

        if (!saved) {
            QString msg;
            if (!m_db->metadata()->name().toHtmlEscaped().isEmpty()) {
                msg = tr("\"%1\" was modified.\nSave changes?").arg(m_db->metadata()->name().toHtmlEscaped());
            } else {
                msg = tr("Database was modified.\nSave changes?");
            }
            auto result = MessageBox::question(this,
                                               tr("Save changes?"),
                                               msg,
                                               MessageBox::Save | MessageBox::Discard | MessageBox::Cancel,
                                               MessageBox::Save);
            if (result == MessageBox::Save) {
                if (!save()) {
                    return false;
                }
            } else if (result == MessageBox::Cancel) {
                return false;
            }
        }
    } else if (m_db->hasNonDataChanges() && config()->get(Config::AutoSaveNonDataChanges).toBool()) {
        // Silently auto-save non-data changes, ignore errors
        QString errorMessage;
        performSave(errorMessage);
    }

    if (m_groupView->currentGroup()) {
        m_groupBeforeLock = m_groupView->currentGroup()->uuid();
    } else {
        m_groupBeforeLock = m_db->rootGroup()->uuid();
    }

    auto currentEntry = currentSelectedEntry();
    if (currentEntry) {
        m_entryBeforeLock = currentEntry->uuid();
    }

#ifdef WITH_XC_SSHAGENT
    sshAgent()->databaseLocked(m_db);
#endif

    endSearch();
    clearAllWidgets();
    switchToOpenDatabase(m_db->filePath());

    auto newDb = QSharedPointer<Database>::create(m_db->filePath());
    replaceDatabase(newDb);

    emit databaseLocked();

    return true;
}

void DatabaseWidget::reloadDatabaseFile()
{
    // Ignore reload if we are locked or currently editing an entry or group
    if (!m_db || isLocked() || isEntryEditActive() || isGroupEditActive()) {
        return;
    }

    m_blockAutoSave = true;

    if (!config()->get(Config::AutoReloadOnChange).toBool()) {
        // Ask if we want to reload the db
        auto result = MessageBox::question(this,
                                           tr("File has changed"),
                                           tr("The database file has changed. Do you want to load the changes?"),
                                           MessageBox::Yes | MessageBox::No);

        if (result == MessageBox::No) {
            // Notify everyone the database does not match the file
            m_db->markAsModified();
            return;
        }
    }

    // Lock out interactions
    m_entryView->setDisabled(true);
    m_groupView->setDisabled(true);
    QApplication::processEvents();

    QString error;
    auto db = QSharedPointer<Database>::create(m_db->filePath());
    if (db->open(database()->key(), &error)) {
        if (m_db->isModified() || db->hasNonDataChanges()) {
            // Ask if we want to merge changes into new database
            auto result = MessageBox::question(
                this,
                tr("Merge Request"),
                tr("The database file has changed and you have unsaved changes.\nDo you want to merge your changes?"),
                MessageBox::Merge | MessageBox::Discard,
                MessageBox::Merge);

            if (result == MessageBox::Merge) {
                // Merge the old database into the new one
                Merger merger(m_db.data(), db.data());
                merger.merge();
            }
        }

        QUuid groupBeforeReload = m_db->rootGroup()->uuid();
        if (m_groupView && m_groupView->currentGroup()) {
            groupBeforeReload = m_groupView->currentGroup()->uuid();
        }

        QUuid entryBeforeReload;
        if (m_entryView && m_entryView->currentEntry()) {
            entryBeforeReload = m_entryView->currentEntry()->uuid();
        }

        replaceDatabase(db);
        processAutoOpen();
        restoreGroupEntryFocus(groupBeforeReload, entryBeforeReload);
        m_blockAutoSave = false;
    } else {
        showMessage(tr("Could not open the new database file while attempting to autoreload.\nError: %1").arg(error),
                    MessageWidget::Error);
        // Mark db as modified since existing data may differ from file or file was deleted
        m_db->markAsModified();
    }

    // Return control
    m_entryView->setDisabled(false);
    m_groupView->setDisabled(false);
}

int DatabaseWidget::numberOfSelectedEntries() const
{
    return m_entryView->numberOfSelectedEntries();
}

int DatabaseWidget::currentEntryIndex() const
{
    return m_entryView->currentEntryIndex();
}

QStringList DatabaseWidget::customEntryAttributes() const
{
    Entry* entry = m_entryView->currentEntry();
    if (!entry) {
        return QStringList();
    }

    return entry->attributes()->customKeys();
}

/*
 * Restores the focus on the group and entry provided
 */
void DatabaseWidget::restoreGroupEntryFocus(const QUuid& groupUuid, const QUuid& entryUuid)
{
    auto group = m_db->rootGroup()->findGroupByUuid(groupUuid);
    if (group) {
        m_groupView->setCurrentGroup(group);
        auto entry = group->findEntryByUuid(entryUuid, false);
        if (entry) {
            m_entryView->setCurrentEntry(entry);
        }
    }
}

bool DatabaseWidget::isGroupSelected() const
{
    return m_groupView->currentGroup();
}

bool DatabaseWidget::currentEntryHasTitle()
{
    auto currentEntry = currentSelectedEntry();
    Q_ASSERT(currentEntry);
    if (!currentEntry) {
        return false;
    }
    return !currentEntry->title().isEmpty();
}

bool DatabaseWidget::currentEntryHasUsername()
{
    auto currentEntry = currentSelectedEntry();
    Q_ASSERT(currentEntry);
    if (!currentEntry) {
        return false;
    }
    return !currentEntry->resolveMultiplePlaceholders(currentEntry->username()).isEmpty();
}

bool DatabaseWidget::currentEntryHasPassword()
{
    auto currentEntry = currentSelectedEntry();
    Q_ASSERT(currentEntry);
    if (!currentEntry) {
        return false;
    }
    return !currentEntry->resolveMultiplePlaceholders(currentEntry->password()).isEmpty();
}

bool DatabaseWidget::currentEntryHasUrl()
{
    auto currentEntry = currentSelectedEntry();
    Q_ASSERT(currentEntry);
    if (!currentEntry) {
        return false;
    }
    return !currentEntry->resolveMultiplePlaceholders(currentEntry->url()).isEmpty();
}

bool DatabaseWidget::currentEntryHasTotp()
{
    auto currentEntry = currentSelectedEntry();
    Q_ASSERT(currentEntry);
    if (!currentEntry) {
        return false;
    }
    return currentEntry->hasTotp();
}

#ifdef WITH_XC_SSHAGENT
bool DatabaseWidget::currentEntryHasSshKey()
{
    Entry* currentEntry = m_entryView->currentEntry();
    Q_ASSERT(currentEntry);
    if (!currentEntry) {
        return false;
    }

    return KeeAgentSettings::inEntryAttachments(currentEntry->attachments());
}
#endif

bool DatabaseWidget::currentEntryHasNotes()
{
    auto currentEntry = currentSelectedEntry();
    Q_ASSERT(currentEntry);
    if (!currentEntry) {
        return false;
    }
    return !currentEntry->resolveMultiplePlaceholders(currentEntry->notes()).isEmpty();
}

GroupView* DatabaseWidget::groupView()
{
    return m_groupView;
}

EntryView* DatabaseWidget::entryView()
{
    return m_entryView;
}

/**
 * Save the database to disk.
 *
 * This method will try to save several times in case of failure and
 * ask to disable safe saves if it is unable to save after the third attempt.
 * Set `attempt` to -1 to disable this behavior.
 *
 * @return true on success
 */
bool DatabaseWidget::save()
{
    // Never allow saving a locked database; it causes corruption
    Q_ASSERT(!isLocked());
    // Release build interlock
    if (isLocked()) {
        // We return true since a save is not required
        return true;
    }

    // Read-only and new databases ask for filename
    if (m_db->isReadOnly() || m_db->filePath().isEmpty()) {
        return saveAs();
    }

    // Prevent recursions and infinite save loops
    m_blockAutoSave = true;
    ++m_saveAttempts;

    QString errorMessage;
    if (performSave(errorMessage)) {
        m_saveAttempts = 0;
        m_blockAutoSave = false;
        return true;
    }

    if (m_saveAttempts > 2 && config()->get(Config::UseAtomicSaves).toBool()) {
        // Saving failed 3 times, issue a warning and attempt to resolve
        auto result = MessageBox::question(this,
                                           tr("Disable safe saves?"),
                                           tr("KeePassXC has failed to save the database multiple times. "
                                              "This is likely caused by file sync services holding a lock on "
                                              "the save file.\nDisable safe saves and try again?"),
                                           MessageBox::Disable | MessageBox::Cancel,
                                           MessageBox::Disable);
        if (result == MessageBox::Disable) {
            config()->set(Config::UseAtomicSaves, false);
            return save();
        }
    }

    showMessage(tr("Writing the database failed: %1").arg(errorMessage),
                MessageWidget::Error,
                true,
                MessageWidget::LongAutoHideTimeout);

    return false;
}

/**
 * Save database under a new user-selected filename.
 *
 * @return true on success
 */
bool DatabaseWidget::saveAs()
{
    // Never allow saving a locked database; it causes corruption
    Q_ASSERT(!isLocked());
    // Release build interlock
    if (isLocked()) {
        // We return true since a save is not required
        return true;
    }

    QString oldFilePath = m_db->filePath();
    if (!QFileInfo::exists(oldFilePath)) {
        oldFilePath =
            QDir::toNativeSeparators(config()->get(Config::LastDir).toString() + "/" + tr("Passwords").append(".kdbx"));
    }
    const QString newFilePath = fileDialog()->getSaveFileName(
        this, tr("Save database as"), oldFilePath, tr("KeePass 2 Database").append(" (*.kdbx)"), nullptr, nullptr);

    bool ok = false;
    if (!newFilePath.isEmpty()) {
        QString errorMessage;
        if (!performSave(errorMessage, newFilePath)) {
            showMessage(tr("Writing the database failed: %1").arg(errorMessage),
                        MessageWidget::Error,
                        true,
                        MessageWidget::LongAutoHideTimeout);
        }
    }

    return ok;
}

bool DatabaseWidget::performSave(QString& errorMessage, const QString& fileName)
{
    QPointer<QWidget> focusWidget(qApp->focusWidget());

    // Lock out interactions
    m_entryView->setDisabled(true);
    m_groupView->setDisabled(true);
    QApplication::processEvents();

    bool ok;
    if (fileName.isEmpty()) {
        ok = m_db->save(&errorMessage,
                        config()->get(Config::UseAtomicSaves).toBool(),
                        config()->get(Config::BackupBeforeSave).toBool());
    } else {
        ok = m_db->saveAs(fileName,
                          &errorMessage,
                          config()->get(Config::UseAtomicSaves).toBool(),
                          config()->get(Config::BackupBeforeSave).toBool());
    }

    // Return control
    m_entryView->setDisabled(false);
    m_groupView->setDisabled(false);

    if (focusWidget) {
        focusWidget->setFocus();
    }

    return ok;
}

/**
 * Save copy of database under a new user-selected filename.
 *
 * @return true on success
 */
bool DatabaseWidget::saveBackup()
{
    while (true) {
        QString oldFilePath = m_db->filePath();
        if (!QFileInfo::exists(oldFilePath)) {
            oldFilePath = QDir::toNativeSeparators(config()->get(Config::LastDir).toString() + "/"
                                                   + tr("Passwords").append(".kdbx"));
        }
        const QString newFilePath = fileDialog()->getSaveFileName(this,
                                                                  tr("Save database backup"),
                                                                  oldFilePath,
                                                                  tr("KeePass 2 Database").append(" (*.kdbx)"),
                                                                  nullptr,
                                                                  nullptr);

        if (!newFilePath.isEmpty()) {
            // Ensure we don't recurse back into this function
            m_db->setReadOnly(false);
            m_db->setFilePath(newFilePath);
            m_saveAttempts = 0;

            bool modified = m_db->isModified();

            if (!save()) {
                // Failed to save, try again
                m_db->setFilePath(oldFilePath);
                continue;
            }

            m_db->setFilePath(oldFilePath);
            if (modified) {
                // Source database is marked as clean when copy is saved, even if source has unsaved changes
                m_db->markAsModified();
            }
            return true;
        }

        // Canceled file selection
        return false;
    }
}

void DatabaseWidget::showMessage(const QString& text,
                                 MessageWidget::MessageType type,
                                 bool showClosebutton,
                                 int autoHideTimeout)
{
    m_messageWidget->setCloseButtonVisible(showClosebutton);
    m_messageWidget->showMessage(text, type, autoHideTimeout);
}

void DatabaseWidget::showErrorMessage(const QString& errorMessage)
{
    showMessage(errorMessage, MessageWidget::MessageType::Error);
}

void DatabaseWidget::hideMessage()
{
    if (m_messageWidget->isVisible()) {
        m_messageWidget->animatedHide();
    }
}

bool DatabaseWidget::isRecycleBinSelected() const
{
    return m_groupView->currentGroup() && m_groupView->currentGroup() == m_db->metadata()->recycleBin();
}

void DatabaseWidget::emptyRecycleBin()
{
    if (!isRecycleBinSelected()) {
        return;
    }

    auto result =
        MessageBox::question(this,
                             tr("Empty recycle bin?"),
                             tr("Are you sure you want to permanently delete everything from your recycle bin?"),
                             MessageBox::Empty | MessageBox::Cancel,
                             MessageBox::Cancel);

    if (result == MessageBox::Empty) {
        m_db->emptyRecycleBin();
        refreshSearch();
    }
}

void DatabaseWidget::processAutoOpen()
{
    Q_ASSERT(m_db);

    auto* autoopenGroup = m_db->rootGroup()->findGroupByPath("/AutoOpen");
    if (!autoopenGroup) {
        return;
    }

    for (const auto* entry : autoopenGroup->entries()) {
        if (entry->url().isEmpty() || (entry->password().isEmpty() && entry->username().isEmpty())) {
            continue;
        }

        // Support ifDevice advanced entry, a comma separated list of computer names
        // that control whether to perform AutoOpen on this entry or not. Can be
        // negated using '!'
        auto ifDevice = entry->attribute("IfDevice");
        if (!ifDevice.isEmpty()) {
            bool loadDb = false;
            auto hostName = QHostInfo::localHostName();
            for (auto& device : ifDevice.split(",")) {
                device = device.trimmed();
                if (device.startsWith("!")) {
                    if (device.mid(1).compare(hostName, Qt::CaseInsensitive) == 0) {
                        // Machine name matched an exclusion, don't load this database
                        loadDb = false;
                        break;
                    } else {
                        // Not matching an exclusion allows loading on all machines
                        loadDb = true;
                    }
                } else if (device.compare(hostName, Qt::CaseInsensitive) == 0) {
                    // Explicitly named for loading
                    loadDb = true;
                }
            }
            if (!loadDb) {
                continue;
            }
        }

        openDatabaseFromEntry(entry);
    }
}

void DatabaseWidget::openDatabaseFromEntry(const Entry* entry, bool inBackground)
{
    auto keyFile = entry->resolveMultiplePlaceholders(entry->username());
    auto password = entry->resolveMultiplePlaceholders(entry->password());
    auto databaseUrl = entry->resolveMultiplePlaceholders(entry->url());
    if (databaseUrl.startsWith("kdbx://")) {
        databaseUrl = databaseUrl.mid(7);
    }

    QFileInfo dbFileInfo;
    if (databaseUrl.startsWith("file://")) {
        QUrl url(databaseUrl);
        dbFileInfo.setFile(url.toLocalFile());
    } else {
        dbFileInfo.setFile(databaseUrl);
        if (dbFileInfo.isRelative()) {
            QFileInfo currentpath(m_db->filePath());
            dbFileInfo.setFile(currentpath.absoluteDir(), databaseUrl);
        }
    }

    if (!dbFileInfo.isFile()) {
        showErrorMessage(tr("Could not find database file: %1").arg(databaseUrl));
        return;
    }

    QFileInfo keyFileInfo;
    if (!keyFile.isEmpty()) {
        if (keyFile.startsWith("file://")) {
            QUrl keyfileUrl(keyFile);
            keyFileInfo.setFile(keyfileUrl.toLocalFile());
        } else {
            keyFileInfo.setFile(keyFile);
            if (keyFileInfo.isRelative()) {
                QFileInfo currentpath(m_db->filePath());
                keyFileInfo.setFile(currentpath.absoluteDir(), keyFile);
            }
        }
    }

    // Request to open the database file in the background with a password and keyfile
    emit requestOpenDatabase(dbFileInfo.canonicalFilePath(), inBackground, password, keyFileInfo.canonicalFilePath());
}
