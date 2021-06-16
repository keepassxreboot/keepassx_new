/*
 *  Copyright (C) 2010 Felix Geyer <debfx@fobos.de>
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

#include <QCommandLineParser>
#include <QFile>
#include <QTextStream>
#include <QWindow>

#include "cli/Utils.h"
#include "config-keepassx.h"
#include "core/Config.h"
#include "core/Tools.h"
#include "crypto/Crypto.h"
#include "gui/Application.h"
#include "gui/MainWindow.h"
#include "gui/MessageBox.h"
#include "gui/osutils/OSUtils.h"

#if defined(WITH_ASAN) && defined(WITH_LSAN)
#include <sanitizer/lsan_interface.h>
#endif

#ifdef QT_STATIC
#include <QtPlugin>

#if defined(Q_OS_WIN)
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
#elif defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
Q_IMPORT_PLUGIN(QXcbIntegrationPlugin)
#endif
#endif

int main(int argc, char** argv)
{
    QT_REQUIRE_VERSION(argc, argv, QT_VERSION_STR)

#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0) && defined(Q_OS_WIN)
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif
    Application app(argc, argv);
    // don't set organizationName as that changes the return value of
    // QStandardPaths::writableLocation(QDesktopServices::DataLocation)
    Application::setApplicationName("KeePassXC");
    Application::setApplicationVersion(KEEPASSXC_VERSION);
    app.setProperty("KPXC_QUALIFIED_APPNAME", "org.keepassxc.KeePassXC");

    QCommandLineParser parser;
    parser.setApplicationDescription(QObject::tr("KeePassXC - cross-platform password manager"));
    parser.addPositionalArgument(
        "filename(s)", QObject::tr("filenames of the password databases to open (*.kdbx)"), "[filename(s)]");

    QCommandLineOption configOption("config", QObject::tr("path to a custom config file"), "config");
    QCommandLineOption localConfigOption(
        "localconfig", QObject::tr("path to a custom local config file"), "localconfig");
    QCommandLineOption lockOption("lock", QObject::tr("lock all open databases"));
    QCommandLineOption keyfileOption("keyfile", QObject::tr("key file of the database"), "keyfile");
    QCommandLineOption pwstdinOption("pw-stdin", QObject::tr("read password of the database from stdin"));
    QCommandLineOption allowScreenCaptureOption("allow-screencapture",
                                                QObject::tr("allow app screen recordering and screenshots"));

    QCommandLineOption helpOption = parser.addHelpOption();
    QCommandLineOption versionOption = parser.addVersionOption();
    QCommandLineOption debugInfoOption(QStringList() << "debug-info", QObject::tr("Displays debugging information."));
    parser.addOption(configOption);
    parser.addOption(localConfigOption);
    parser.addOption(lockOption);
    parser.addOption(keyfileOption);
    parser.addOption(pwstdinOption);
    parser.addOption(debugInfoOption);

    if (osUtils->canPreventScreenCapture()) {
        parser.addOption(allowScreenCaptureOption);
    }

    parser.process(app);

    // Exit early if we're only showing the help / version
    if (parser.isSet(versionOption) || parser.isSet(helpOption)) {
        return EXIT_SUCCESS;
    }

    // Show debug information and then exit
    if (parser.isSet(debugInfoOption)) {
        QTextStream out(stdout, QIODevice::WriteOnly);
        QString debugInfo = Tools::debugInfo().append("\n").append(Crypto::debugInfo());
        out << debugInfo << endl;
        return EXIT_SUCCESS;
    }

    // Process config file options early
    if (parser.isSet(configOption) || parser.isSet(localConfigOption)) {
        Config::createConfigFromFile(parser.value(configOption), parser.value(localConfigOption));
    }

    // Process single instance and early exit if already running
    // FIXME: this is a *mess* and it is entirely my fault. --wundrweapon
    const QStringList fileNames = parser.positionalArguments();
    if (app.isAlreadyRunning()) {
        if (parser.isSet(lockOption)) {
            if (app.sendLockToInstance()) {
                qInfo() << QObject::tr("Locked databases.").toUtf8().constData();
            } else {
                qWarning() << QObject::tr("Database failed to lock.").toUtf8().constData();
                return EXIT_FAILURE;
            }
        } else {
            if (!fileNames.isEmpty()) {
                app.sendFileNamesToRunningInstance(fileNames);
            }

            qWarning() << QObject::tr("Another instance of KeePassXC is already running.").toUtf8().constData();
        }
        return EXIT_SUCCESS;
    }

    if (!Crypto::init()) {
        QString error = QObject::tr("Fatal error while testing the cryptographic functions.");
        error.append("\n");
        error.append(Crypto::errorString());
        MessageBox::critical(nullptr, QObject::tr("KeePassXC - Error"), error);
        return EXIT_FAILURE;
    }

    // Apply the configured theme before creating any GUI elements
    app.applyTheme();

#if QT_VERSION >= QT_VERSION_CHECK(5, 7, 0)
    QGuiApplication::setDesktopFileName(app.property("KPXC_QUALIFIED_APPNAME").toString() + QStringLiteral(".desktop"));
#endif

    Application::bootstrap();

    MainWindow mainWindow;

#ifndef QT_DEBUG
    // Disable screen capture if capable and not explicitly allowed
    if (osUtils->canPreventScreenCapture() && !parser.isSet(allowScreenCaptureOption)) {
        // This ensures any top-level windows (Main Window, Modal Dialogs, etc.) are excluded from screenshots
        QObject::connect(&app, &QGuiApplication::focusWindowChanged, &mainWindow, [&](QWindow* window) {
            if (window) {
                if (!osUtils->setPreventScreenCapture(window, true)) {
                    mainWindow.displayGlobalMessage(
                        QObject::tr("Warning: Failed to prevent screenshots on a top level window!"),
                        MessageWidget::Error);
                }
            }
        });
    }
#endif

    const bool pwstdin = parser.isSet(pwstdinOption);
    if (!fileNames.isEmpty() && pwstdin) {
        Utils::setDefaultTextStreams();
    }
    for (const QString& filename : fileNames) {
        QString password;
        if (pwstdin) {
            // we always need consume a line of STDIN if --pw-stdin is set to clear out the
            // buffer for native messaging, even if the specified file does not exist
            QTextStream out(stdout, QIODevice::WriteOnly);
            out << QObject::tr("Database password: ") << flush;
            password = Utils::getPassword();
        }

        if (!filename.isEmpty() && QFile::exists(filename) && !filename.endsWith(".json", Qt::CaseInsensitive)) {
            mainWindow.openDatabase(filename, password, parser.value(keyfileOption));
        }
    }

    int exitCode = Application::exec();

    // Check if restart was requested
    if (exitCode == RESTART_EXITCODE) {
        QProcess::startDetached(QCoreApplication::applicationFilePath(), {});
    }

#if defined(WITH_ASAN) && defined(WITH_LSAN)
    // do leak check here to prevent massive tail of end-of-process leak errors from third-party libraries
    __lsan_do_leak_check();
    __lsan_disable();
#endif

    return exitCode;
}
