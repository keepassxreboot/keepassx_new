/*
 *  Copyright (C) 2012 Felix Geyer <debfx@fobos.de>
 *  Copyright (C) 2017 Lennart Glauer <mail@lennart-glauer.de>
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

#include "Tools.h"

#include "config-keepassx.h"
#include "core/Config.h"
#include "core/Translator.h"

#include "git-info.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QIODevice>
#include <QImageReader>
#include <QLocale>
#include <QRegularExpression>
#include <QStringList>
#include <QSysInfo>
#include <QUrl>
#include <QUuid>
#include <cctype>

#ifdef Q_OS_WIN
#include <windows.h> // for Sleep()
#endif

#ifdef Q_OS_UNIX
#include <time.h> // for nanosleep()
#endif

namespace Tools
{
    QString debugInfo()
    {
        QString debugInfo = "KeePassXC - ";
        debugInfo.append(QObject::tr("Version %1").arg(KEEPASSXC_VERSION).append("\n"));
#ifndef KEEPASSXC_BUILD_TYPE_RELEASE
        debugInfo.append(QObject::tr("Build Type: %1").arg(KEEPASSXC_BUILD_TYPE).append("\n"));
#endif

        QString commitHash;
        if (!QString(GIT_HEAD).isEmpty()) {
            commitHash = GIT_HEAD;
        }
        if (!commitHash.isEmpty()) {
            debugInfo.append(QObject::tr("Revision: %1").arg(commitHash.left(7)).append("\n"));
        }

#ifdef KEEPASSXC_DIST
        debugInfo.append(QObject::tr("Distribution: %1").arg(KEEPASSXC_DIST_TYPE).append("\n"));
#endif

        // Qt related debugging information.
        debugInfo.append("\n");
        debugInfo.append("Qt ").append(QString::fromLocal8Bit(qVersion())).append("\n");
#ifdef QT_NO_DEBUG
        debugInfo.append(QObject::tr("Debugging mode is disabled.").append("\n"));
#else
        debugInfo.append(QObject::tr("Debugging mode is enabled.").append("\n"));
#endif
        debugInfo.append("\n");

#if QT_VERSION >= QT_VERSION_CHECK(5, 4, 0)
        debugInfo.append(QObject::tr("Operating system: %1\nCPU architecture: %2\nKernel: %3 %4")
                             .arg(QSysInfo::prettyProductName(),
                                  QSysInfo::currentCpuArchitecture(),
                                  QSysInfo::kernelType(),
                                  QSysInfo::kernelVersion()));

        debugInfo.append("\n\n");
#endif

        QString extensions;
#ifdef WITH_XC_AUTOTYPE
        extensions += "\n- " + QObject::tr("Auto-Type");
#endif
#ifdef WITH_XC_BROWSER
        extensions += "\n- " + QObject::tr("Browser Integration");
#endif
#ifdef WITH_XC_SSHAGENT
        extensions += "\n- " + QObject::tr("SSH Agent");
#endif
#if defined(WITH_XC_KEESHARE_SECURE) && defined(WITH_XC_KEESHARE_INSECURE)
        extensions += "\n- " + QObject::tr("KeeShare (signed and unsigned sharing)");
#elif defined(WITH_XC_KEESHARE_SECURE)
        extensions += "\n- " + QObject::tr("KeeShare (only signed sharing)");
#elif defined(WITH_XC_KEESHARE_INSECURE)
        extensions += "\n- " + QObject::tr("KeeShare (only unsigned sharing)");
#endif
#ifdef WITH_XC_YUBIKEY
        extensions += "\n- " + QObject::tr("YubiKey");
#endif
#ifdef WITH_XC_TOUCHID
        extensions += "\n- " + QObject::tr("TouchID");
#endif
#ifdef WITH_XC_FDOSECRETS
        extensions += "\n- " + QObject::tr("Secret Service Integration");
#endif

        if (extensions.isEmpty()) {
            extensions = " " + QObject::tr("None");
        }

        debugInfo.append(QObject::tr("Enabled extensions:").append(extensions).append("\n"));
        return debugInfo;
    }

    QString humanReadableFileSize(qint64 bytes, quint32 precision)
    {
        constexpr auto kibibyte = 1024;
        double size = bytes;

        QStringList units = QStringList() << "B"
                                          << "KiB"
                                          << "MiB"
                                          << "GiB";
        int i = 0;
        int maxI = units.size() - 1;

        while ((size >= kibibyte) && (i < maxI)) {
            size /= kibibyte;
            i++;
        }

        return QString("%1 %2").arg(QLocale().toString(size, 'f', precision), units.at(i));
    }

    bool readFromDevice(QIODevice* device, QByteArray& data, int size)
    {
        QByteArray buffer;
        buffer.resize(size);

        qint64 readResult = device->read(buffer.data(), size);
        if (readResult == -1) {
            return false;
        } else {
            buffer.resize(readResult);
            data = buffer;
            return true;
        }
    }

    bool readAllFromDevice(QIODevice* device, QByteArray& data)
    {
        QByteArray result;
        qint64 readBytes = 0;
        qint64 readResult;
        do {
            result.resize(result.size() + 16384);
            readResult = device->read(result.data() + readBytes, result.size() - readBytes);
            if (readResult > 0) {
                readBytes += readResult;
            }
        } while (readResult > 0);

        if (readResult == -1) {
            return false;
        } else {
            result.resize(static_cast<int>(readBytes));
            data = result;
            return true;
        }
    }

    QString imageReaderFilter()
    {
        const QList<QByteArray> formats = QImageReader::supportedImageFormats();
        QStringList formatsStringList;

        for (const QByteArray& format : formats) {
            for (char codePoint : format) {
                if (!QChar(codePoint).isLetterOrNumber()) {
                    continue;
                }
            }

            formatsStringList.append("*." + QString::fromLatin1(format).toLower());
        }

        return formatsStringList.join(" ");
    }

    bool isHex(const QByteArray& ba)
    {
        for (const uchar c : ba) {
            if (!std::isxdigit(c)) {
                return false;
            }
        }

        return true;
    }

    bool isBase64(const QByteArray& ba)
    {
        constexpr auto pattern = R"(^(?:[a-z0-9+/]{4})*(?:[a-z0-9+/]{3}=|[a-z0-9+/]{2}==)?$)";
        QRegExp regexp(pattern, Qt::CaseInsensitive, QRegExp::RegExp2);

        QString base64 = QString::fromLatin1(ba.constData(), ba.size());

        return regexp.exactMatch(base64);
    }

    void sleep(int ms)
    {
        Q_ASSERT(ms >= 0);

        if (ms == 0) {
            return;
        }

#ifdef Q_OS_WIN
        Sleep(uint(ms));
#else
        timespec ts;
        ts.tv_sec = ms / 1000;
        ts.tv_nsec = (ms % 1000) * 1000 * 1000;
        nanosleep(&ts, nullptr);
#endif
    }

    void wait(int ms)
    {
        Q_ASSERT(ms >= 0);

        if (ms == 0) {
            return;
        }

        QElapsedTimer timer;
        timer.start();

        if (ms <= 50) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, ms);
            sleep(qMax(ms - static_cast<int>(timer.elapsed()), 0));
        } else {
            int timeLeft;
            do {
                timeLeft = ms - timer.elapsed();
                if (timeLeft > 0) {
                    QCoreApplication::processEvents(QEventLoop::AllEvents, timeLeft);
                    sleep(10);
                }
            } while (!timer.hasExpired(ms));
        }
    }

    bool checkUrlValid(const QString& urlField)
    {
        if (urlField.isEmpty() || urlField.startsWith("cmd://", Qt::CaseInsensitive)
            || urlField.startsWith("{REF:A", Qt::CaseInsensitive)) {
            return true;
        }

        QUrl url;
        if (urlField.contains("://")) {
            url = urlField;
        } else {
            url = QUrl::fromUserInput(urlField);
        }

        if (url.scheme() != "file" && url.host().isEmpty()) {
            return false;
        }

        // Check for illegal characters. Adds also the wildcard * to the list
        QRegularExpression re("[<>\\^`{|}\\*]");
        auto match = re.match(urlField);
        if (match.hasMatch()) {
            return false;
        }

        return true;
    }

    // Escape common regex symbols except for *, ?, and |
    auto regexEscape = QRegularExpression(R"re(([-[\]{}()+.,\\\/^$#]))re");

    QRegularExpression convertToRegex(const QString& string, bool useWildcards, bool exactMatch, bool caseSensitive)
    {
        QString pattern = string;

        // Wildcard support (*, ?, |)
        if (useWildcards) {
            pattern.replace(regexEscape, "\\\\1");
            pattern.replace("*", ".*");
            pattern.replace("?", ".");
        }

        // Exact modifier
        if (exactMatch) {
            pattern = "^" + pattern + "$";
        }

        auto regex = QRegularExpression(pattern);
        if (!caseSensitive) {
            regex.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
        }

        return regex;
    }

    QString uuidToHex(const QUuid& uuid)
    {
        return QString::fromLatin1(uuid.toRfc4122().toHex());
    }

    QUuid hexToUuid(const QString& uuid)
    {
        return QUuid::fromRfc4122(QByteArray::fromHex(uuid.toLatin1()));
    }

    QString envSubstitute(const QString& filepath, QProcessEnvironment environment)
    {
        QString subbed = filepath;

#if defined(Q_OS_WIN)
        QRegularExpression varRe("\\%([A-Za-z][A-Za-z0-9_]*)\\%");
        QString homeEnv = "USERPROFILE";
#else
        QRegularExpression varRe("\\$([A-Za-z][A-Za-z0-9_]*)");
        QString homeEnv = "HOME";
#endif

        if (subbed.startsWith("~/") || subbed.startsWith("~\\"))
            subbed.replace(0, 1, environment.value(homeEnv));

        QRegularExpressionMatch match;

        do {
            match = varRe.match(subbed);
            if (match.hasMatch()) {
                subbed.replace(match.capturedStart(), match.capturedLength(), environment.value(match.captured(1)));
            }
        } while (match.hasMatch());

        return subbed;
    }

    Buffer::Buffer()
        : raw(nullptr)
        , size(0)
    {
    }

    Buffer::~Buffer()
    {
        clear();
    }

    void Buffer::clear()
    {
        if (size > 0) {
            free(raw);
        }
        raw = nullptr;
        size = 0;
    }

    QByteArray Buffer::content() const
    {
        return QByteArray(reinterpret_cast<char*>(raw), size);
    }

} // namespace Tools
