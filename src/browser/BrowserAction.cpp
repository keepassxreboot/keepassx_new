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

#include "BrowserAction.h"
#include "BrowserService.h"
#include "BrowserSettings.h"
#include "BrowserShared.h"
#include "config-keepassx.h"
#include "core/Global.h"

#include <QJsonDocument>
#include <QJsonParseError>
#include <sodium.h>
#include <sodium/crypto_box.h>
#include <sodium/randombytes.h>

namespace
{
    enum
    {
        ERROR_KEEPASS_DATABASE_NOT_OPENED = 1,
        ERROR_KEEPASS_DATABASE_HASH_NOT_RECEIVED = 2,
        ERROR_KEEPASS_CLIENT_PUBLIC_KEY_NOT_RECEIVED = 3,
        ERROR_KEEPASS_CANNOT_DECRYPT_MESSAGE = 4,
        ERROR_KEEPASS_TIMEOUT_OR_NOT_CONNECTED = 5,
        ERROR_KEEPASS_ACTION_CANCELLED_OR_DENIED = 6,
        ERROR_KEEPASS_CANNOT_ENCRYPT_MESSAGE = 7,
        ERROR_KEEPASS_ASSOCIATION_FAILED = 8,
        ERROR_KEEPASS_KEY_CHANGE_FAILED = 9,
        ERROR_KEEPASS_ENCRYPTION_KEY_UNRECOGNIZED = 10,
        ERROR_KEEPASS_NO_SAVED_DATABASES_FOUND = 11,
        ERROR_KEEPASS_INCORRECT_ACTION = 12,
        ERROR_KEEPASS_EMPTY_MESSAGE_RECEIVED = 13,
        ERROR_KEEPASS_NO_URL_PROVIDED = 14,
        ERROR_KEEPASS_NO_LOGINS_FOUND = 15,
        ERROR_KEEPASS_NO_GROUPS_FOUND = 16,
        ERROR_KEEPASS_CANNOT_CREATE_NEW_GROUP = 17
    };
}

QJsonObject BrowserAction::processClientMessage(const QJsonObject& json)
{
    if (json.isEmpty()) {
        return getErrorReply("", ERROR_KEEPASS_EMPTY_MESSAGE_RECEIVED);
    }

    bool triggerUnlock = false;
    const QString trigger = json.value("triggerUnlock").toString();
    if (!trigger.isEmpty() && trigger.compare(TRUE_STR) == 0) {
        triggerUnlock = true;
    }

    const QString action = json.value("action").toString();
    if (action.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_INCORRECT_ACTION);
    }

    if (action.compare("change-public-keys") != 0 && action.compare("request-autotype") != 0
        && !browserService()->isDatabaseOpened()) {
        if (m_clientPublicKey.isEmpty()) {
            return getErrorReply(action, ERROR_KEEPASS_CLIENT_PUBLIC_KEY_NOT_RECEIVED);
        } else if (!browserService()->openDatabase(triggerUnlock)) {
            return getErrorReply(action, ERROR_KEEPASS_DATABASE_NOT_OPENED);
        }
    }

    return handleAction(json);
}

// Private functions
///////////////////////

QJsonObject BrowserAction::handleAction(const QJsonObject& json)
{
    QString action = json.value("action").toString();

    if (action.compare("change-public-keys") == 0) {
        return handleChangePublicKeys(json, action);
    } else if (action.compare("get-databasehash") == 0) {
        return handleGetDatabaseHash(json, action);
    } else if (action.compare("associate") == 0) {
        return handleAssociate(json, action);
    } else if (action.compare("test-associate") == 0) {
        return handleTestAssociate(json, action);
    } else if (action.compare("get-logins") == 0) {
        return handleGetLogins(json, action);
    } else if (action.compare("generate-password") == 0) {
        return handleGeneratePassword(json, action);
    } else if (action.compare("set-login") == 0) {
        return handleSetLogin(json, action);
    } else if (action.compare("lock-database") == 0) {
        return handleLockDatabase(json, action);
    } else if (action.compare("get-database-groups") == 0) {
        return handleGetDatabaseGroups(json, action);
    } else if (action.compare("create-new-group") == 0) {
        return handleCreateNewGroup(json, action);
    } else if (action.compare("get-totp") == 0) {
        return handleGetTotp(json, action);
    } else if (action.compare("request-autotype") == 0) {
        return handleGlobalAutoType(json, action);
    }

    // Action was not recognized
    return getErrorReply(action, ERROR_KEEPASS_INCORRECT_ACTION);
}

QJsonObject BrowserAction::handleChangePublicKeys(const QJsonObject& json, const QString& action)
{
    const QString nonce = json.value("nonce").toString();
    const QString clientPublicKey = json.value("publicKey").toString();

    if (clientPublicKey.isEmpty() || nonce.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_CLIENT_PUBLIC_KEY_NOT_RECEIVED);
    }

    m_associated = false;
    unsigned char pk[crypto_box_PUBLICKEYBYTES];
    unsigned char sk[crypto_box_SECRETKEYBYTES];
    crypto_box_keypair(pk, sk);

    const QString publicKey = getBase64FromKey(pk, crypto_box_PUBLICKEYBYTES);
    const QString secretKey = getBase64FromKey(sk, crypto_box_SECRETKEYBYTES);
    if (publicKey.isEmpty() || secretKey.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_ENCRYPTION_KEY_UNRECOGNIZED);
    }

    m_clientPublicKey = clientPublicKey;
    m_publicKey = publicKey;
    m_secretKey = secretKey;

    QJsonObject response = buildMessage(incrementNonce(nonce));
    response["action"] = action;
    response["publicKey"] = publicKey;

    return response;
}

QJsonObject BrowserAction::handleGetDatabaseHash(const QJsonObject& json, const QString& action)
{
    const QString hash = browserService()->getDatabaseHash();
    const QString nonce = json.value("nonce").toString();
    const QString encrypted = json.value("message").toString();
    const QJsonObject decrypted = decryptMessage(encrypted, nonce);

    if (decrypted.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_CANNOT_DECRYPT_MESSAGE);
    }

    if (hash.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_DATABASE_HASH_NOT_RECEIVED);
    }

    QString command = decrypted.value("action").toString();
    if (!command.isEmpty() && command.compare("get-databasehash") == 0) {
        const QString newNonce = incrementNonce(nonce);

        QJsonObject message = buildMessage(newNonce);
        message["hash"] = hash;

        // Update a legacy database hash if found
        const QJsonArray hashes = decrypted.value("connectedKeys").toArray();
        if (!hashes.isEmpty()) {
            const QString legacyHash = browserService()->getDatabaseHash(true);
            if (hashes.contains(legacyHash)) {
                message["oldHash"] = legacyHash;
            }
        }

        return buildResponse(action, message, newNonce);
    }

    return getErrorReply(action, ERROR_KEEPASS_CANNOT_DECRYPT_MESSAGE);
}

QJsonObject BrowserAction::handleAssociate(const QJsonObject& json, const QString& action)
{
    const QString hash = browserService()->getDatabaseHash();
    const QString nonce = json.value("nonce").toString();
    const QString encrypted = json.value("message").toString();
    const QJsonObject decrypted = decryptMessage(encrypted, nonce);

    if (decrypted.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_CANNOT_DECRYPT_MESSAGE);
    }

    const QString key = decrypted.value("key").toString();
    if (key.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_ASSOCIATION_FAILED);
    }

    if (key.compare(m_clientPublicKey) == 0) {
        // Check for identification key. If it's not found, ensure backwards compatibility and use the current public
        // key
        const QString idKey = decrypted.value("idKey").toString();
        const QString id = browserService()->storeKey((idKey.isEmpty() ? key : idKey));
        if (id.isEmpty()) {
            return getErrorReply(action, ERROR_KEEPASS_ACTION_CANCELLED_OR_DENIED);
        }

        m_associated = true;
        const QString newNonce = incrementNonce(nonce);

        QJsonObject message = buildMessage(newNonce);
        message["hash"] = hash;
        message["id"] = id;
        return buildResponse(action, message, newNonce);
    }

    return getErrorReply(action, ERROR_KEEPASS_ASSOCIATION_FAILED);
}

QJsonObject BrowserAction::handleTestAssociate(const QJsonObject& json, const QString& action)
{
    const QString hash = browserService()->getDatabaseHash();
    const QString nonce = json.value("nonce").toString();
    const QString encrypted = json.value("message").toString();
    const QJsonObject decrypted = decryptMessage(encrypted, nonce);

    if (decrypted.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_CANNOT_DECRYPT_MESSAGE);
    }

    const QString responseKey = decrypted.value("key").toString();
    const QString id = decrypted.value("id").toString();
    if (responseKey.isEmpty() || id.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_DATABASE_NOT_OPENED);
    }

    const QString key = browserService()->getKey(id);
    if (key.isEmpty() || key.compare(responseKey) != 0) {
        return getErrorReply(action, ERROR_KEEPASS_ASSOCIATION_FAILED);
    }

    m_associated = true;
    const QString newNonce = incrementNonce(nonce);

    QJsonObject message = buildMessage(newNonce);
    message["hash"] = hash;
    message["id"] = id;

    return buildResponse(action, message, newNonce);
}

QJsonObject BrowserAction::handleGetLogins(const QJsonObject& json, const QString& action)
{
    const QString hash = browserService()->getDatabaseHash();
    const QString nonce = json.value("nonce").toString();
    const QString encrypted = json.value("message").toString();

    if (!m_associated) {
        return getErrorReply(action, ERROR_KEEPASS_ASSOCIATION_FAILED);
    }

    const QJsonObject decrypted = decryptMessage(encrypted, nonce);
    if (decrypted.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_CANNOT_DECRYPT_MESSAGE);
    }

    const QString siteUrl = decrypted.value("url").toString();
    if (siteUrl.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_NO_URL_PROVIDED);
    }

    const QJsonArray keys = decrypted.value("keys").toArray();

    StringPairList keyList;
    for (const QJsonValue val : keys) {
        const QJsonObject keyObject = val.toObject();
        keyList.push_back(qMakePair(keyObject.value("id").toString(), keyObject.value("key").toString()));
    }

    const QString id = decrypted.value("id").toString();
    const QString formUrl = decrypted.value("submitUrl").toString();
    const QString auth = decrypted.value("httpAuth").toString();
    const bool httpAuth = auth.compare(TRUE_STR) == 0 ? true : false;
    const QJsonArray users = browserService()->findMatchingEntries(id, siteUrl, formUrl, "", keyList, httpAuth);

    if (users.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_NO_LOGINS_FOUND);
    }

    const QString newNonce = incrementNonce(nonce);

    QJsonObject message = buildMessage(newNonce);
    message["count"] = users.count();
    message["entries"] = users;
    message["hash"] = hash;
    message["id"] = id;

    return buildResponse(action, message, newNonce);
}

QJsonObject BrowserAction::handleGeneratePassword(const QJsonObject& json, const QString& action)
{
    auto nonce = json.value("nonce").toString();
    auto password = browserSettings()->generatePassword();

    if (nonce.isEmpty() || password.isEmpty()) {
        return QJsonObject();
    }

    // For backwards compatibility
    password["login"] = password["entropy"];

    QJsonArray arr;
    arr.append(password);

    const QString newNonce = incrementNonce(nonce);

    QJsonObject message = buildMessage(newNonce);
    message["entries"] = arr;

    return buildResponse(action, message, newNonce);
}

QJsonObject BrowserAction::handleSetLogin(const QJsonObject& json, const QString& action)
{
    const QString hash = browserService()->getDatabaseHash();
    const QString nonce = json.value("nonce").toString();
    const QString encrypted = json.value("message").toString();

    if (!m_associated) {
        return getErrorReply(action, ERROR_KEEPASS_ASSOCIATION_FAILED);
    }

    const QJsonObject decrypted = decryptMessage(encrypted, nonce);
    if (decrypted.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_CANNOT_DECRYPT_MESSAGE);
    }

    const QString url = decrypted.value("url").toString();
    if (url.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_NO_URL_PROVIDED);
    }

    const QString id = decrypted.value("id").toString();
    const QString login = decrypted.value("login").toString();
    const QString password = decrypted.value("password").toString();
    const QString submitUrl = decrypted.value("submitUrl").toString();
    const QString uuid = decrypted.value("uuid").toString();
    const QString group = decrypted.value("group").toString();
    const QString groupUuid = decrypted.value("groupUuid").toString();
    const QString realm;

    bool result = true;
    if (uuid.isEmpty()) {
        browserService()->addEntry(id, login, password, url, submitUrl, realm, group, groupUuid);
    } else {
        result = browserService()->updateEntry(id, uuid, login, password, url, submitUrl);
    }

    const QString newNonce = incrementNonce(nonce);

    QJsonObject message = buildMessage(newNonce);
    message["count"] = QJsonValue::Null;
    message["entries"] = QJsonValue::Null;
    message["error"] = result ? QStringLiteral("success") : QStringLiteral("error");
    message["hash"] = hash;

    return buildResponse(action, message, newNonce);
}

QJsonObject BrowserAction::handleLockDatabase(const QJsonObject& json, const QString& action)
{
    const QString hash = browserService()->getDatabaseHash();
    const QString nonce = json.value("nonce").toString();
    const QString encrypted = json.value("message").toString();
    const QJsonObject decrypted = decryptMessage(encrypted, nonce);

    if (decrypted.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_CANNOT_DECRYPT_MESSAGE);
    }

    if (hash.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_DATABASE_HASH_NOT_RECEIVED);
    }

    QString command = decrypted.value("action").toString();
    if (!command.isEmpty() && command.compare("lock-database") == 0) {
        browserService()->lockDatabase();

        const QString newNonce = incrementNonce(nonce);
        QJsonObject message = buildMessage(newNonce);

        return buildResponse(action, message, newNonce);
    }

    return getErrorReply(action, ERROR_KEEPASS_DATABASE_HASH_NOT_RECEIVED);
}

QJsonObject BrowserAction::handleGetDatabaseGroups(const QJsonObject& json, const QString& action)
{
    const QString hash = browserService()->getDatabaseHash();
    const QString nonce = json.value("nonce").toString();
    const QString encrypted = json.value("message").toString();

    if (!m_associated) {
        return getErrorReply(action, ERROR_KEEPASS_ASSOCIATION_FAILED);
    }

    const QJsonObject decrypted = decryptMessage(encrypted, nonce);
    if (decrypted.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_CANNOT_DECRYPT_MESSAGE);
    }

    QString command = decrypted.value("action").toString();
    if (command.isEmpty() || command.compare("get-database-groups") != 0) {
        return getErrorReply(action, ERROR_KEEPASS_INCORRECT_ACTION);
    }

    const QJsonObject groups = browserService()->getDatabaseGroups();
    if (groups.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_NO_GROUPS_FOUND);
    }

    const QString newNonce = incrementNonce(nonce);

    QJsonObject message = buildMessage(newNonce);
    message["groups"] = groups;

    return buildResponse(action, message, newNonce);
}

QJsonObject BrowserAction::handleCreateNewGroup(const QJsonObject& json, const QString& action)
{
    const QString hash = browserService()->getDatabaseHash();
    const QString nonce = json.value("nonce").toString();
    const QString encrypted = json.value("message").toString();

    if (!m_associated) {
        return getErrorReply(action, ERROR_KEEPASS_ASSOCIATION_FAILED);
    }

    const QJsonObject decrypted = decryptMessage(encrypted, nonce);
    if (decrypted.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_CANNOT_DECRYPT_MESSAGE);
    }

    QString command = decrypted.value("action").toString();
    if (command.isEmpty() || command.compare("create-new-group") != 0) {
        return getErrorReply(action, ERROR_KEEPASS_INCORRECT_ACTION);
    }

    QString group = decrypted.value("groupName").toString();
    const QJsonObject newGroup = browserService()->createNewGroup(group);
    if (newGroup.isEmpty() || newGroup["name"].toString().isEmpty() || newGroup["uuid"].toString().isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_CANNOT_CREATE_NEW_GROUP);
    }

    const QString newNonce = incrementNonce(nonce);

    QJsonObject message = buildMessage(newNonce);
    message["name"] = newGroup["name"];
    message["uuid"] = newGroup["uuid"];

    return buildResponse(action, message, newNonce);
}

QJsonObject BrowserAction::handleGetTotp(const QJsonObject& json, const QString& action)
{
    const QString nonce = json.value("nonce").toString();
    const QString encrypted = json.value("message").toString();

    if (!m_associated) {
        return getErrorReply(action, ERROR_KEEPASS_ASSOCIATION_FAILED);
    }

    const QJsonObject decrypted = decryptMessage(encrypted, nonce);
    if (decrypted.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_CANNOT_DECRYPT_MESSAGE);
    }

    QString command = decrypted.value("action").toString();
    if (command.isEmpty() || command.compare("get-totp") != 0) {
        return getErrorReply(action, ERROR_KEEPASS_INCORRECT_ACTION);
    }

    const QString uuid = decrypted.value("uuid").toString();

    // Get the current TOTP
    const auto totp = browserService()->getCurrentTotp(uuid);
    const QString newNonce = incrementNonce(nonce);

    QJsonObject message = buildMessage(newNonce);
    message["totp"] = totp;

    return buildResponse(action, message, newNonce);
}

QJsonObject BrowserAction::handleGlobalAutoType(const QJsonObject& json, const QString& action)
{
    const QString nonce = json.value("nonce").toString();
    const QString encrypted = json.value("message").toString();
    const QJsonObject decrypted = decryptMessage(encrypted, nonce);

    if (decrypted.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_CANNOT_DECRYPT_MESSAGE);
    }

    QString command = decrypted.value("action").toString();
    if (command.isEmpty() || command.compare("request-autotype") != 0) {
        return getErrorReply(action, ERROR_KEEPASS_INCORRECT_ACTION);
    }

    const auto topLevelDomain = decrypted.value("search").toString();
    browserService()->requestGlobalAutoType(topLevelDomain);

    const QString newNonce = incrementNonce(nonce);
    QJsonObject message = buildMessage(newNonce);
    return buildResponse(action, message, newNonce);
}

QJsonObject BrowserAction::getErrorReply(const QString& action, const int errorCode) const
{
    QJsonObject response;
    response["action"] = action;
    response["errorCode"] = QString::number(errorCode);
    response["error"] = getErrorMessage(errorCode);
    return response;
}

QJsonObject BrowserAction::buildMessage(const QString& nonce) const
{
    QJsonObject message;
    message["version"] = KEEPASSXC_VERSION;
    message["success"] = TRUE_STR;
    message["nonce"] = nonce;
    return message;
}

QJsonObject BrowserAction::buildResponse(const QString& action, const QJsonObject& message, const QString& nonce)
{
    QJsonObject response;
    QString encryptedMessage = encryptMessage(message, nonce);
    if (encryptedMessage.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_CANNOT_ENCRYPT_MESSAGE);
    }

    response["action"] = action;
    response["message"] = encryptedMessage;
    response["nonce"] = nonce;
    return response;
}

QString BrowserAction::getErrorMessage(const int errorCode) const
{
    switch (errorCode) {
    case ERROR_KEEPASS_DATABASE_NOT_OPENED:
        return QObject::tr("Database not opened");
    case ERROR_KEEPASS_DATABASE_HASH_NOT_RECEIVED:
        return QObject::tr("Database hash not available");
    case ERROR_KEEPASS_CLIENT_PUBLIC_KEY_NOT_RECEIVED:
        return QObject::tr("Client public key not received");
    case ERROR_KEEPASS_CANNOT_DECRYPT_MESSAGE:
        return QObject::tr("Cannot decrypt message");
    case ERROR_KEEPASS_ACTION_CANCELLED_OR_DENIED:
        return QObject::tr("Action cancelled or denied");
    case ERROR_KEEPASS_CANNOT_ENCRYPT_MESSAGE:
        return QObject::tr("Message encryption failed.");
    case ERROR_KEEPASS_ASSOCIATION_FAILED:
        return QObject::tr("KeePassXC association failed, try again");
    case ERROR_KEEPASS_ENCRYPTION_KEY_UNRECOGNIZED:
        return QObject::tr("Encryption key is not recognized");
    case ERROR_KEEPASS_INCORRECT_ACTION:
        return QObject::tr("Incorrect action");
    case ERROR_KEEPASS_EMPTY_MESSAGE_RECEIVED:
        return QObject::tr("Empty message received");
    case ERROR_KEEPASS_NO_URL_PROVIDED:
        return QObject::tr("No URL provided");
    case ERROR_KEEPASS_NO_LOGINS_FOUND:
        return QObject::tr("No logins found");
    case ERROR_KEEPASS_NO_GROUPS_FOUND:
        return QObject::tr("No groups found");
    case ERROR_KEEPASS_CANNOT_CREATE_NEW_GROUP:
        return QObject::tr("Cannot create new group");
    default:
        return QObject::tr("Unknown error");
    }
}

QString BrowserAction::encryptMessage(const QJsonObject& message, const QString& nonce)
{
    if (message.isEmpty() || nonce.isEmpty()) {
        return QString();
    }

    const QString reply(QJsonDocument(message).toJson());
    if (!reply.isEmpty()) {
        return encrypt(reply, nonce);
    }

    return QString();
}

QJsonObject BrowserAction::decryptMessage(const QString& message, const QString& nonce)
{
    if (message.isEmpty() || nonce.isEmpty()) {
        return QJsonObject();
    }

    QByteArray ba = decrypt(message, nonce);
    if (ba.isEmpty()) {
        return QJsonObject();
    }

    return getJsonObject(ba);
}

QString BrowserAction::encrypt(const QString& plaintext, const QString& nonce)
{
    const QByteArray ma = plaintext.toUtf8();
    const QByteArray na = base64Decode(nonce);
    const QByteArray ca = base64Decode(m_clientPublicKey);
    const QByteArray sa = base64Decode(m_secretKey);

    std::vector<unsigned char> m(ma.cbegin(), ma.cend());
    std::vector<unsigned char> n(na.cbegin(), na.cend());
    std::vector<unsigned char> ck(ca.cbegin(), ca.cend());
    std::vector<unsigned char> sk(sa.cbegin(), sa.cend());

    std::vector<unsigned char> e;
    e.resize(BrowserShared::NATIVEMSG_MAX_LENGTH);

    if (m.empty() || n.empty() || ck.empty() || sk.empty()) {
        return QString();
    }

    if (crypto_box_easy(e.data(), m.data(), m.size(), n.data(), ck.data(), sk.data()) == 0) {
        QByteArray res = getQByteArray(e.data(), (crypto_box_MACBYTES + ma.length()));
        return res.toBase64();
    }

    return QString();
}

QByteArray BrowserAction::decrypt(const QString& encrypted, const QString& nonce)
{
    const QByteArray ma = base64Decode(encrypted);
    const QByteArray na = base64Decode(nonce);
    const QByteArray ca = base64Decode(m_clientPublicKey);
    const QByteArray sa = base64Decode(m_secretKey);

    std::vector<unsigned char> m(ma.cbegin(), ma.cend());
    std::vector<unsigned char> n(na.cbegin(), na.cend());
    std::vector<unsigned char> ck(ca.cbegin(), ca.cend());
    std::vector<unsigned char> sk(sa.cbegin(), sa.cend());

    std::vector<unsigned char> d;
    d.resize(BrowserShared::NATIVEMSG_MAX_LENGTH);

    if (m.empty() || n.empty() || ck.empty() || sk.empty()) {
        return QByteArray();
    }

    if (crypto_box_open_easy(d.data(), m.data(), ma.length(), n.data(), ck.data(), sk.data()) == 0) {
        return getQByteArray(d.data(), std::char_traits<char>::length(reinterpret_cast<const char*>(d.data())));
    }

    return QByteArray();
}

QString BrowserAction::getBase64FromKey(const uchar* array, const uint len)
{
    return getQByteArray(array, len).toBase64();
}

QByteArray BrowserAction::getQByteArray(const uchar* array, const uint len) const
{
    QByteArray qba;
    qba.reserve(len);
    for (uint i = 0; i < len; ++i) {
        qba.append(static_cast<char>(array[i]));
    }
    return qba;
}

QJsonObject BrowserAction::getJsonObject(const uchar* pArray, const uint len) const
{
    QByteArray arr = getQByteArray(pArray, len);
    QJsonParseError err;
    QJsonDocument doc(QJsonDocument::fromJson(arr, &err));
    return doc.object();
}

QJsonObject BrowserAction::getJsonObject(const QByteArray& ba) const
{
    QJsonParseError err;
    QJsonDocument doc(QJsonDocument::fromJson(ba, &err));
    return doc.object();
}

QByteArray BrowserAction::base64Decode(const QString& str)
{
    return QByteArray::fromBase64(str.toUtf8());
}

QString BrowserAction::incrementNonce(const QString& nonce)
{
    const QByteArray nonceArray = base64Decode(nonce);
    std::vector<unsigned char> n(nonceArray.cbegin(), nonceArray.cend());

    sodium_increment(n.data(), n.size());
    return getQByteArray(n.data(), n.size()).toBase64();
}
