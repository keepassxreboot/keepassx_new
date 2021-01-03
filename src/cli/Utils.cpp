/*
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

#include "Utils.h"

#ifdef WITH_XC_YUBIKEY
#include "keys/YkChallengeResponseKeyCLI.h"
#endif

#ifdef WITH_XC_LEDGER
#include "keys/LedgerKey.h"
#include "keys/drivers/LedgerHardwareKey.h"
#endif

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

#include <QFileInfo>
#include <QProcess>
#include <QScopedPointer>

namespace Utils
{
    QTextStream STDOUT;
    QTextStream STDERR;
    QTextStream STDIN;
    QTextStream DEVNULL;

    void setDefaultTextStreams()
    {
        auto fd = new QFile();
        fd->open(stdout, QIODevice::WriteOnly);
        STDOUT.setDevice(fd);

        fd = new QFile();
        fd->open(stderr, QIODevice::WriteOnly);
        STDERR.setDevice(fd);

        fd = new QFile();
        fd->open(stdin, QIODevice::ReadOnly);
        STDIN.setDevice(fd);

        fd = new QFile();
#ifdef Q_OS_WIN
        fd->open(fopen("nul", "w"), QIODevice::WriteOnly);
#else
        fd->open(fopen("/dev/null", "w"), QIODevice::WriteOnly);
#endif
        DEVNULL.setDevice(fd);
    }

    void setStdinEcho(bool enable = true)
    {
#ifdef Q_OS_WIN
        HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
        DWORD mode;
        GetConsoleMode(hIn, &mode);

        if (enable) {
            mode |= ENABLE_ECHO_INPUT;
        } else {
            mode &= ~ENABLE_ECHO_INPUT;
        }

        SetConsoleMode(hIn, mode);
#else
        struct termios t;
        tcgetattr(STDIN_FILENO, &t);

        if (enable) {
            t.c_lflag |= ECHO;
        } else {
            t.c_lflag &= ~ECHO;
        }

        tcsetattr(STDIN_FILENO, TCSANOW, &t);
#endif
    }

    QSharedPointer<Database> unlockDatabase(const QString& databaseFilename,
                                            const bool isPasswordProtected,
                                            const QString& keyFilename,
                                            const QString& yubiKeySlot,
                                            const QString& ledgerKey,
                                            bool quiet)
    {
        auto& err = quiet ? DEVNULL : STDERR;
        auto compositeKey = QSharedPointer<CompositeKey>::create();

        QFileInfo dbFileInfo(databaseFilename);
        if (dbFileInfo.canonicalFilePath().isEmpty()) {
            err << QObject::tr("Failed to open database file %1: not found").arg(databaseFilename) << endl;
            return {};
        }

        if (!dbFileInfo.isFile()) {
            err << QObject::tr("Failed to open database file %1: not a plain file").arg(databaseFilename) << endl;
            return {};
        }

        if (!dbFileInfo.isReadable()) {
            err << QObject::tr("Failed to open database file %1: not readable").arg(databaseFilename) << endl;
            return {};
        }

        if (isPasswordProtected) {
            err << QObject::tr("Enter password to unlock %1: ").arg(databaseFilename) << flush;
            QString line = Utils::getPassword(quiet);
            auto passwordKey = QSharedPointer<PasswordKey>::create();
            passwordKey->setPassword(line);
            compositeKey->addKey(passwordKey);
        }

        if (!keyFilename.isEmpty()) {
            auto fileKey = QSharedPointer<FileKey>::create();
            QString errorMessage;
            // LCOV_EXCL_START
            if (!fileKey->load(keyFilename, &errorMessage)) {
                err << QObject::tr("Failed to load key file %1: %2").arg(keyFilename, errorMessage) << endl;
                return {};
            }

            if (fileKey->type() != FileKey::KeePass2XMLv2 && fileKey->type() != FileKey::Hashed) {
                err << QObject::tr("WARNING: You are using an old key file format which KeePassXC may\n"
                                   "stop supporting in the future.\n\n"
                                   "Please consider generating a new key file.")
                    << endl;
            }
            // LCOV_EXCL_STOP

            compositeKey->addKey(fileKey);
        }

#ifdef WITH_XC_YUBIKEY
        if (!yubiKeySlot.isEmpty()) {
            unsigned int serial = 0;
            int slot;

            bool ok = false;
            auto parts = yubiKeySlot.split(":");
            slot = parts[0].toInt(&ok);

            if (!ok || (slot != 1 && slot != 2)) {
                err << QObject::tr("Invalid YubiKey slot %1").arg(parts[0]) << endl;
                return {};
            }

            if (parts.size() > 1) {
                serial = parts[1].toUInt(&ok, 10);
                if (!ok) {
                    err << QObject::tr("Invalid YubiKey serial %1").arg(parts[1]) << endl;
                    return {};
                }
            }

            auto key = QSharedPointer<YkChallengeResponseKeyCLI>(new YkChallengeResponseKeyCLI(
                {serial, slot}, QObject::tr("Please touch the button on your YubiKey to continue…"), err));
            compositeKey->addChallengeResponseKey(key);
        }
#else
        Q_UNUSED(yubiKeySlot);
#endif // WITH_XC_YUBIKEY

#ifdef WITH_XC_LEDGER
        if (!ledgerKey.isEmpty()) {
            QSharedPointer<LedgerKey> key;
            QString errMsg;
            if (!loadLedgerKey(ledgerKey, key, errMsg, quiet)) {
                err << QObject::tr("Unable to load key from ledger device: ") << errMsg << "." << endl;
                return {};
            }
            compositeKey->addKey(key);
        }
#else
        Q_UNUSED(ledgerKey);
#endif

        auto db = QSharedPointer<Database>::create();
        QString error;
        if (db->open(databaseFilename, compositeKey, &error, false)) {
            return db;
        } else {
            err << error << endl;
            return {};
        }
    }

    /**
     * Read a user password from STDIN or return a password previously
     * set by \link setNextPassword().
     *
     * @return the password
     */
    QString getPassword(bool quiet)
    {
#ifdef __AFL_COMPILER
        // Fuzz test build takes password from environment variable to
        // allow non-interactive operation
        const auto env = getenv("KEYPASSXC_AFL_PASSWORD");
        return env ? env : "";
#else
        auto& in = STDIN;
        auto& out = quiet ? DEVNULL : STDERR;

        setStdinEcho(false);
        QString line = in.readLine();
        setStdinEcho(true);
        out << endl;

        return line;
#endif // __AFL_COMPILER
    }

    /**
     * Read optional password from stdin.
     *
     * @return Pointer to the PasswordKey or null if passwordkey is skipped
     *         by user
     */
    QSharedPointer<PasswordKey> getConfirmedPassword()
    {
        auto& err = STDERR;
        auto& in = STDIN;

        QSharedPointer<PasswordKey> passwordKey;

        err << QObject::tr("Enter password to encrypt database (optional): ");
        err.flush();
        auto password = Utils::getPassword();

        if (password.isEmpty()) {
            err << QObject::tr("Do you want to create a database with an empty password? [y/N]: ");
            err.flush();
            auto ans = in.readLine();
            if (ans.toLower().startsWith("y")) {
                passwordKey = QSharedPointer<PasswordKey>::create("");
            }
            err << endl;
        } else {
            err << QObject::tr("Repeat password: ");
            err.flush();
            auto repeat = Utils::getPassword();

            if (password == repeat) {
                passwordKey = QSharedPointer<PasswordKey>::create(password);
            } else {
                err << QObject::tr("Error: Passwords do not match.") << endl;
            }
        }

        return passwordKey;
    }

    /**
     * A valid and running event loop is needed to use the global QClipboard,
     * so we need to use this from the CLI.
     */
    int clipText(const QString& text)
    {
        auto& err = STDERR;

        // List of programs and their arguments
        QList<QPair<QString, QString>> clipPrograms;

#ifdef Q_OS_UNIX
        if (QProcessEnvironment::systemEnvironment().contains("WAYLAND_DISPLAY")) {
            clipPrograms << qMakePair(QStringLiteral("wl-copy"), QStringLiteral(""));
        } else {
            clipPrograms << qMakePair(QStringLiteral("xclip"), QStringLiteral("-selection clipboard -i"));
        }
#endif

#ifdef Q_OS_MACOS
        clipPrograms << qMakePair(QStringLiteral("pbcopy"), QStringLiteral(""));
#endif

#ifdef Q_OS_WIN
        clipPrograms << qMakePair(QStringLiteral("clip"), QStringLiteral(""));
#endif

        if (clipPrograms.isEmpty()) {
            err << QObject::tr("No program defined for clipboard manipulation");
            err.flush();
            return EXIT_FAILURE;
        }

        QStringList failedProgramNames;

        for (auto prog : clipPrograms) {
            QScopedPointer<QProcess> clipProcess(new QProcess(nullptr));

            // Skip empty parts, otherwise the program may clip the empty string
            QStringList progArgs = prog.second.split(" ", QString::SkipEmptyParts);

            clipProcess->start(prog.first, progArgs);
            clipProcess->waitForStarted();

            if (clipProcess->state() != QProcess::Running) {
                failedProgramNames.append(prog.first);
                continue;
            }

            if (clipProcess->write(text.toLatin1()) == -1) {
                qDebug("Unable to write to process : %s", qPrintable(clipProcess->errorString()));
            }
            clipProcess->waitForBytesWritten();
            clipProcess->closeWriteChannel();
            clipProcess->waitForFinished();

            if (clipProcess->exitCode() == EXIT_SUCCESS) {
                return EXIT_SUCCESS;
            }
        }

        // No clipping program worked
        err << QObject::tr("All clipping programs failed. Tried %1\n").arg(failedProgramNames.join(", "));
        err.flush();
        return EXIT_FAILURE;
    }

    /**
     * Splits the given QString into a QString list. For example:
     *
     * "hello world" -> ["hello", "world"]
     * "hello    world" -> ["hello", "world"]
     * "hello\\ world" -> ["hello world"] (i.e. backslash is an escape character
     * "\"hello world\"" -> ["hello world"]
     */
    QStringList splitCommandString(const QString& command)
    {
        QStringList result;

        bool insideQuotes = false;
        QString cur;
        for (int i = 0; i < command.size(); ++i) {
            QChar c = command[i];
            if (c == '\\' && i < command.size() - 1) {
                cur.append(command[i + 1]);
                ++i;
            } else if (!insideQuotes && (c == ' ' || c == '\t')) {
                if (!cur.isEmpty()) {
                    result.append(cur);
                    cur.clear();
                }
            } else if (c == '"' && (insideQuotes || i == 0 || command[i - 1].isSpace())) {
                insideQuotes = !insideQuotes;
            } else {
                cur.append(c);
            }
        }

        if (!cur.isEmpty()) {
            result.append(cur);
        }

        return result;
    }

    QStringList findAttributes(const EntryAttributes& attributes, const QString& name)
    {
        QStringList result;
        if (attributes.hasKey(name)) {
            result.append(name);
            return result;
        }

        for (const QString& key : attributes.keys()) {
            if (key.compare(name, Qt::CaseSensitivity::CaseInsensitive) == 0) {
                result.append(key);
            }
        }

        return result;
    }

    /**
     * Load a key file from disk. When the path specified does not exist a
     * new file will be generated. No folders will be generated so the parent
     * folder of the specified file needs to exist
     *
     * If the key file cannot be loaded or created the function will fail.
     *
     * @param path Path to the key file to be loaded
     * @param fileKey Resulting fileKey
     * @return true if the key file was loaded succesfully
     */
    bool loadFileKey(const QString& path, QSharedPointer<FileKey>& fileKey)
    {
        auto& err = Utils::STDERR;
        QString error;
        fileKey = QSharedPointer<FileKey>(new FileKey());

        if (!QFileInfo::exists(path)) {
            fileKey->create(path, &error);

            if (!error.isEmpty()) {
                err << QObject::tr("Creating KeyFile %1 failed: %2").arg(path, error) << endl;
                return false;
            }
        }

        if (!fileKey->load(path, &error)) {
            err << QObject::tr("Loading KeyFile %1 failed: %2").arg(path, error) << endl;
            return false;
        }

        return true;
    }

#ifdef WITH_XC_LEDGER
    bool initLedgerKey()
    {
        auto& Hardware = LedgerHardwareKey::instance();
        QFuture<bool> Ret = Hardware.findDevices();
        Ret.waitForFinished();
        return Ret.result();
    }

    bool loadLedgerKey(const QString& ledgerKey, QSharedPointer<LedgerKey>& out, QString& errMsg, bool quiet)
    {
        // Format:
        // * slot:XX, to use a key on a slot
        // * name:str, to derive the key from a user-given name
        auto& err = quiet ? DEVNULL : STDERR;
        QStringList Parts = ledgerKey.split(':');
        if (Parts.size() != 2) {
            return false;
        }
        if (!initLedgerKey()) {
            return false;
        }
        auto Devices = LedgerHardwareKey::instance().foundDevices();
        if (Devices.size() == 0) {
            return false;
        }
        auto& DevInfo = Devices[0];
        auto& Dev = *DevInfo.Dev;
        QString Type = Parts[0];
        QString Value = Parts[1];
        QString AcceptMsg =
            QObject::tr("Please accept accessing the database key on the Ledger device '%1'").arg(DevInfo.Name);
        if (Type == "slot") {
            bool ok;
            int Slot = Value.toInt(&ok);
            if (!ok || Slot < 0) {
                return false;
            }
            err << AcceptMsg << "\n" << flush;
            out = LedgerKey::fromDeviceSlot(Dev, Slot, errMsg);
            return !out.isNull();
        }
        if (Type == "name") {
            err << AcceptMsg << "\n" << flush;
            out = LedgerKey::fromDeviceDeriveName(Dev, Value, errMsg);
            return !out.isNull();
        }
        return false;
    }
#endif
} // namespace Utils
