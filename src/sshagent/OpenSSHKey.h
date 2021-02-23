/*
 *  Copyright (C) 2017 Toni Spets <toni.spets@iki.fi>
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

#ifndef KEEPASSXC_OPENSSHKEY_H
#define KEEPASSXC_OPENSSHKEY_H

#include <QCryptographicHash>
#include <QObject>

class BinaryStream;

class OpenSSHKey : public QObject
{
    Q_OBJECT
public:
    explicit OpenSSHKey(QObject* parent = nullptr);
    OpenSSHKey(const OpenSSHKey& other);
    bool operator==(const OpenSSHKey& other) const;

    bool parsePKCS1PEM(const QByteArray& in);
    bool encrypted() const;
    bool openKey(const QString& passphrase = QString());

    const QString cipherName() const;
    const QString type() const;
    const QString fingerprint(QCryptographicHash::Algorithm algo = QCryptographicHash::Sha256) const;
    const QString comment() const;
    const QString publicKey() const;
    const QString errorString() const;

    void setType(const QString& type);
    void setPublicData(const QByteArray& data);
    void setPrivateData(const QByteArray& data);
    void setComment(const QString& comment);

    void clearPrivate();

    bool readPublic(BinaryStream& stream);
    bool readPrivate(BinaryStream& stream);
    bool writePublic(BinaryStream& stream);
    bool writePrivate(BinaryStream& stream);

    static const QString TYPE_DSA_PRIVATE;
    static const QString TYPE_RSA_PRIVATE;
    static const QString TYPE_OPENSSH_PRIVATE;

private:
    enum KeyPart
    {
        STR_PART,
        UINT8_PART
    };
    bool readKeyParts(BinaryStream& in, const QList<KeyPart> parts, BinaryStream& out);

    bool extractPEM(const QByteArray& in, QByteArray& out);

    QString m_type;
    QString m_cipherName;
    QByteArray m_cipherIV;
    QString m_kdfName;
    QByteArray m_kdfOptions;

    QString m_rawType;
    QByteArray m_rawData;
    QByteArray m_rawPublicData;
    QByteArray m_rawPrivateData;
    QString m_comment;
    QString m_error;
};

uint qHash(const OpenSSHKey& key);

#endif // KEEPASSXC_OPENSSHKEY_H
