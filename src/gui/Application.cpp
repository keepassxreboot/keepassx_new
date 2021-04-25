/*
 *  Copyright (C) 2012 Tobias Tangemann
 *  Copyright (C) 2012 Felix Geyer <debfx@fobos.de>
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

#include "Application.h"

#include "autotype/AutoType.h"
#include "core/Bootstrap.h"
#include "core/Config.h"
#include "core/Global.h"
#include "gui/Icons.h"
#include "gui/MainWindow.h"
#include "gui/MessageBox.h"
#include "gui/osutils/OSUtils.h"
#include "gui/styles/dark/DarkStyle.h"
#include "gui/styles/light/LightStyle.h"

#include <QFileInfo>
#include <QFileOpenEvent>
#include <QLockFile>
#include <QPixmapCache>
#include <QSocketNotifier>
#include <QStandardPaths>
#include <QtNetwork/QLocalSocket>

#if defined(Q_OS_UNIX)
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace
{
    constexpr int WaitTimeoutMSec = 150;
    const char BlockSizeProperty[] = "blockSize";
} // namespace

Application::Application(int& argc, char** argv)
    : QApplication(argc, argv)
#ifdef Q_OS_UNIX
    , m_unixSignalNotifier(nullptr)
#endif
    , m_alreadyRunning(false)
    , m_lockFile(nullptr)
#if defined(Q_OS_WIN) || (defined(Q_OS_UNIX) && !defined(Q_OS_MACOS))
{
#else
{
#endif
#if defined(Q_OS_UNIX)
    registerUnixSignals();
#endif

    QString userName = qgetenv("USER");
    if (userName.isEmpty()) {
        userName = qgetenv("USERNAME");
    }
    QString identifier = "keepassxc";
    if (!userName.isEmpty()) {
        identifier += "-" + userName;
    }
#ifdef QT_DEBUG
    // In DEBUG mode don't interfere with Release instances
    identifier += "-DEBUG";
#endif
    QString lockName = identifier + ".lock";
    m_socketName = identifier + ".socket";

    // According to documentation we should use RuntimeLocation on *nixes, but even Qt doesn't respect
    // this and creates sockets in TempLocation, so let's be consistent.
    m_lockFile = new QLockFile(QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/" + lockName);
    m_lockFile->setStaleLockTime(0);
    m_lockFile->tryLock();

    m_lockServer.setSocketOptions(QLocalServer::UserAccessOption);
    connect(&m_lockServer, SIGNAL(newConnection()), this, SIGNAL(anotherInstanceStarted()));
    connect(&m_lockServer, SIGNAL(newConnection()), this, SLOT(processIncomingConnection()));

    switch (m_lockFile->error()) {
    case QLockFile::NoError:
        // No existing lock was found, start listener
        m_lockServer.listen(m_socketName);
        break;
    case QLockFile::LockFailedError: {
        if (config()->get(Config::SingleInstance).toBool()) {
            // Attempt to connect to the existing instance
            QLocalSocket client;
            for (int i = 0; i < 3; ++i) {
                client.connectToServer(m_socketName);
                if (client.waitForConnected(WaitTimeoutMSec)) {
                    // Connection succeeded, this will raise the existing window if minimized
                    client.abort();
                    m_alreadyRunning = true;
                    break;
                }
            }

            if (!m_alreadyRunning) {
                // If we get here then the original instance is likely dead
                qWarning() << QObject::tr("Existing single-instance lock file is invalid. Launching new instance.")
                                  .toUtf8()
                                  .constData();

                // forceably reset the lock file
                m_lockFile->removeStaleLockFile();
                m_lockFile->tryLock();
                // start the listen server
                m_lockServer.listen(m_socketName);
            }
        }
        break;
    }
    default:
        qWarning()
            << QObject::tr("The lock file could not be created. Single-instance mode disabled.").toUtf8().constData();
    }

    connect(osUtils, &OSUtilsBase::interfaceThemeChanged, this, [this]() {
        if (config()->get(Config::GUI_ApplicationTheme).toString() != "classic") {
            applyTheme();
        }
    });
}

Application::~Application()
{
    m_lockServer.close();
    if (m_lockFile) {
        m_lockFile->unlock();
        delete m_lockFile;
    }
}

/**
 * Perform early application bootstrapping such as setting up search paths,
 * configuration OS security properties, and loading translators.
 * A QApplication object has to be instantiated before calling this function.
 */
void Application::bootstrap()
{
    Bootstrap::bootstrap();

#ifdef Q_OS_WIN
    // Qt on Windows uses "MS Shell Dlg 2" as the default font for many widgets, which resolves
    // to Tahoma 8pt, whereas the correct font would be "Segoe UI" 9pt.
    // Apparently, some widgets are already using the correct font. Thanks, MuseScore for this neat fix!
    QApplication::setFont(QApplication::font("QMessageBox"));
#endif

    osUtils->registerNativeEventFilter();
    MessageBox::initializeButtonDefs();

#ifdef Q_OS_MACOS
    // Don't show menu icons on OSX
    QApplication::setAttribute(Qt::AA_DontShowIconsInMenus);
#endif
}

void Application::applyTheme()
{
    auto appTheme = config()->get(Config::GUI_ApplicationTheme).toString();
    if (appTheme == "auto") {
        appTheme = osUtils->isDarkMode() ? "dark" : "light";
#ifdef Q_OS_WIN
        if (winUtils()->isHighContrastMode()) {
            appTheme = "classic";
        }
#endif
    }
    QPixmapCache::clear();
    if (appTheme == "light") {
        auto* s = new LightStyle;
        setPalette(s->standardPalette());
        setStyle(s);
    } else if (appTheme == "dark") {
        auto* s = new DarkStyle;
        setPalette(s->standardPalette());
        setStyle(s);
        m_darkTheme = true;
    } else {
        // Classic mode, don't check for dark theme on Windows
        // because Qt 5.x does not support it
#ifndef Q_OS_WIN
        m_darkTheme = osUtils->isDarkMode();
#endif
        QFile stylesheetFile(":/styles/base/classicstyle.qss");
        if (stylesheetFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            setStyleSheet(stylesheetFile.readAll());
            stylesheetFile.close();
        }
    }
}

bool Application::event(QEvent* event)
{
    // Handle Apple QFileOpenEvent from finder (double click on .kdbx file)
    if (event->type() == QEvent::FileOpen) {
        emit openFile(static_cast<QFileOpenEvent*>(event)->file());
        return true;
    }
#ifdef Q_OS_MACOS
    // restore main window when clicking on the docker icon
    else if (event->type() == QEvent::ApplicationActivate) {
        emit applicationActivated();
    }
#endif

    return QApplication::event(event);
}

#if defined(Q_OS_UNIX)
int Application::unixSignalSocket[2];

void Application::registerUnixSignals()
{
    int result = ::socketpair(AF_UNIX, SOCK_STREAM, 0, unixSignalSocket);
    Q_ASSERT(0 == result);
    if (0 != result) {
        // do not register handles when socket creation failed, otherwise
        // application will be unresponsive to signals such as SIGINT or SIGTERM
        return;
    }

    QVector<int> const handledSignals = {SIGQUIT, SIGINT, SIGTERM, SIGHUP};
    for (auto s : handledSignals) {
        struct sigaction sigAction;

        sigAction.sa_handler = handleUnixSignal;
        sigemptyset(&sigAction.sa_mask);
        sigAction.sa_flags = 0 | SA_RESTART;
        sigaction(s, &sigAction, nullptr);
    }

    m_unixSignalNotifier = new QSocketNotifier(unixSignalSocket[1], QSocketNotifier::Read, this);
    connect(m_unixSignalNotifier, SIGNAL(activated(int)), this, SLOT(quitBySignal()));
}

void Application::handleUnixSignal(int sig)
{
    switch (sig) {
    case SIGQUIT:
    case SIGINT:
    case SIGTERM: {
        char buf = 0;
        Q_UNUSED(::write(unixSignalSocket[0], &buf, sizeof(buf)));
        return;
    }
    case SIGHUP:
        return;
    }
}

void Application::quitBySignal()
{
    m_unixSignalNotifier->setEnabled(false);
    char buf;
    Q_UNUSED(::read(unixSignalSocket[1], &buf, sizeof(buf)));
    emit quitSignalReceived();
}
#endif

void Application::processIncomingConnection()
{
    if (m_lockServer.hasPendingConnections()) {
        QLocalSocket* socket = m_lockServer.nextPendingConnection();
        socket->setProperty(BlockSizeProperty, 0);
        connect(socket, SIGNAL(readyRead()), this, SLOT(socketReadyRead()));
    }
}

void Application::socketReadyRead()
{
    QLocalSocket* socket = qobject_cast<QLocalSocket*>(sender());
    if (!socket) {
        return;
    }

    QDataStream in(socket);
    in.setVersion(QDataStream::Qt_5_0);

    int blockSize = socket->property(BlockSizeProperty).toInt();
    if (blockSize == 0) {
        // Relies on the fact that QDataStream format streams a quint32 into sizeof(quint32) bytes
        if (socket->bytesAvailable() < qint64(sizeof(quint32))) {
            return;
        }
        in >> blockSize;
    }

    if (socket->bytesAvailable() < blockSize || in.atEnd()) {
        socket->setProperty(BlockSizeProperty, blockSize);
        return;
    }

    QStringList fileNames;
    in >> fileNames;
    for (const QString& fileName : asConst(fileNames)) {
        const QFileInfo fInfo(fileName);
        if (fInfo.isFile() && fInfo.suffix().toLower() == "kdbx") {
            emit openFile(fileName);
        }
    }
    socket->deleteLater();
}

bool Application::isAlreadyRunning() const
{
#ifdef QT_DEBUG
    // In DEBUG mode we can run unlimited instances
    return false;
#endif
    return config()->get(Config::SingleInstance).toBool() && m_alreadyRunning;
}

bool Application::sendFileNamesToRunningInstance(const QStringList& fileNames)
{
    QLocalSocket client;
    client.connectToServer(m_socketName);
    const bool connected = client.waitForConnected(WaitTimeoutMSec);
    if (!connected) {
        return false;
    }

    QByteArray data;
    QDataStream out(&data, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_5_0);
    out << quint32(0) << fileNames;
    out.device()->seek(0);
    out << quint32(data.size() - sizeof(quint32));

    const bool writeOk = client.write(data) != -1 && client.waitForBytesWritten(WaitTimeoutMSec);
    client.disconnectFromServer();
    const bool disconnected = client.waitForDisconnected(WaitTimeoutMSec);
    return writeOk && disconnected;
}

bool Application::isDarkTheme() const
{
    return m_darkTheme;
}

void Application::restart()
{
    // Disable single instance
    m_lockServer.close();
    if (m_lockFile) {
        m_lockFile->unlock();
        delete m_lockFile;
        m_lockFile = nullptr;
    }

    exit(RESTART_EXITCODE);
}
