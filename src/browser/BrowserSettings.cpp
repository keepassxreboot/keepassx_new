/*
 *  Copyright (C) 2013 Francois Ferrand
 *  Copyright (C) 2017 Sami Vänttinen <sami.vanttinen@protonmail.com>
 *  Copyright (C) 2021 KeePassXC Team <team@keepassxc.org>
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

#include "BrowserSettings.h"
#include "core/Config.h"

BrowserSettings* BrowserSettings::m_instance(nullptr);

BrowserSettings* BrowserSettings::instance()
{
    if (!m_instance) {
        m_instance = new BrowserSettings();
    }

    return m_instance;
}

bool BrowserSettings::isEnabled()
{
    return config()->get(Config::Browser_Enabled).toBool();
}

void BrowserSettings::setEnabled(bool enabled)
{
    config()->set(Config::Browser_Enabled, enabled);
}

bool BrowserSettings::showNotification()
{
    return config()->get(Config::Browser_ShowNotification).toBool();
}

void BrowserSettings::setShowNotification(bool showNotification)
{
    config()->set(Config::Browser_ShowNotification, showNotification);
}

bool BrowserSettings::bestMatchOnly()
{
    return config()->get(Config::Browser_BestMatchOnly).toBool();
}

void BrowserSettings::setBestMatchOnly(bool bestMatchOnly)
{
    config()->set(Config::Browser_BestMatchOnly, bestMatchOnly);
}

bool BrowserSettings::unlockDatabase()
{
    return config()->get(Config::Browser_UnlockDatabase).toBool();
}

void BrowserSettings::setUnlockDatabase(bool unlockDatabase)
{
    config()->set(Config::Browser_UnlockDatabase, unlockDatabase);
}

bool BrowserSettings::matchUrlScheme()
{
    return config()->get(Config::Browser_MatchUrlScheme).toBool();
}

void BrowserSettings::setMatchUrlScheme(bool matchUrlScheme)
{
    config()->set(Config::Browser_MatchUrlScheme, matchUrlScheme);
}

bool BrowserSettings::alwaysAllowAccess()
{
    return config()->get(Config::Browser_AlwaysAllowAccess).toBool();
}

void BrowserSettings::setAlwaysAllowAccess(bool alwaysAllowAccess)
{
    config()->set(Config::Browser_AlwaysAllowAccess, alwaysAllowAccess);
}

bool BrowserSettings::alwaysAllowUpdate()
{
    return config()->get(Config::Browser_AlwaysAllowUpdate).toBool();
}

void BrowserSettings::setAlwaysAllowUpdate(bool alwaysAllowUpdate)
{
    config()->set(Config::Browser_AlwaysAllowUpdate, alwaysAllowUpdate);
}

bool BrowserSettings::httpAuthPermission()
{
    return config()->get(Config::Browser_HttpAuthPermission).toBool();
}

void BrowserSettings::setHttpAuthPermission(bool httpAuthPermission)
{
    config()->set(Config::Browser_HttpAuthPermission, httpAuthPermission);
}

bool BrowserSettings::searchInAllDatabases()
{
    return config()->get(Config::Browser_SearchInAllDatabases).toBool();
}

void BrowserSettings::setSearchInAllDatabases(bool searchInAllDatabases)
{
    config()->set(Config::Browser_SearchInAllDatabases, searchInAllDatabases);
}

bool BrowserSettings::supportKphFields()
{
    return config()->get(Config::Browser_SupportKphFields).toBool();
}

void BrowserSettings::setSupportKphFields(bool supportKphFields)
{
    config()->set(Config::Browser_SupportKphFields, supportKphFields);
}

bool BrowserSettings::noMigrationPrompt()
{
    return config()->get(Config::Browser_NoMigrationPrompt).toBool();
}

void BrowserSettings::setNoMigrationPrompt(bool prompt)
{
    config()->set(Config::Browser_NoMigrationPrompt, prompt);
}

bool BrowserSettings::useCustomProxy()
{
    return config()->get(Config::Browser_UseCustomProxy).toBool();
}

void BrowserSettings::setUseCustomProxy(bool enabled)
{
    config()->set(Config::Browser_UseCustomProxy, enabled);
}

QString BrowserSettings::customProxyLocation()
{
    return config()->get(Config::Browser_CustomProxyLocation).toString();
}

void BrowserSettings::setCustomProxyLocation(const QString& location)
{
    config()->set(Config::Browser_CustomProxyLocation, location);
}

bool BrowserSettings::customBrowserSupport()
{
    return config()->get(Config::Browser_UseCustomBrowser).toBool();
}

void BrowserSettings::setCustomBrowserSupport(bool enabled)
{
    config()->set(Config::Browser_UseCustomBrowser, enabled);
}

int BrowserSettings::customBrowserType()
{
    return config()->get(Config::Browser_CustomBrowserType).toInt();
}

void BrowserSettings::setCustomBrowserType(int type)
{
    config()->set(Config::Browser_CustomBrowserType, type);
}

QString BrowserSettings::customBrowserLocation()
{
    return config()->get(Config::Browser_CustomBrowserLocation).toString();
}

void BrowserSettings::setCustomBrowserLocation(const QString& location)
{
    config()->set(Config::Browser_CustomBrowserLocation, location);
}

QString BrowserSettings::proxyLocation()
{
    return m_nativeMessageInstaller.getProxyPath();
}

#ifdef QT_DEBUG
QString BrowserSettings::customExtensionId()
{
    return config()->get(Config::Browser_CustomExtensionId).toString();
}

void BrowserSettings::setCustomExtensionId(const QString& id)
{
    config()->set(Config::Browser_CustomExtensionId, id);
}
#endif

bool BrowserSettings::updateBinaryPath()
{
    return config()->get(Config::Browser_UpdateBinaryPath).toBool();
}

void BrowserSettings::setUpdateBinaryPath(bool enabled)
{
    config()->set(Config::Browser_UpdateBinaryPath, enabled);
}

bool BrowserSettings::allowExpiredCredentials()
{
    return config()->get(Config::Browser_AllowExpiredCredentials).toBool();
}

void BrowserSettings::setAllowExpiredCredentials(bool enabled)
{
    config()->set(Config::Browser_AllowExpiredCredentials, enabled);
}

bool BrowserSettings::browserSupport(BrowserShared::SupportedBrowsers browser)
{
    return m_nativeMessageInstaller.isBrowserEnabled(browser);
}

void BrowserSettings::setBrowserSupport(BrowserShared::SupportedBrowsers browser, bool enabled)
{
    m_nativeMessageInstaller.setBrowserEnabled(browser, enabled);
}

void BrowserSettings::updateBinaryPaths()
{
    m_nativeMessageInstaller.updateBinaryPaths();
}
