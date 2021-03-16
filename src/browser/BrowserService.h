/*
 *  Copyright (C) 2013 Francois Ferrand
 *  Copyright (C) 2017 Sami Vänttinen <sami.vanttinen@protonmail.com>
 *  Copyright (C) 2020 KeePassXC Team <team@keepassxc.org>
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

#ifndef BROWSERSERVICE_H
#define BROWSERSERVICE_H

#include "core/Entry.h"
#include <QObject>
#include <QPointer>
#include <QSharedPointer>
#include <QtCore>

typedef QPair<QString, QString> StringPair;
typedef QList<StringPair> StringPairList;

enum
{
    max_length = 16 * 1024
};

class DatabaseWidget;
class BrowserHost;
class BrowserAction;

class BrowserService : public QObject
{
    Q_OBJECT

public:
    explicit BrowserService();
    static BrowserService* instance();

    void setEnabled(bool enabled);

    QString getKey(const QString& id);
    QString storeKey(const QString& key);
    QString getDatabaseHash(bool legacy = false);

    bool isDatabaseOpened() const;
    bool openDatabase(bool triggerUnlock);
    void lockDatabase();

    QJsonObject getDatabaseGroups();
    QJsonObject createNewGroup(const QString& groupName);
    QString getCurrentTotp(const QString& uuid);

    void addEntry(const QString& dbid,
                  const QString& login,
                  const QString& password,
                  const QString& siteUrlStr,
                  const QString& formUrlStr,
                  const QString& realm,
                  const QString& group,
                  const QString& groupUuid,
                  const QSharedPointer<Database>& selectedDb = {});
    bool updateEntry(const QString& dbid,
                     const QString& uuid,
                     const QString& login,
                     const QString& password,
                     const QString& siteUrlStr,
                     const QString& formUrlStr);

    QJsonArray findMatchingEntries(const QString& dbid,
                                   const QString& siteUrlStr,
                                   const QString& formUrlStr,
                                   const QString& realm,
                                   const StringPairList& keyList,
                                   const bool httpAuth = false);

    void performGlobalAutoType(const QString& url);
    static void convertAttributesToCustomData(QSharedPointer<Database> db);

    static const QString KEEPASSXCBROWSER_NAME;
    static const QString KEEPASSXCBROWSER_OLD_NAME;
    static const QString OPTION_SKIP_AUTO_SUBMIT;
    static const QString OPTION_HIDE_ENTRY;
    static const QString OPTION_ONLY_HTTP_AUTH;
    static const QString OPTION_NOT_HTTP_AUTH;
    static const QString ADDITIONAL_URL;

signals:
    void requestUnlock();

public slots:
    void databaseLocked(DatabaseWidget* dbWidget);
    void databaseUnlocked(DatabaseWidget* dbWidget);
    void activeDatabaseChanged(DatabaseWidget* dbWidget);

private slots:
    void processClientMessage(const QJsonObject& message);

private:
    enum Access
    {
        Denied,
        Unknown,
        Allowed
    };

    enum WindowState
    {
        Normal,
        Minimized,
        Hidden
    };

    QList<Entry*>
    searchEntries(const QSharedPointer<Database>& db, const QString& siteUrlStr, const QString& formUrlStr);
    QList<Entry*> searchEntries(const QString& siteUrlStr, const QString& formUrlStr, const StringPairList& keyList);
    QList<Entry*> sortEntries(QList<Entry*>& pwEntries, const QString& siteUrlStr, const QString& formUrlStr);
    QList<Entry*> confirmEntries(QList<Entry*>& pwEntriesToConfirm,
                                 const QString& siteUrlStr,
                                 const QString& siteHost,
                                 const QString& formUrlStr,
                                 const QString& realm,
                                 const bool httpAuth);
    QJsonObject prepareEntry(const Entry* entry);
    QJsonArray getChildrenFromGroup(Group* group);
    Access checkAccess(const Entry* entry, const QString& siteHost, const QString& formHost, const QString& realm);
    Group* getDefaultEntryGroup(const QSharedPointer<Database>& selectedDb = {});
    int sortPriority(const QStringList& urls, const QString& siteUrlStr, const QString& formUrlStr);
    bool schemeFound(const QString& url);
    bool removeFirstDomain(QString& hostname);
    bool handleEntry(Entry* entry, const QString& url, const QString& submitUrl);
    bool handleURL(const QString& entryUrl, const QString& siteUrlStr, const QString& formUrlStr);
    QSharedPointer<Database> getDatabase();
    QSharedPointer<Database> selectedDatabase();
    QString getDatabaseRootUuid();
    QString getDatabaseRecycleBinUuid();
    bool checkLegacySettings(QSharedPointer<Database> db);
    QStringList getEntryURLs(const Entry* entry);

    void hideWindow() const;
    void raiseWindow(const bool force = false);
    void updateWindowState();

    static bool moveSettingsToCustomData(Entry* entry, const QString& name);
    static int moveKeysToCustomData(Entry* entry, QSharedPointer<Database> db);

    QPointer<BrowserHost> m_browserHost;
    QHash<QString, QSharedPointer<BrowserAction>> m_browserClients;

    bool m_dialogActive;
    bool m_bringToFrontRequested;
    WindowState m_prevWindowState;
    QUuid m_keepassBrowserUUID;

    QPointer<DatabaseWidget> m_currentDatabaseWidget;

    Q_DISABLE_COPY(BrowserService);

    friend class TestBrowser;
};

static inline BrowserService* browserService()
{
    return BrowserService::instance();
}

#endif // BROWSERSERVICE_H
