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

#ifndef BROWSERSETTINGS_H
#define BROWSERSETTINGS_H

#include "BrowserShared.h"
#include "NativeMessageInstaller.h"
#include "core/PassphraseGenerator.h"
#include "core/PasswordGenerator.h"

class BrowserSettings
{
public:
    explicit BrowserSettings() = default;
    static BrowserSettings* instance();

    bool isEnabled();
    void setEnabled(bool enabled);

    bool showNotification(); // TODO!!
    void setShowNotification(bool showNotification);
    bool bestMatchOnly();
    void setBestMatchOnly(bool bestMatchOnly);
    bool unlockDatabase();
    void setUnlockDatabase(bool unlockDatabase);
    bool matchUrlScheme();
    void setMatchUrlScheme(bool matchUrlScheme);
    bool alwaysAllowAccess();
    void setAlwaysAllowAccess(bool alwaysAllowAccess);
    bool alwaysAllowUpdate();
    void setAlwaysAllowUpdate(bool alwaysAllowUpdate);
    bool searchInAllDatabases();
    void setSearchInAllDatabases(bool searchInAllDatabases);
    bool httpAuthPermission();
    void setHttpAuthPermission(bool httpAuthPermission);
    bool supportKphFields();
    void setSupportKphFields(bool supportKphFields);
    bool noMigrationPrompt();
    void setNoMigrationPrompt(bool prompt);

    bool useCustomProxy();
    void setUseCustomProxy(bool enabled);
    QString customProxyLocation();
    void setCustomProxyLocation(const QString& location);
    QString proxyLocation();
#ifdef QT_DEBUG
    QString customExtensionId();
    void setCustomExtensionId(const QString& id);
#endif
    bool updateBinaryPath();
    void setUpdateBinaryPath(bool enabled);
    bool allowExpiredCredentials();
    void setAllowExpiredCredentials(bool enabled);

    bool browserSupport(BrowserShared::SupportedBrowsers browser);
    void setBrowserSupport(BrowserShared::SupportedBrowsers browser, bool enabled);
    bool customBrowserSupport();
    void setCustomBrowserSupport(bool enabled);
    int customBrowserType();
    void setCustomBrowserType(int type);
    QString customBrowserLocation();
    void setCustomBrowserLocation(const QString& location);
    bool passwordUseNumbers();
    void setPasswordUseNumbers(bool useNumbers);
    bool passwordUseLowercase();
    void setPasswordUseLowercase(bool useLowercase);
    bool passwordUseUppercase();
    void setPasswordUseUppercase(bool useUppercase);
    bool passwordUseSpecial();
    void setPasswordUseSpecial(bool useSpecial);
    bool passwordUseBraces();
    void setPasswordUseBraces(bool useBraces);
    bool passwordUsePunctuation();
    void setPasswordUsePunctuation(bool usePunctuation);
    bool passwordUseQuotes();
    void setPasswordUseQuotes(bool useQuotes);
    bool passwordUseDashes();
    void setPasswordUseDashes(bool useDashes);
    bool passwordUseMath();
    void setPasswordUseMath(bool useMath);
    bool passwordUseLogograms();
    void setPasswordUseLogograms(bool useLogograms);
    bool passwordUseEASCII();
    void setPasswordUseEASCII(bool useEASCII);
    bool advancedMode();
    void setAdvancedMode(bool advancedMode);
    QString passwordAdditionalChars();
    void setPasswordAdditionalChars(const QString& chars);
    QString passwordExcludedChars();
    void setPasswordExcludedChars(const QString& chars);
    int passPhraseWordCount();
    void setPassPhraseWordCount(int wordCount);
    QString passPhraseWordSeparator();
    void setPassPhraseWordSeparator(const QString& separator);
    int generatorType();
    void setGeneratorType(int type);
    bool passwordEveryGroup();
    void setPasswordEveryGroup(bool everyGroup);
    bool passwordExcludeAlike();
    void setPasswordExcludeAlike(bool excludeAlike);
    int passwordLength();
    void setPasswordLength(int length);
    PasswordGenerator::CharClasses passwordCharClasses();
    PasswordGenerator::GeneratorFlags passwordGeneratorFlags();
    QJsonObject generatePassword();
    void updateBinaryPaths();
    QString replaceHomePath(QString location);

private:
    static BrowserSettings* m_instance;

    PasswordGenerator m_passwordGenerator;
    PassphraseGenerator m_passPhraseGenerator;
    NativeMessageInstaller m_nativeMessageInstaller;
};

inline BrowserSettings* browserSettings()
{
    return BrowserSettings::instance();
}

#endif // BROWSERSETTINGS_H
