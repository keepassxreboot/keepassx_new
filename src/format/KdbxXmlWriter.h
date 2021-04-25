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

#ifndef KEEPASSX_KDBXXMLWRITER_H
#define KEEPASSX_KDBXXMLWRITER_H

#include <QDateTime>
#include <QXmlStreamWriter>

#include "core/Database.h"
#include "core/Entry.h"
#include "core/Group.h"
#include "core/TimeInfo.h"

class KeePass2RandomStream;
class Metadata;

class KdbxXmlWriter
{
public:
    explicit KdbxXmlWriter(quint32 version);

    void writeDatabase(QIODevice* device,
                       const Database* db,
                       KeePass2RandomStream* randomStream = nullptr,
                       const QByteArray& headerHash = QByteArray());
    void writeDatabase(const QString& filename, Database* db);
    void disableInnerStreamProtection(bool disable);
    bool innerStreamProtectionDisabled() const;
    bool hasError();
    QString errorString();

private:
    void generateIdMap();

    void writeMetadata();
    void writeMemoryProtection();
    void writeCustomIcons();
    void writeIcon(const QUuid& uuid, const QByteArray& icon);
    void writeBinaries();
    void writeCustomData(const CustomData* customData);
    void writeCustomDataItem(const QString& key, const QString& value);
    void writeRoot();
    void writeGroup(const Group* group);
    void writeTimes(const TimeInfo& ti);
    void writeDeletedObjects();
    void writeDeletedObject(const DeletedObject& delObj);
    void writeEntry(const Entry* entry);
    void writeAutoType(const Entry* entry);
    void writeAutoTypeAssoc(const AutoTypeAssociations::Association& assoc);
    void writeEntryHistory(const Entry* entry);

    void writeString(const QString& qualifiedName, const QString& string);
    void writeNumber(const QString& qualifiedName, int number);
    void writeBool(const QString& qualifiedName, bool b);
    void writeDateTime(const QString& qualifiedName, const QDateTime& dateTime);
    void writeUuid(const QString& qualifiedName, const QUuid& uuid);
    void writeUuid(const QString& qualifiedName, const Group* group);
    void writeUuid(const QString& qualifiedName, const Entry* entry);
    void writeBinary(const QString& qualifiedName, const QByteArray& ba);
    void writeTriState(const QString& qualifiedName, Group::TriState triState);
    QString colorPartToString(int value);
    QString stripInvalidXml10Chars(QString str);

    void raiseError(const QString& errorMessage);

    const quint32 m_kdbxVersion;

    bool m_innerStreamProtectionDisabled = false;

    QXmlStreamWriter m_xml;
    QPointer<const Database> m_db;
    QPointer<const Metadata> m_meta;
    KeePass2RandomStream* m_randomStream = nullptr;
    QHash<QByteArray, int> m_idMap;
    QByteArray m_headerHash;

    bool m_error = false;

    QString m_errorStr = "";
};

#endif // KEEPASSX_KDBXXMLWRITER_H
