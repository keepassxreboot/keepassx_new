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

#include "BrowserHost.h"
#include "BrowserSettings.h"
#include "BrowserShared.h"

#include <QJsonDocument>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMutexLocker>
#include <QtNetwork>

#include <iostream>

#ifdef Q_OS_WIN
#include <fcntl.h>
#undef NOMINMAX
#define NOMINMAX
#include <windows.h>
#else
#include <sys/socket.h>
#include <sys/types.h>
#endif

BrowserHost::BrowserHost(QObject* parent)
    : QObject(parent)
{
    m_localServer = new QLocalServer(this);
    m_localServer->setSocketOptions(QLocalServer::UserAccessOption);
    connect(m_localServer.data(), SIGNAL(newConnection()), this, SLOT(proxyConnected()));
}

BrowserHost::~BrowserHost()
{
    stop();
}

void BrowserHost::start()
{
    if (!m_localServer->isListening()) {
        m_localServer->listen(BrowserShared::localServerPath());
    }
}

void BrowserHost::stop()
{
    m_socketList.clear();
    m_localServer->close();
}

void BrowserHost::proxyConnected()
{
    auto socket = m_localServer->nextPendingConnection();
    if (socket) {
        m_socketList.append(socket);
        connect(socket, SIGNAL(readyRead()), this, SLOT(readProxyMessage()));
        connect(socket, SIGNAL(disconnected()), this, SLOT(proxyDisconnected()));
    }
}

void BrowserHost::readProxyMessage()
{
    QLocalSocket* socket = qobject_cast<QLocalSocket*>(QObject::sender());
    if (!socket || socket->bytesAvailable() <= 0) {
        return;
    }

    socket->setReadBufferSize(BrowserShared::NATIVEMSG_MAX_LENGTH);
    int socketDesc = socket->socketDescriptor();
    if (socketDesc) {
        int max = BrowserShared::NATIVEMSG_MAX_LENGTH;
        setsockopt(socketDesc, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<char*>(&max), sizeof(max));
    }

    QJsonParseError error;
    auto json = QJsonDocument::fromJson(socket->readAll(), &error);
    if (json.isNull()) {
        qWarning() << "Failed to read proxy message: " << error.errorString();
        return;
    }

    emit clientMessageReceived(json.object());
}

void BrowserHost::sendClientMessage(const QJsonObject& json)
{
    QString reply(QJsonDocument(json).toJson(QJsonDocument::Compact));
    for (const auto socket : m_socketList) {
        if (socket && socket->isValid() && socket->state() == QLocalSocket::ConnectedState) {
            QByteArray arr = reply.toUtf8();
            socket->write(arr.constData(), arr.length());
            socket->flush();
        }
    }
}

void BrowserHost::proxyDisconnected()
{
    auto socket = qobject_cast<QLocalSocket*>(QObject::sender());
    m_socketList.removeOne(socket);
}
