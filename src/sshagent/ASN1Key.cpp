/*
 *  Copyright (C) 2018 Toni Spets <toni.spets@iki.fi>
 *  Copyright (C) 2018 KeePassXC Team <team@keepassxc.org>
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

#include "ASN1Key.h"
#include "BinaryStream.h"
#include "OpenSSHKey.h"

namespace
{
    constexpr quint8 TAG_INT = 0x02;
    constexpr quint8 TAG_SEQUENCE = 0x30;
    constexpr quint8 KEY_ZERO = 0x0;

    bool nextTag(BinaryStream& stream, quint8& tag, quint32& len)
    {
        stream.read(tag);

        quint8 lenByte;
        stream.read(lenByte);

        if (lenByte & 0x80) {
            quint32 bytes = lenByte & ~0x80;
            if (bytes == 1) {
                stream.read(lenByte);
                len = lenByte;
            } else if (bytes == 2) {
                quint16 lenShort;
                stream.read(lenShort);
                len = lenShort;
            } else if (bytes == 4) {
                stream.read(len);
            } else {
                return false;
            }
        } else {
            len = lenByte;
        }

        return true;
    }

    bool parsePrivateHeader(BinaryStream& stream, quint8 wantedType)
    {
        quint8 tag;
        quint32 len;

        nextTag(stream, tag, len);

        if (tag != TAG_SEQUENCE) {
            return false;
        }

        nextTag(stream, tag, len);

        if (tag != TAG_INT || len != 1) {
            return false;
        }

        quint8 keyType;
        stream.read(keyType);

        return (keyType == wantedType);
    }

    bool readInt(BinaryStream& stream, QByteArray& target)
    {
        quint8 tag;
        quint32 len;

        nextTag(stream, tag, len);

        if (tag != TAG_INT) {
            return false;
        }

        target.resize(len);
        stream.read(target);
        return true;
    }
} // namespace

bool ASN1Key::parseDSA(QByteArray& ba, OpenSSHKey& key)
{
    BinaryStream stream(&ba);

    if (!parsePrivateHeader(stream, KEY_ZERO)) {
        return false;
    }

    QByteArray p, q, g, y, x;
    readInt(stream, p);
    readInt(stream, q);
    readInt(stream, g);
    readInt(stream, y);
    readInt(stream, x);

    QByteArray publicData;
    BinaryStream publicDataStream(&publicData);
    publicDataStream.writeString(p);
    publicDataStream.writeString(q);
    publicDataStream.writeString(g);
    publicDataStream.writeString(y);

    QByteArray privateData;
    BinaryStream privateDataStream(&privateData);
    privateDataStream.writeString(p);
    privateDataStream.writeString(q);
    privateDataStream.writeString(g);
    privateDataStream.writeString(y);
    privateDataStream.writeString(x);

    key.setType("ssh-dss");
    key.setPublicData(publicData);
    key.setPrivateData(privateData);
    key.setComment("");
    return true;
}

bool ASN1Key::parseRSA(QByteArray& ba, OpenSSHKey& key)
{
    BinaryStream stream(&ba);

    if (!parsePrivateHeader(stream, KEY_ZERO)) {
        return false;
    }

    QByteArray n, e, d, p, q, dp, dq, qinv;
    readInt(stream, n);
    readInt(stream, e);
    readInt(stream, d);
    readInt(stream, p);
    readInt(stream, q);
    readInt(stream, dp);
    readInt(stream, dq);
    readInt(stream, qinv);

    // Note: To properly calculate the key fingerprint, e and n are reversed per RFC 4253
    QByteArray publicData;
    BinaryStream publicDataStream(&publicData);
    publicDataStream.writeString(e);
    publicDataStream.writeString(n);

    QByteArray privateData;
    BinaryStream privateDataStream(&privateData);
    privateDataStream.writeString(n);
    privateDataStream.writeString(e);
    privateDataStream.writeString(d);
    privateDataStream.writeString(qinv);
    privateDataStream.writeString(p);
    privateDataStream.writeString(q);

    key.setType("ssh-rsa");
    key.setPublicData(publicData);
    key.setPrivateData(privateData);
    key.setComment("");
    return true;
}
