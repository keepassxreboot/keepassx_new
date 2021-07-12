/*
 *  Copyright (C) 2013 Francois Ferrand
 *  Copyright (C) 2017 Sami Vänttinen <sami.vanttinen@protonmail.com>
 *  Copyright (C) 2017 KeePassXC Team <team@keepassxc.org>
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
#include "core/PasswordHealth.h"
#include <QDir>

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
    auto homePath = replaceHomePath(location);
    config()->set(Config::Browser_CustomProxyLocation, homePath);
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
    auto homePath = replaceHomePath(location);
    config()->set(Config::Browser_CustomBrowserLocation, homePath);
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

bool BrowserSettings::passwordUseNumbers()
{
    return config()->get(Config::PasswordGenerator_Numbers).toBool();
}

void BrowserSettings::setPasswordUseNumbers(bool useNumbers)
{
    config()->set(Config::PasswordGenerator_Numbers, useNumbers);
}

bool BrowserSettings::passwordUseLowercase()
{
    return config()->get(Config::PasswordGenerator_LowerCase).toBool();
}

void BrowserSettings::setPasswordUseLowercase(bool useLowercase)
{
    config()->set(Config::PasswordGenerator_LowerCase, useLowercase);
}

bool BrowserSettings::passwordUseUppercase()
{
    return config()->get(Config::PasswordGenerator_UpperCase).toBool();
}

void BrowserSettings::setPasswordUseUppercase(bool useUppercase)
{
    config()->set(Config::PasswordGenerator_UpperCase, useUppercase);
}

bool BrowserSettings::passwordUseSpecial()
{
    return config()->get(Config::PasswordGenerator_SpecialChars).toBool();
}

void BrowserSettings::setPasswordUseSpecial(bool useSpecial)
{
    config()->set(Config::PasswordGenerator_SpecialChars, useSpecial);
}

bool BrowserSettings::passwordUseBraces()
{
    return config()->get(Config::PasswordGenerator_Braces).toBool();
}

void BrowserSettings::setPasswordUseBraces(bool useBraces)
{
    config()->set(Config::PasswordGenerator_Braces, useBraces);
}

bool BrowserSettings::passwordUsePunctuation()
{
    return config()->get(Config::PasswordGenerator_Punctuation).toBool();
}

void BrowserSettings::setPasswordUsePunctuation(bool usePunctuation)
{
    config()->set(Config::PasswordGenerator_Punctuation, usePunctuation);
}

bool BrowserSettings::passwordUseQuotes()
{
    return config()->get(Config::PasswordGenerator_Quotes).toBool();
}

void BrowserSettings::setPasswordUseQuotes(bool useQuotes)
{
    config()->set(Config::PasswordGenerator_Quotes, useQuotes);
}

bool BrowserSettings::passwordUseDashes()
{
    return config()->get(Config::PasswordGenerator_Dashes).toBool();
}

void BrowserSettings::setPasswordUseDashes(bool useDashes)
{
    config()->set(Config::PasswordGenerator_Dashes, useDashes);
}

bool BrowserSettings::passwordUseMath()
{
    return config()->get(Config::PasswordGenerator_Math).toBool();
}

void BrowserSettings::setPasswordUseMath(bool useMath)
{
    config()->set(Config::PasswordGenerator_Math, useMath);
}

bool BrowserSettings::passwordUseLogograms()
{
    return config()->get(Config::PasswordGenerator_Logograms).toBool();
}

void BrowserSettings::setPasswordUseLogograms(bool useLogograms)
{
    config()->set(Config::PasswordGenerator_Logograms, useLogograms);
}

bool BrowserSettings::passwordUseEASCII()
{
    return config()->get(Config::PasswordGenerator_EASCII).toBool();
}

void BrowserSettings::setPasswordUseEASCII(bool useEASCII)
{
    config()->set(Config::PasswordGenerator_EASCII, useEASCII);
}

bool BrowserSettings::advancedMode()
{
    return config()->get(Config::PasswordGenerator_AdvancedMode).toBool();
}

void BrowserSettings::setAdvancedMode(bool advancedMode)
{
    config()->set(Config::PasswordGenerator_AdvancedMode, advancedMode);
}

QString BrowserSettings::passwordAdditionalChars()
{
    return config()->get(Config::PasswordGenerator_AdditionalChars).toString();
}

void BrowserSettings::setPasswordAdditionalChars(const QString& chars)
{
    config()->set(Config::PasswordGenerator_AdditionalChars, chars);
}

QString BrowserSettings::passwordExcludedChars()
{
    return config()->get(Config::PasswordGenerator_ExcludedChars).toString();
}

void BrowserSettings::setPasswordExcludedChars(const QString& chars)
{
    config()->set(Config::PasswordGenerator_ExcludedChars, chars);
}

int BrowserSettings::passPhraseWordCount()
{
    return config()->get(Config::PasswordGenerator_WordCount).toInt();
}

void BrowserSettings::setPassPhraseWordCount(int wordCount)
{
    config()->set(Config::PasswordGenerator_WordCount, wordCount);
}

QString BrowserSettings::passPhraseWordSeparator()
{
    return config()->get(Config::PasswordGenerator_WordSeparator).toString();
}

void BrowserSettings::setPassPhraseWordSeparator(const QString& separator)
{
    config()->set(Config::PasswordGenerator_WordSeparator, separator);
}

int BrowserSettings::generatorType()
{
    return config()->get(Config::PasswordGenerator_Type).toInt();
}

void BrowserSettings::setGeneratorType(int type)
{
    config()->set(Config::PasswordGenerator_Type, type);
}

bool BrowserSettings::passwordEveryGroup()
{
    return config()->get(Config::PasswordGenerator_EnsureEvery).toBool();
}

void BrowserSettings::setPasswordEveryGroup(bool everyGroup)
{
    config()->set(Config::PasswordGenerator_EnsureEvery, everyGroup);
}

bool BrowserSettings::passwordExcludeAlike()
{
    return config()->get(Config::PasswordGenerator_ExcludeAlike).toBool();
}

void BrowserSettings::setPasswordExcludeAlike(bool excludeAlike)
{
    config()->set(Config::PasswordGenerator_ExcludeAlike, excludeAlike);
}

int BrowserSettings::passwordLength()
{
    return config()->get(Config::PasswordGenerator_Length).toInt();
}

void BrowserSettings::setPasswordLength(int length)
{
    config()->set(Config::PasswordGenerator_Length, length);
    m_passwordGenerator.setLength(length);
}

PasswordGenerator::CharClasses BrowserSettings::passwordCharClasses()
{
    PasswordGenerator::CharClasses classes;
    if (passwordUseLowercase()) {
        classes |= PasswordGenerator::LowerLetters;
    }
    if (passwordUseUppercase()) {
        classes |= PasswordGenerator::UpperLetters;
    }
    if (passwordUseNumbers()) {
        classes |= PasswordGenerator::Numbers;
    }
    if (passwordUseSpecial()) {
        classes |= PasswordGenerator::SpecialCharacters;
    }
    if (passwordUseBraces()) {
        classes |= PasswordGenerator::Braces;
    }
    if (passwordUsePunctuation()) {
        classes |= PasswordGenerator::Punctuation;
    }
    if (passwordUseQuotes()) {
        classes |= PasswordGenerator::Quotes;
    }
    if (passwordUseDashes()) {
        classes |= PasswordGenerator::Dashes;
    }
    if (passwordUseMath()) {
        classes |= PasswordGenerator::Math;
    }
    if (passwordUseLogograms()) {
        classes |= PasswordGenerator::Logograms;
    }
    if (passwordUseEASCII()) {
        classes |= PasswordGenerator::EASCII;
    }
    return classes;
}

PasswordGenerator::GeneratorFlags BrowserSettings::passwordGeneratorFlags()
{
    PasswordGenerator::GeneratorFlags flags;
    if (passwordExcludeAlike()) {
        flags |= PasswordGenerator::ExcludeLookAlike;
    }
    if (passwordEveryGroup()) {
        flags |= PasswordGenerator::CharFromEveryGroup;
    }
    return flags;
}

QJsonObject BrowserSettings::generatePassword()
{
    QJsonObject password;
    if (generatorType() == 0) {
        m_passwordGenerator.setLength(passwordLength());
        m_passwordGenerator.setCharClasses(passwordCharClasses());
        m_passwordGenerator.setFlags(passwordGeneratorFlags());
        const QString pw = m_passwordGenerator.generatePassword();
        password["entropy"] = PasswordHealth(pw).entropy();
        password["password"] = pw;
    } else {
        m_passPhraseGenerator.setWordCount(passPhraseWordCount());
        m_passPhraseGenerator.setWordSeparator(passPhraseWordSeparator());
        password["entropy"] = m_passPhraseGenerator.estimateEntropy();
        password["password"] = m_passPhraseGenerator.generatePassphrase();
    }
    return password;
}

void BrowserSettings::updateBinaryPaths()
{
    m_nativeMessageInstaller.updateBinaryPaths();
}

QString BrowserSettings::replaceHomePath(QString location)
{
    auto homePath = QDir::homePath();
    if (location.startsWith(homePath)) {
        location.replace(homePath, "~");
    }

    return location;
}
