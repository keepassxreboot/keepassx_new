/*
 *  Copyright (C) 2020 KeePassXC Team <team@keepassxc.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "BrowserSettingsWidget.h"
#include "ui_BrowserSettingsWidget.h"

#include "BrowserSettings.h"
#include "config-keepassx.h"
#include "core/Resources.h"

#include <QFileDialog>

BrowserSettingsWidget::BrowserSettingsWidget(QWidget* parent)
    : QWidget(parent)
    , m_ui(new Ui::BrowserSettingsWidget())
{
    m_ui->setupUi(this);

    // clang-format off
    QString snapInstructions;
#if defined(KEEPASSXC_DIST_SNAP)
    snapInstructions = "<br /><br />" +
            tr("Due to Snap sandboxing, you must run a script to enable browser integration."
               "<br />"
               "You can obtain this script from %1")
               .arg("<a href=\"https://keepassxc.org/download#linux\">https://keepassxc.org</a>");
#endif

    m_ui->extensionLabel->setOpenExternalLinks(true);
    m_ui->extensionLabel->setText(
        tr("KeePassXC-Browser is needed for the browser integration to work. <br />Download it for %1 and %2 and %3. %4")
            .arg("<a href=\"https://addons.mozilla.org/firefox/addon/keepassxc-browser/\">Firefox</a>",
                 "<a href=\"https://chrome.google.com/webstore/detail/keepassxc-browser/oboonakemofpalcgghocfoadofidjkkk\">"
                 "Google Chrome / Chromium / Vivaldi / Brave</a>",
                 "<a href=\"https://microsoftedge.microsoft.com/addons/detail/pdffhmdngciaglkoonimfcmckehcpafo\">Microsoft Edge</a>",
                 snapInstructions));
    // clang-format on

    m_ui->warningWidget->setCloseButtonVisible(false);
    m_ui->warningWidget->setAutoHideTimeout(-1);
    m_ui->warningWidget->setAnimate(false);

    m_ui->tabWidget->setEnabled(m_ui->enableBrowserSupport->isChecked());
    connect(m_ui->enableBrowserSupport, SIGNAL(toggled(bool)), m_ui->tabWidget, SLOT(setEnabled(bool)));

    // Custom Browser option
#ifdef Q_OS_WIN
    // TODO: Custom browser is disabled on Windows
    m_ui->customBrowserSupport->setVisible(false);
    m_ui->customBrowserGroupBox->setVisible(false);
#else
    connect(m_ui->customBrowserLocationBrowseButton, SIGNAL(clicked()), SLOT(showCustomBrowserLocationFileDialog()));
    connect(m_ui->customBrowserSupport, SIGNAL(toggled(bool)), m_ui->customBrowserGroupBox, SLOT(setEnabled(bool)));
#endif

    // Custom Proxy option
    m_ui->customProxyLocation->setEnabled(m_ui->useCustomProxy->isChecked());
    m_ui->customProxyLocationBrowseButton->setEnabled(m_ui->useCustomProxy->isChecked());

    connect(m_ui->useCustomProxy, SIGNAL(toggled(bool)), m_ui->customProxyLocation, SLOT(setEnabled(bool)));
    connect(m_ui->useCustomProxy, SIGNAL(toggled(bool)), m_ui->customProxyLocationBrowseButton, SLOT(setEnabled(bool)));
    connect(m_ui->useCustomProxy, SIGNAL(toggled(bool)), SLOT(validateCustomProxyLocation()));
    connect(m_ui->customProxyLocation, SIGNAL(editingFinished()), SLOT(validateCustomProxyLocation()));
    connect(m_ui->customProxyLocationBrowseButton, SIGNAL(clicked()), this, SLOT(showProxyLocationFileDialog()));

#ifndef Q_OS_LINUX
    m_ui->snapWarningLabel->setVisible(false);
#endif

#ifdef Q_OS_WIN
    // Brave uses Chrome's registry settings
    m_ui->braveSupport->setHidden(true);
    // Vivaldi uses Chrome's registry settings
    m_ui->vivaldiSupport->setHidden(true);
    m_ui->chromeSupport->setText("Chrome, Vivaldi, and Brave");
    // Tor Browser uses Firefox's registry settings
    m_ui->torBrowserSupport->setHidden(true);
    m_ui->firefoxSupport->setText("Firefox and Tor Browser");
#endif
    m_ui->browserGlobalWarningWidget->setVisible(false);

#ifndef QT_DEBUG
    m_ui->customExtensionId->setVisible(false);
    m_ui->customExtensionLabel->setVisible(false);
#endif
}

BrowserSettingsWidget::~BrowserSettingsWidget()
{
}

void BrowserSettingsWidget::loadSettings()
{
    auto settings = browserSettings();
    m_ui->enableBrowserSupport->setChecked(settings->isEnabled());

    m_ui->showNotification->setChecked(settings->showNotification());
    m_ui->bestMatchOnly->setChecked(settings->bestMatchOnly());
    m_ui->unlockDatabase->setChecked(settings->unlockDatabase());
    m_ui->matchUrlScheme->setChecked(settings->matchUrlScheme());

    // hide unimplemented options
    // TODO: fix this
    m_ui->showNotification->hide();

    m_ui->alwaysAllowAccess->setChecked(settings->alwaysAllowAccess());
    m_ui->alwaysAllowUpdate->setChecked(settings->alwaysAllowUpdate());
    m_ui->httpAuthPermission->setChecked(settings->httpAuthPermission());
    m_ui->searchInAllDatabases->setChecked(settings->searchInAllDatabases());
    m_ui->supportKphFields->setChecked(settings->supportKphFields());
    m_ui->noMigrationPrompt->setChecked(settings->noMigrationPrompt());
    m_ui->useCustomProxy->setChecked(settings->useCustomProxy());
    m_ui->customProxyLocation->setText(settings->replaceHomePath(settings->customProxyLocation()));
    m_ui->updateBinaryPath->setChecked(settings->updateBinaryPath());
    m_ui->allowExpiredCredentials->setChecked(settings->allowExpiredCredentials());
    m_ui->chromeSupport->setChecked(settings->browserSupport(BrowserShared::CHROME));
    m_ui->chromiumSupport->setChecked(settings->browserSupport(BrowserShared::CHROMIUM));
    m_ui->firefoxSupport->setChecked(settings->browserSupport(BrowserShared::FIREFOX));
    m_ui->edgeSupport->setChecked(settings->browserSupport(BrowserShared::EDGE));
#ifndef Q_OS_WIN
    m_ui->braveSupport->setChecked(settings->browserSupport(BrowserShared::BRAVE));
    m_ui->vivaldiSupport->setChecked(settings->browserSupport(BrowserShared::VIVALDI));
    m_ui->torBrowserSupport->setChecked(settings->browserSupport(BrowserShared::TOR_BROWSER));
#endif
#ifndef Q_OS_LINUX
    m_ui->snapWarningLabel->setVisible(false);
#endif

// TODO: Enable Edge support when Linux version is released
#ifdef Q_OS_LINUX
    m_ui->edgeSupport->setChecked(false);
    m_ui->edgeSupport->setEnabled(false);
#endif

#ifdef KEEPASSXC_DIST_SNAP
    // Disable settings that will not work
    m_ui->useCustomProxy->setChecked(false);
    m_ui->useCustomProxy->setVisible(false);
    m_ui->customProxyLocation->setVisible(false);
    m_ui->customProxyLocationBrowseButton->setVisible(false);
    m_ui->browsersGroupBox->setVisible(false);
    m_ui->browsersGroupBox->setEnabled(false);
    m_ui->updateBinaryPath->setChecked(false);
    m_ui->updateBinaryPath->setVisible(false);
    // No custom browser for snaps
    m_ui->customBrowserSupport->setVisible(false);
    m_ui->customBrowserGroupBox->setVisible(false);
    // Show notice to user
    m_ui->browserGlobalWarningWidget->showMessage(tr("Please see special instructions for browser extension use below"),
                                                  MessageWidget::Warning);
    m_ui->browserGlobalWarningWidget->setCloseButtonVisible(false);
    m_ui->browserGlobalWarningWidget->setAutoHideTimeout(-1);
#endif

    const auto customBrowserSet = settings->customBrowserSupport();
    m_ui->customBrowserSupport->setChecked(customBrowserSet);
    m_ui->customBrowserGroupBox->setEnabled(customBrowserSet);
    m_ui->browserTypeComboBox->clear();
    m_ui->browserTypeComboBox->addItem(tr("Firefox"), BrowserShared::SupportedBrowsers::FIREFOX);
    m_ui->browserTypeComboBox->addItem(tr("Chromium"), BrowserShared::SupportedBrowsers::CHROMIUM);
    auto typeIndex = m_ui->browserTypeComboBox->findData(settings->customBrowserType());
    if (typeIndex >= 0) {
        m_ui->browserTypeComboBox->setCurrentIndex(typeIndex);
    }
    m_ui->customBrowserLocation->setText(settings->replaceHomePath(settings->customBrowserLocation()));

#ifdef QT_DEBUG
    m_ui->customExtensionId->setText(settings->customExtensionId());
#endif

    validateCustomProxyLocation();
}

void BrowserSettingsWidget::validateCustomProxyLocation()
{
    auto path = browserSettings()->customBrowserLocation();

    if (m_ui->useCustomProxy->isChecked() && !QFile::exists(path)) {
        m_ui->warningWidget->showMessage(tr("<b>Error:</b> The custom proxy location cannot be found!"
                                            "<br/>Browser integration WILL NOT WORK without the proxy application."),
                                         MessageWidget::Error);
    } else {
        m_ui->warningWidget->showMessage(tr("<b>Warning:</b> The following options can be dangerous!"),
                                         MessageWidget::Warning);
    }
}

void BrowserSettingsWidget::saveSettings()
{
    auto settings = browserSettings();
    settings->setEnabled(m_ui->enableBrowserSupport->isChecked());
    settings->setShowNotification(m_ui->showNotification->isChecked());
    settings->setBestMatchOnly(m_ui->bestMatchOnly->isChecked());
    settings->setUnlockDatabase(m_ui->unlockDatabase->isChecked());
    settings->setMatchUrlScheme(m_ui->matchUrlScheme->isChecked());

    settings->setUseCustomProxy(m_ui->useCustomProxy->isChecked());
    settings->setCustomProxyLocation(m_ui->customProxyLocation->text());

    settings->setUpdateBinaryPath(m_ui->updateBinaryPath->isChecked());
    settings->setAllowExpiredCredentials(m_ui->allowExpiredCredentials->isChecked());
    settings->setAlwaysAllowAccess(m_ui->alwaysAllowAccess->isChecked());
    settings->setAlwaysAllowUpdate(m_ui->alwaysAllowUpdate->isChecked());
    settings->setHttpAuthPermission(m_ui->httpAuthPermission->isChecked());
    settings->setSearchInAllDatabases(m_ui->searchInAllDatabases->isChecked());
    settings->setSupportKphFields(m_ui->supportKphFields->isChecked());
    settings->setNoMigrationPrompt(m_ui->noMigrationPrompt->isChecked());

#ifdef QT_DEBUG
    settings->setCustomExtensionId(m_ui->customExtensionId->text());
#endif

    settings->setBrowserSupport(BrowserShared::CHROME, m_ui->chromeSupport->isChecked());
    settings->setBrowserSupport(BrowserShared::CHROMIUM, m_ui->chromiumSupport->isChecked());
    settings->setBrowserSupport(BrowserShared::FIREFOX, m_ui->firefoxSupport->isChecked());
    settings->setBrowserSupport(BrowserShared::EDGE, m_ui->edgeSupport->isChecked());
#ifndef Q_OS_WIN
    settings->setBrowserSupport(BrowserShared::BRAVE, m_ui->braveSupport->isChecked());
    settings->setBrowserSupport(BrowserShared::VIVALDI, m_ui->vivaldiSupport->isChecked());
    settings->setBrowserSupport(BrowserShared::TOR_BROWSER, m_ui->torBrowserSupport->isChecked());

    // Custom browser settings
    bool customBrowserEnabled = m_ui->customBrowserSupport->isChecked();
    settings->setCustomBrowserType(m_ui->browserTypeComboBox->currentData().toInt());
    settings->setCustomBrowserLocation(m_ui->customBrowserLocation->text());
    settings->setCustomBrowserSupport(customBrowserEnabled);
    settings->setBrowserSupport(BrowserShared::CUSTOM, customBrowserEnabled);

    // If we disabled the custom browser support make sure to clear variables
    if (!customBrowserEnabled) {
        settings->setCustomBrowserLocation("");
    }
#endif
}

void BrowserSettingsWidget::showProxyLocationFileDialog()
{
#ifdef Q_OS_WIN
    QString fileTypeFilter(QString("%1 (*.exe);;%2 (*.*)").arg(tr("Executable Files"), tr("All Files")));
#else
    QString fileTypeFilter(QString("%1 (*)").arg(tr("Executable Files")));
#endif
    auto proxyLocation = QFileDialog::getOpenFileName(this,
                                                      tr("Select custom proxy location"),
                                                      QFileInfo(QCoreApplication::applicationDirPath()).filePath(),
                                                      fileTypeFilter);

    proxyLocation = browserSettings()->replaceHomePath(proxyLocation);
    m_ui->customProxyLocation->setText(proxyLocation);
    validateCustomProxyLocation();
}

void BrowserSettingsWidget::showCustomBrowserLocationFileDialog()
{
    auto location = QFileDialog::getExistingDirectory(this,
                                                      tr("Select native messaging host folder location"),
                                                      QFileInfo(QCoreApplication::applicationDirPath()).filePath());

    location = browserSettings()->replaceHomePath(location);
    if (!location.isEmpty()) {
        m_ui->customBrowserLocation->setText(location);
    }
}
