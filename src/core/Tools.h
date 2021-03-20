/*
 *  Copyright (C) 2012 Felix Geyer <debfx@fobos.de>
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

#ifndef KEEPASSX_TOOLS_H
#define KEEPASSX_TOOLS_H

#include "core/Global.h"

#include <QObject>
#include <QProcessEnvironment>
#include <QString>
#include <QUuid>

#include <algorithm>

class QIODevice;
class QRegularExpression;

namespace Tools
{
    QString debugInfo();
    QString humanReadableFileSize(qint64 bytes, quint32 precision = 2);
    bool readFromDevice(QIODevice* device, QByteArray& data, int size = 16384);
    bool readAllFromDevice(QIODevice* device, QByteArray& data);
    QString imageReaderFilter();
    bool isHex(const QByteArray& ba);
    bool isBase64(const QByteArray& ba);
    void sleep(int ms);
    void wait(int ms);
    bool checkUrlValid(const QString& urlField);
    QString uuidToHex(const QUuid& uuid);
    QUuid hexToUuid(const QString& uuid);
    QRegularExpression convertToRegex(const QString& string,
                                      bool useWildcards = false,
                                      bool exactMatch = false,
                                      bool caseSensitive = false);
    QString envSubstitute(const QString& filepath,
                          QProcessEnvironment environment = QProcessEnvironment::systemEnvironment());

    template <typename RandomAccessIterator, typename T>
    RandomAccessIterator binaryFind(RandomAccessIterator begin, RandomAccessIterator end, const T& value)
    {
        RandomAccessIterator it = std::lower_bound(begin, end, value);

        if ((it == end) || (value < *it)) {
            return end;
        } else {
            return it;
        }
    }

    template <typename Key, typename Value, void deleter(Value)> struct Map
    {
        QMap<Key, Value> values;
        Value& operator[](const Key index)
        {
            return values[index];
        }

        ~Map()
        {
            for (Value m : values) {
                deleter(m);
            }
        }
    };

    struct Buffer
    {
        unsigned char* raw;
        size_t size;

        Buffer();
        ~Buffer();

        void clear();
        QByteArray content() const;
    };

    inline int qtRuntimeVersion()
    {
        // Cache the result since the Qt version can't change during
        // the execution, computing it once will be enough
        const static int version = []() {
            const auto sq = QString::fromLatin1(qVersion());
            return (sq.section(QChar::fromLatin1('.'), 0, 0).toInt() << 16)
                   + (sq.section(QChar::fromLatin1('.'), 1, 1).toInt() << 8)
                   + (sq.section(QChar::fromLatin1('.'), 2, 2).toInt());
        }();

        return version;
    }
} // namespace Tools

#endif // KEEPASSX_TOOLS_H
