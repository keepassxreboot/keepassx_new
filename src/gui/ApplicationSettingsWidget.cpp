/*
 *  Copyright (C) 2012 Felix Geyer <debfx@fobos.de>
 *  Copyright (C) 2017 KeePassXC Team <team@keepassxc.org>
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

#include "ApplicationSettingsWidget.h"
#include "ui_ApplicationSettingsWidgetGeneral.h"
#include "ui_ApplicationSettingsWidgetSecurity.h"

#include "config-keepassx.h"

#include "autotype/AutoType.h"
#include "core/Config.h"
#include "core/Global.h"
#include "core/Translator.h"
#include "gui/Icons.h"
#include "gui/MainWindow.h"
#include "gui/osutils/OSUtils.h"

#include "MessageBox.h"
#include "touchid/TouchID.h"

#ifdef WITH_XC_WINDOWSHELLO
#include "winhello/WindowsHello.h"
#endif

class ApplicationSettingsWidget::ExtraPage
{
public:
    ExtraPage(ISettingsPage* page, QWidget* widget)
        : settingsPage(page)
        , widget(widget)
    {
    }

    void loadSettings() const
    {
        settingsPage->loadSettings(widget);
    }

    void saveSettings() const
    {
        settingsPage->saveSettings(widget);
    }

private:
    QSharedPointer<ISettingsPage> settingsPage;
    QWidget* widget;
};

/**
 * Helper class to ignore mouse wheel events on non-focused widgets
 * NOTE: The widget must NOT have a focus policy of "WHEEL"
 */
class MouseWheelEventFilter : public QObject
{
public:
    explicit MouseWheelEventFilter(QObject* parent)
        : QObject(parent){};

protected:
    bool eventFilter(QObject* obj, QEvent* event) override
    {
        const auto* widget = qobject_cast<QWidget*>(obj);
        if (event->type() == QEvent::Wheel && widget && !widget->hasFocus()) {
            event->ignore();
            return true;
        }
        return QObject::eventFilter(obj, event);
    }
};

ApplicationSettingsWidget::ApplicationSettingsWidget(QWidget* parent)
    : EditWidget(parent)
    , m_secWidget(new QWidget())
    , m_generalWidget(new QWidget())
    , m_secUi(new Ui::ApplicationSettingsWidgetSecurity())
    , m_generalUi(new Ui::ApplicationSettingsWidgetGeneral())
    , m_globalAutoTypeKey(static_cast<Qt::Key>(0))
    , m_globalAutoTypeModifiers(Qt::NoModifier)
{
    setHeadline(tr("Application Settings"));
    showApplyButton(false);

    m_secUi->setupUi(m_secWidget);
    m_generalUi->setupUi(m_generalWidget);
    addPage(tr("General"), icons()->icon("preferences-other"), m_generalWidget);
    addPage(tr("Security"), icons()->icon("security-high"), m_secWidget);

    if (!autoType()->isAvailable()) {
        m_generalUi->generalSettingsTabWidget->removeTab(1);
    }

    connect(this, SIGNAL(accepted()), SLOT(saveSettings()));
    connect(this, SIGNAL(rejected()), SLOT(reject()));

    // clang-format off
    connect(m_generalUi->autoSaveAfterEveryChangeCheckBox, SIGNAL(toggled(bool)), SLOT(autoSaveToggled(bool)));
    connect(m_generalUi->hideWindowOnCopyCheckBox, SIGNAL(toggled(bool)), SLOT(hideWindowOnCopyCheckBoxToggled(bool)));
    connect(m_generalUi->systrayShowCheckBox, SIGNAL(toggled(bool)), SLOT(systrayToggled(bool)));
    connect(m_generalUi->rememberLastDatabasesCheckBox, SIGNAL(toggled(bool)), SLOT(rememberDatabasesToggled(bool)));
    connect(m_generalUi->resetSettingsButton, SIGNAL(clicked()), SLOT(resetSettings()));

    connect(m_secUi->clearClipboardCheckBox, SIGNAL(toggled(bool)),
            m_secUi->clearClipboardSpinBox, SLOT(setEnabled(bool)));
    connect(m_secUi->clearSearchCheckBox, SIGNAL(toggled(bool)),
            m_secUi->clearSearchSpinBox, SLOT(setEnabled(bool)));
    connect(m_secUi->lockDatabaseIdleCheckBox, SIGNAL(toggled(bool)),
            m_secUi->lockDatabaseIdleSpinBox, SLOT(setEnabled(bool)));
    connect(m_secUi->touchIDResetCheckBox, SIGNAL(toggled(bool)),
            m_secUi->touchIDResetSpinBox, SLOT(setEnabled(bool)));
    // clang-format on

    // Disable mouse wheel grab when scrolling
    // This prevents combo box and spinner values from changing without explicit focus
    auto mouseWheelFilter = new MouseWheelEventFilter(this);
    m_generalUi->faviconTimeoutSpinBox->installEventFilter(mouseWheelFilter);
    m_generalUi->toolButtonStyleComboBox->installEventFilter(mouseWheelFilter);
    m_generalUi->languageComboBox->installEventFilter(mouseWheelFilter);
    m_generalUi->trayIconAppearance->installEventFilter(mouseWheelFilter);

#ifdef WITH_XC_UPDATECHECK
    connect(m_generalUi->checkForUpdatesOnStartupCheckBox, SIGNAL(toggled(bool)), SLOT(checkUpdatesToggled(bool)));
#else
    m_generalUi->checkForUpdatesOnStartupCheckBox->setVisible(false);
    m_generalUi->checkForUpdatesIncludeBetasCheckBox->setVisible(false);
    m_generalUi->checkUpdatesSpacer->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
#endif

#ifndef WITH_XC_NETWORKING
    m_secUi->privacy->setVisible(false);
    m_generalUi->faviconTimeoutLabel->setVisible(false);
    m_generalUi->faviconTimeoutSpinBox->setVisible(false);
#endif

m_secUi->winHelloBox->setVisible(false);
#ifdef WITH_XC_WINDOWSHELLO
    if(WindowsHello::isAvailable()) {
        m_secUi->winHelloBox->setVisible(true);
        connect(m_secUi->winHelloResetButton, &QPushButton::clicked, [this](){
            auto selectedButton = MessageBox::warning(
                this,
                tr("Reset Windows Hello"),
                tr("You are about to remove all stored database keys\n"
                   "from Windows Hello storage.\n\n"
                   "Do you want to continue?"
                ),
                MessageBox::Yes | MessageBox::No
            );
            if (selectedButton == MessageBox::Yes) {
                WindowsHello(this).reset();
            }
        });
    }
#endif

#ifndef WITH_XC_TOUCHID
    bool hideTouchID = true;
#else
    bool hideTouchID = !TouchID::getInstance().isAvailable();
#endif
    if (hideTouchID) {
        m_secUi->touchIDResetCheckBox->setVisible(false);
        m_secUi->touchIDResetSpinBox->setVisible(false);
        m_secUi->touchIDResetOnScreenLockCheckBox->setVisible(false);
    }
}

ApplicationSettingsWidget::~ApplicationSettingsWidget()
{
}

void ApplicationSettingsWidget::addSettingsPage(ISettingsPage* page)
{
    QWidget* widget = page->createWidget();
    widget->setParent(this);
    m_extraPages.append(ExtraPage(page, widget));
    addPage(page->name(), page->icon(), widget);
}

void ApplicationSettingsWidget::loadSettings()
{
    if (config()->hasAccessError()) {
        showMessage(tr("Access error for config file %1").arg(config()->getFileName()), MessageWidget::Error);
    }

#ifdef QT_DEBUG
    m_generalUi->singleInstanceCheckBox->setEnabled(false);
    m_generalUi->launchAtStartup->setEnabled(false);
#endif
    m_generalUi->singleInstanceCheckBox->setChecked(config()->get(Config::SingleInstance).toBool());
    m_generalUi->launchAtStartup->setChecked(osUtils->isLaunchAtStartupEnabled());
    m_generalUi->rememberLastDatabasesCheckBox->setChecked(config()->get(Config::RememberLastDatabases).toBool());
    m_generalUi->rememberLastKeyFilesCheckBox->setChecked(config()->get(Config::RememberLastKeyFiles).toBool());
    m_generalUi->openPreviousDatabasesOnStartupCheckBox->setChecked(
        config()->get(Config::OpenPreviousDatabasesOnStartup).toBool());
    m_generalUi->autoSaveAfterEveryChangeCheckBox->setChecked(config()->get(Config::AutoSaveAfterEveryChange).toBool());
    m_generalUi->autoSaveOnExitCheckBox->setChecked(config()->get(Config::AutoSaveOnExit).toBool());
    m_generalUi->autoSaveNonDataChangesCheckBox->setChecked(config()->get(Config::AutoSaveNonDataChanges).toBool());
    m_generalUi->backupBeforeSaveCheckBox->setChecked(config()->get(Config::BackupBeforeSave).toBool());
    m_generalUi->useAtomicSavesCheckBox->setChecked(config()->get(Config::UseAtomicSaves).toBool());
    m_generalUi->autoReloadOnChangeCheckBox->setChecked(config()->get(Config::AutoReloadOnChange).toBool());
    m_generalUi->minimizeAfterUnlockCheckBox->setChecked(config()->get(Config::MinimizeAfterUnlock).toBool());
    m_generalUi->minimizeOnOpenUrlCheckBox->setChecked(config()->get(Config::MinimizeOnOpenUrl).toBool());
    m_generalUi->hideWindowOnCopyCheckBox->setChecked(config()->get(Config::HideWindowOnCopy).toBool());
    hideWindowOnCopyCheckBoxToggled(m_generalUi->hideWindowOnCopyCheckBox->isChecked());
    m_generalUi->minimizeOnCopyRadioButton->setChecked(config()->get(Config::MinimizeOnCopy).toBool());
    m_generalUi->dropToBackgroundOnCopyRadioButton->setChecked(config()->get(Config::DropToBackgroundOnCopy).toBool());
    m_generalUi->useGroupIconOnEntryCreationCheckBox->setChecked(
        config()->get(Config::UseGroupIconOnEntryCreation).toBool());
    m_generalUi->autoTypeEntryTitleMatchCheckBox->setChecked(config()->get(Config::AutoTypeEntryTitleMatch).toBool());
    m_generalUi->autoTypeEntryURLMatchCheckBox->setChecked(config()->get(Config::AutoTypeEntryURLMatch).toBool());
    m_generalUi->autoTypeHideExpiredEntryCheckBox->setChecked(config()->get(Config::AutoTypeHideExpiredEntry).toBool());
    m_generalUi->faviconTimeoutSpinBox->setValue(config()->get(Config::FaviconDownloadTimeout).toInt());

    m_generalUi->languageComboBox->clear();
    QList<QPair<QString, QString>> languages = Translator::availableLanguages();
    for (const auto& language : languages) {
        m_generalUi->languageComboBox->addItem(language.second, language.first);
    }
    int defaultIndex = m_generalUi->languageComboBox->findData(config()->get(Config::GUI_Language));
    if (defaultIndex > 0) {
        m_generalUi->languageComboBox->setCurrentIndex(defaultIndex);
    }

    m_generalUi->toolbarMovableCheckBox->setChecked(config()->get(Config::GUI_MovableToolbar).toBool());
    m_generalUi->monospaceNotesCheckBox->setChecked(config()->get(Config::GUI_MonospaceNotes).toBool());

    m_generalUi->toolButtonStyleComboBox->clear();
    m_generalUi->toolButtonStyleComboBox->addItem(tr("Icon only"), Qt::ToolButtonIconOnly);
    m_generalUi->toolButtonStyleComboBox->addItem(tr("Text only"), Qt::ToolButtonTextOnly);
    m_generalUi->toolButtonStyleComboBox->addItem(tr("Text beside icon"), Qt::ToolButtonTextBesideIcon);
    m_generalUi->toolButtonStyleComboBox->addItem(tr("Text under icon"), Qt::ToolButtonTextUnderIcon);
    m_generalUi->toolButtonStyleComboBox->addItem(tr("Follow style"), Qt::ToolButtonFollowStyle);
    int toolButtonStyleIndex =
        m_generalUi->toolButtonStyleComboBox->findData(config()->get(Config::GUI_ToolButtonStyle));
    if (toolButtonStyleIndex > 0) {
        m_generalUi->toolButtonStyleComboBox->setCurrentIndex(toolButtonStyleIndex);
    }

    m_generalUi->systrayShowCheckBox->setChecked(config()->get(Config::GUI_ShowTrayIcon).toBool());
    systrayToggled(m_generalUi->systrayShowCheckBox->isChecked());
    m_generalUi->systrayMinimizeToTrayCheckBox->setChecked(config()->get(Config::GUI_MinimizeToTray).toBool());
    m_generalUi->minimizeOnCloseCheckBox->setChecked(config()->get(Config::GUI_MinimizeOnClose).toBool());
    m_generalUi->systrayMinimizeOnStartup->setChecked(config()->get(Config::GUI_MinimizeOnStartup).toBool());
    m_generalUi->checkForUpdatesOnStartupCheckBox->setChecked(config()->get(Config::GUI_CheckForUpdates).toBool());
    checkUpdatesToggled(m_generalUi->checkForUpdatesOnStartupCheckBox->isChecked());
    m_generalUi->checkForUpdatesIncludeBetasCheckBox->setChecked(
        config()->get(Config::GUI_CheckForUpdatesIncludeBetas).toBool());

    m_generalUi->autoTypeAskCheckBox->setChecked(config()->get(Config::Security_AutoTypeAsk).toBool());

    if (autoType()->isAvailable()) {
        m_globalAutoTypeKey = static_cast<Qt::Key>(config()->get(Config::GlobalAutoTypeKey).toInt());
        m_globalAutoTypeModifiers =
            static_cast<Qt::KeyboardModifiers>(config()->get(Config::GlobalAutoTypeModifiers).toInt());
        if (m_globalAutoTypeKey > 0 && m_globalAutoTypeModifiers > 0) {
            m_generalUi->autoTypeShortcutWidget->setShortcut(m_globalAutoTypeKey, m_globalAutoTypeModifiers);
        }
        m_generalUi->autoTypeShortcutWidget->setAttribute(Qt::WA_MacShowFocusRect, true);
        m_generalUi->autoTypeDelaySpinBox->setValue(config()->get(Config::AutoTypeDelay).toInt());
        m_generalUi->autoTypeStartDelaySpinBox->setValue(config()->get(Config::AutoTypeStartDelay).toInt());
    }

    m_generalUi->trayIconAppearance->clear();
#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
    m_generalUi->trayIconAppearance->addItem(tr("Monochrome"), "monochrome");
#else
    m_generalUi->trayIconAppearance->addItem(tr("Monochrome (light)"), "monochrome-light");
    m_generalUi->trayIconAppearance->addItem(tr("Monochrome (dark)"), "monochrome-dark");
#endif
    m_generalUi->trayIconAppearance->addItem(tr("Colorful"), "colorful");
    int trayIconIndex = m_generalUi->trayIconAppearance->findData(icons()->trayIconAppearance());
    if (trayIconIndex > 0) {
        m_generalUi->trayIconAppearance->setCurrentIndex(trayIconIndex);
    }

    m_secUi->clearClipboardCheckBox->setChecked(config()->get(Config::Security_ClearClipboard).toBool());
    m_secUi->clearClipboardSpinBox->setValue(config()->get(Config::Security_ClearClipboardTimeout).toInt());

    m_secUi->clearSearchCheckBox->setChecked(config()->get(Config::Security_ClearSearch).toBool());
    m_secUi->clearSearchSpinBox->setValue(config()->get(Config::Security_ClearSearchTimeout).toInt());

    m_secUi->lockDatabaseIdleCheckBox->setChecked(config()->get(Config::Security_LockDatabaseIdle).toBool());
    m_secUi->lockDatabaseIdleSpinBox->setValue(config()->get(Config::Security_LockDatabaseIdleSeconds).toInt());
    m_secUi->lockDatabaseMinimizeCheckBox->setChecked(config()->get(Config::Security_LockDatabaseMinimize).toBool());
    m_secUi->lockDatabaseOnScreenLockCheckBox->setChecked(
        config()->get(Config::Security_LockDatabaseScreenLock).toBool());
    m_secUi->relockDatabaseAutoTypeCheckBox->setChecked(config()->get(Config::Security_RelockAutoType).toBool());
    m_secUi->fallbackToSearch->setChecked(config()->get(Config::Security_IconDownloadFallback).toBool());

    m_secUi->passwordsHiddenCheckBox->setChecked(config()->get(Config::Security_PasswordsHidden).toBool());
    m_secUi->passwordShowDotsCheckBox->setChecked(config()->get(Config::Security_PasswordEmptyPlaceholder).toBool());
    m_secUi->passwordPreviewCleartextCheckBox->setChecked(
        config()->get(Config::Security_HidePasswordPreviewPanel).toBool());
    m_secUi->passwordsRepeatVisibleCheckBox->setChecked(
        config()->get(Config::Security_PasswordsRepeatVisible).toBool());
    m_secUi->hideNotesCheckBox->setChecked(config()->get(Config::Security_HideNotes).toBool());
    m_secUi->NoConfirmMoveEntryToRecycleBinCheckBox->setChecked(
        config()->get(Config::Security_NoConfirmMoveEntryToRecycleBin).toBool());

    m_secUi->touchIDResetCheckBox->setChecked(config()->get(Config::Security_ResetTouchId).toBool());
    m_secUi->touchIDResetSpinBox->setValue(config()->get(Config::Security_ResetTouchIdTimeout).toInt());
    m_secUi->touchIDResetOnScreenLockCheckBox->setChecked(
        config()->get(Config::Security_ResetTouchIdScreenlock).toBool());

    for (const ExtraPage& page : asConst(m_extraPages)) {
        page.loadSettings();
    }

    setCurrentPage(0);
}

void ApplicationSettingsWidget::saveSettings()
{
    if (config()->hasAccessError()) {
        showMessage(tr("Access error for config file %1").arg(config()->getFileName()), MessageWidget::Error);
        // We prevent closing the settings page if we could not write to
        // the config file.
        return;
    }

#ifndef QT_DEBUG
    osUtils->setLaunchAtStartup(m_generalUi->launchAtStartup->isChecked());
#endif

    config()->set(Config::SingleInstance, m_generalUi->singleInstanceCheckBox->isChecked());
    config()->set(Config::RememberLastDatabases, m_generalUi->rememberLastDatabasesCheckBox->isChecked());
    config()->set(Config::RememberLastKeyFiles, m_generalUi->rememberLastKeyFilesCheckBox->isChecked());
    config()->set(Config::OpenPreviousDatabasesOnStartup,
                  m_generalUi->openPreviousDatabasesOnStartupCheckBox->isChecked());
    config()->set(Config::AutoSaveAfterEveryChange, m_generalUi->autoSaveAfterEveryChangeCheckBox->isChecked());
    config()->set(Config::AutoSaveOnExit, m_generalUi->autoSaveOnExitCheckBox->isChecked());
    config()->set(Config::AutoSaveNonDataChanges, m_generalUi->autoSaveNonDataChangesCheckBox->isChecked());
    config()->set(Config::BackupBeforeSave, m_generalUi->backupBeforeSaveCheckBox->isChecked());
    config()->set(Config::UseAtomicSaves, m_generalUi->useAtomicSavesCheckBox->isChecked());
    config()->set(Config::AutoReloadOnChange, m_generalUi->autoReloadOnChangeCheckBox->isChecked());
    config()->set(Config::MinimizeAfterUnlock, m_generalUi->minimizeAfterUnlockCheckBox->isChecked());
    config()->set(Config::MinimizeOnOpenUrl, m_generalUi->minimizeOnOpenUrlCheckBox->isChecked());
    config()->set(Config::HideWindowOnCopy, m_generalUi->hideWindowOnCopyCheckBox->isChecked());
    config()->set(Config::MinimizeOnCopy, m_generalUi->minimizeOnCopyRadioButton->isChecked());
    config()->set(Config::DropToBackgroundOnCopy, m_generalUi->dropToBackgroundOnCopyRadioButton->isChecked());
    config()->set(Config::UseGroupIconOnEntryCreation, m_generalUi->useGroupIconOnEntryCreationCheckBox->isChecked());
    config()->set(Config::AutoTypeEntryTitleMatch, m_generalUi->autoTypeEntryTitleMatchCheckBox->isChecked());
    config()->set(Config::AutoTypeEntryURLMatch, m_generalUi->autoTypeEntryURLMatchCheckBox->isChecked());
    config()->set(Config::AutoTypeHideExpiredEntry, m_generalUi->autoTypeHideExpiredEntryCheckBox->isChecked());
    config()->set(Config::FaviconDownloadTimeout, m_generalUi->faviconTimeoutSpinBox->value());

    auto language = m_generalUi->languageComboBox->currentData().toString();
    if (config()->get(Config::GUI_Language) != language) {
        QTimer::singleShot(200, [] {
            getMainWindow()->restartApp(
                tr("You must restart the application to set the new language. Would you like to restart now?"));
        });
    }
    config()->set(Config::GUI_Language, language);

    config()->set(Config::GUI_MovableToolbar, m_generalUi->toolbarMovableCheckBox->isChecked());
    config()->set(Config::GUI_MonospaceNotes, m_generalUi->monospaceNotesCheckBox->isChecked());

    config()->set(Config::GUI_ToolButtonStyle, m_generalUi->toolButtonStyleComboBox->currentData().toString());

    config()->set(Config::GUI_ShowTrayIcon, m_generalUi->systrayShowCheckBox->isChecked());
    config()->set(Config::GUI_TrayIconAppearance, m_generalUi->trayIconAppearance->currentData().toString());
    config()->set(Config::GUI_MinimizeToTray, m_generalUi->systrayMinimizeToTrayCheckBox->isChecked());
    config()->set(Config::GUI_MinimizeOnClose, m_generalUi->minimizeOnCloseCheckBox->isChecked());
    config()->set(Config::GUI_MinimizeOnStartup, m_generalUi->systrayMinimizeOnStartup->isChecked());
    config()->set(Config::GUI_CheckForUpdates, m_generalUi->checkForUpdatesOnStartupCheckBox->isChecked());
    config()->set(Config::GUI_CheckForUpdatesIncludeBetas,
                  m_generalUi->checkForUpdatesIncludeBetasCheckBox->isChecked());

    config()->set(Config::Security_AutoTypeAsk, m_generalUi->autoTypeAskCheckBox->isChecked());

    if (autoType()->isAvailable()) {
        config()->set(Config::GlobalAutoTypeKey, m_generalUi->autoTypeShortcutWidget->key());
        config()->set(Config::GlobalAutoTypeModifiers,
                      static_cast<int>(m_generalUi->autoTypeShortcutWidget->modifiers()));
        config()->set(Config::AutoTypeDelay, m_generalUi->autoTypeDelaySpinBox->value());
        config()->set(Config::AutoTypeStartDelay, m_generalUi->autoTypeStartDelaySpinBox->value());
    }
    config()->set(Config::Security_ClearClipboard, m_secUi->clearClipboardCheckBox->isChecked());
    config()->set(Config::Security_ClearClipboardTimeout, m_secUi->clearClipboardSpinBox->value());

    config()->set(Config::Security_ClearSearch, m_secUi->clearSearchCheckBox->isChecked());
    config()->set(Config::Security_ClearSearchTimeout, m_secUi->clearSearchSpinBox->value());

    config()->set(Config::Security_LockDatabaseIdle, m_secUi->lockDatabaseIdleCheckBox->isChecked());
    config()->set(Config::Security_LockDatabaseIdleSeconds, m_secUi->lockDatabaseIdleSpinBox->value());
    config()->set(Config::Security_LockDatabaseMinimize, m_secUi->lockDatabaseMinimizeCheckBox->isChecked());
    config()->set(Config::Security_LockDatabaseScreenLock, m_secUi->lockDatabaseOnScreenLockCheckBox->isChecked());
    config()->set(Config::Security_RelockAutoType, m_secUi->relockDatabaseAutoTypeCheckBox->isChecked());
    config()->set(Config::Security_IconDownloadFallback, m_secUi->fallbackToSearch->isChecked());

    config()->set(Config::Security_PasswordsHidden, m_secUi->passwordsHiddenCheckBox->isChecked());
    config()->set(Config::Security_PasswordEmptyPlaceholder, m_secUi->passwordShowDotsCheckBox->isChecked());

    config()->set(Config::Security_HidePasswordPreviewPanel, m_secUi->passwordPreviewCleartextCheckBox->isChecked());
    config()->set(Config::Security_PasswordsRepeatVisible, m_secUi->passwordsRepeatVisibleCheckBox->isChecked());
    config()->set(Config::Security_HideNotes, m_secUi->hideNotesCheckBox->isChecked());
    config()->set(Config::Security_NoConfirmMoveEntryToRecycleBin,
                  m_secUi->NoConfirmMoveEntryToRecycleBinCheckBox->isChecked());

    config()->set(Config::Security_ResetTouchId, m_secUi->touchIDResetCheckBox->isChecked());
    config()->set(Config::Security_ResetTouchIdTimeout, m_secUi->touchIDResetSpinBox->value());
    config()->set(Config::Security_ResetTouchIdScreenlock, m_secUi->touchIDResetOnScreenLockCheckBox->isChecked());

    // Security: clear storage if related settings are disabled
    if (!config()->get(Config::RememberLastDatabases).toBool()) {
        config()->remove(Config::LastDatabases);
        config()->remove(Config::LastActiveDatabase);
        config()->remove(Config::LastAttachmentDir);
    }

    if (!config()->get(Config::RememberLastKeyFiles).toBool()) {
        config()->remove(Config::LastKeyFiles);
        config()->remove(Config::LastChallengeResponse);
        config()->remove(Config::LastDir);
    }

    for (const ExtraPage& page : asConst(m_extraPages)) {
        page.saveSettings();
    }
}

void ApplicationSettingsWidget::resetSettings()
{
    // Confirm reset
    auto ans = MessageBox::question(this,
                                    tr("Reset Settings?"),
                                    tr("Are you sure you want to reset all general and security settings to default?"),
                                    MessageBox::Reset | MessageBox::Cancel,
                                    MessageBox::Cancel);
    if (ans == MessageBox::Cancel) {
        return;
    }

    if (config()->hasAccessError()) {
        showMessage(tr("Access error for config file %1").arg(config()->getFileName()), MessageWidget::Error);
        // We prevent closing the settings page if we could not write to
        // the config file.
        return;
    }

    // Reset general and security settings to default
    config()->resetToDefaults();

    // Clear recently used data
    config()->remove(Config::LastDatabases);
    config()->remove(Config::LastActiveDatabase);
    config()->remove(Config::LastAttachmentDir);
    config()->remove(Config::LastKeyFiles);
    config()->remove(Config::LastDir);

    // Save the Extra Pages (these are NOT reset)
    for (const ExtraPage& page : asConst(m_extraPages)) {
        page.saveSettings();
    }

    config()->sync();

    // Refresh the settings widget and notify listeners
    loadSettings();
    emit settingsReset();
}

void ApplicationSettingsWidget::reject()
{
    // register the old key again as it might have changed
    if (m_globalAutoTypeKey > 0 && m_globalAutoTypeModifiers > 0) {
        autoType()->registerGlobalShortcut(m_globalAutoTypeKey, m_globalAutoTypeModifiers);
    }
}

void ApplicationSettingsWidget::autoSaveToggled(bool checked)
{
    // Explicitly enable other auto-save options
    if (checked) {
        m_generalUi->autoSaveOnExitCheckBox->setChecked(true);
        m_generalUi->autoSaveNonDataChangesCheckBox->setChecked(true);
    }
    m_generalUi->autoSaveOnExitCheckBox->setEnabled(!checked);
    m_generalUi->autoSaveNonDataChangesCheckBox->setEnabled(!checked);
}

void ApplicationSettingsWidget::hideWindowOnCopyCheckBoxToggled(bool checked)
{
    m_generalUi->minimizeOnCopyRadioButton->setEnabled(checked);
    m_generalUi->dropToBackgroundOnCopyRadioButton->setEnabled(checked);
}

void ApplicationSettingsWidget::systrayToggled(bool checked)
{
    m_generalUi->trayIconAppearance->setEnabled(checked);
    m_generalUi->trayIconAppearanceLabel->setEnabled(checked);
    m_generalUi->systrayMinimizeToTrayCheckBox->setEnabled(checked);
}

void ApplicationSettingsWidget::rememberDatabasesToggled(bool checked)
{
    if (!checked) {
        m_generalUi->rememberLastKeyFilesCheckBox->setChecked(false);
        m_generalUi->openPreviousDatabasesOnStartupCheckBox->setChecked(false);
    }

    m_generalUi->rememberLastKeyFilesCheckBox->setEnabled(checked);
    m_generalUi->openPreviousDatabasesOnStartupCheckBox->setEnabled(checked);
}

void ApplicationSettingsWidget::checkUpdatesToggled(bool checked)
{
    m_generalUi->checkForUpdatesIncludeBetasCheckBox->setEnabled(checked);
}
