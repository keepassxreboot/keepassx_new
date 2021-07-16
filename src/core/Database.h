/*
 *  Copyright (C) 2018 KeePassXC Team <team@keepassxc.org>
 *  Copyright (C) 2010 Felix Geyer <debfx@fobos.de>
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

#ifndef KEEPASSX_DATABASE_H
#define KEEPASSX_DATABASE_H

#include <QDateTime>
#include <QHash>
#include <QMutex>
#include <QPointer>
#include <QTimer>

#include "config-keepassx.h"
#include "core/ModifiableObject.h"
#include "crypto/kdf/AesKdf.h"
#include "format/KeePass2.h"
#include "keys/CompositeKey.h"
#include "keys/PasswordKey.h"

class Entry;
enum class EntryReferenceType;
class FileWatcher;
class Group;
class Metadata;
class QIODevice;

struct DeletedObject
{
    QUuid uuid;
    QDateTime deletionTime;
    bool operator==(const DeletedObject& other) const
    {
        return uuid == other.uuid && deletionTime == other.deletionTime;
    }
};

Q_DECLARE_TYPEINFO(DeletedObject, Q_MOVABLE_TYPE);

class Database : public ModifiableObject
{
    Q_OBJECT

public:
    enum CompressionAlgorithm
    {
        CompressionNone = 0,
        CompressionGZip = 1
    };
    static const quint32 CompressionAlgorithmMax = CompressionGZip;

    enum SaveFlag
    {
        Atomic = 0x0, // Saves are transactional and atomic
        NonAtomic = 0x1, // Save operation may be interrupted
        Backup = 0x2, // Make copy of original database before saving
        DirectWrite = 0x4, // Directly write to the file (dangerous)
    };
    Q_DECLARE_FLAGS(SaveFlags, SaveFlag);

    Database();
    explicit Database(const QString& filePath);
    ~Database() override;

    bool open(QSharedPointer<const CompositeKey> key, QString* error = nullptr, bool readOnly = false);
    bool open(const QString& filePath,
              QSharedPointer<const CompositeKey> key,
              QString* error = nullptr,
              bool readOnly = false);
    bool save(SaveFlags flags = Atomic, QString* error = nullptr);
    bool saveAs(const QString& filePath, SaveFlags flags = Atomic, QString* error = nullptr);
    bool extract(QByteArray&, QString* error = nullptr);
    bool import(const QString& xmlExportPath, QString* error = nullptr);

    void releaseData();

    bool isInitialized() const;
    bool isModified() const;
    bool hasNonDataChanges() const;
    bool isReadOnly() const;
    void setReadOnly(bool readOnly);
    bool isSaving();

    QUuid uuid() const;
    QString filePath() const;
    QString canonicalFilePath() const;
    void setFilePath(const QString& filePath);

    Metadata* metadata();
    const Metadata* metadata() const;
    Group* rootGroup();
    const Group* rootGroup() const;
    void setRootGroup(Group* group);
    QVariantMap& publicCustomData();
    const QVariantMap& publicCustomData() const;
    void setPublicCustomData(const QVariantMap& customData);

    void recycleGroup(Group* group);
    void recycleEntry(Entry* entry);
    void emptyRecycleBin();
    QList<DeletedObject> deletedObjects();
    const QList<DeletedObject>& deletedObjects() const;
    void addDeletedObject(const DeletedObject& delObj);
    void addDeletedObject(const QUuid& uuid);
    bool containsDeletedObject(const QUuid& uuid) const;
    bool containsDeletedObject(const DeletedObject& uuid) const;
    void setDeletedObjects(const QList<DeletedObject>& delObjs);

    QList<QString> commonUsernames();

    QSharedPointer<const CompositeKey> key() const;
    bool setKey(const QSharedPointer<const CompositeKey>& key,
                bool updateChangedTime = true,
                bool updateTransformSalt = false,
                bool transformKey = true);
    QString keyError();
    QByteArray challengeResponseKey() const;
    bool challengeMasterSeed(const QByteArray& masterSeed);
    const QUuid& cipher() const;
    void setCipher(const QUuid& cipher);
    Database::CompressionAlgorithm compressionAlgorithm() const;
    void setCompressionAlgorithm(Database::CompressionAlgorithm algo);

    QSharedPointer<Kdf> kdf() const;
    void setKdf(QSharedPointer<Kdf> kdf);
    bool changeKdf(const QSharedPointer<Kdf>& kdf);
    QByteArray transformedDatabaseKey() const;

    static Database* databaseByUuid(const QUuid& uuid);

public slots:
    void markAsModified();
    void markAsClean();
    void updateCommonUsernames(int topN = 10);
    void markNonDataChange();

signals:
    void filePathChanged(const QString& oldPath, const QString& newPath);
    void groupDataChanged(Group* group);
    void groupAboutToAdd(Group* group, int index);
    void groupAdded();
    void groupAboutToRemove(Group* group);
    void groupRemoved();
    void groupAboutToMove(Group* group, Group* toGroup, int index);
    void groupMoved();
    void databaseOpened();
    void databaseSaved();
    void databaseDiscarded();
    void databaseFileChanged();

private:
    struct DatabaseData
    {
        QString filePath;
        bool isReadOnly = false;
        QUuid cipher = KeePass2::CIPHER_AES256;
        CompressionAlgorithm compressionAlgorithm = CompressionGZip;

        QScopedPointer<PasswordKey> masterSeed;
        QScopedPointer<PasswordKey> transformedDatabaseKey;
        QScopedPointer<PasswordKey> challengeResponseKey;

        QSharedPointer<const CompositeKey> key;
        QSharedPointer<Kdf> kdf = QSharedPointer<AesKdf>::create(true);

        QVariantMap publicCustomData;

        DatabaseData()
            : masterSeed(new PasswordKey())
            , transformedDatabaseKey(new PasswordKey())
            , challengeResponseKey(new PasswordKey())
        {
            kdf->randomizeSeed();
        }

        void clear()
        {
            filePath.clear();

            masterSeed.reset();
            transformedDatabaseKey.reset();
            challengeResponseKey.reset();

            key.reset();
            kdf.reset();

            publicCustomData.clear();
        }
    };

    void createRecycleBin();

    bool writeDatabase(QIODevice* device, QString* error = nullptr);
    bool backupDatabase(const QString& filePath);
    bool restoreDatabase(const QString& filePath);
    bool performSave(const QString& filePath, SaveFlags flags, QString* error);
    void startModifiedTimer();
    void stopModifiedTimer();

    QPointer<Metadata> const m_metadata;
    DatabaseData m_data;
    QPointer<Group> m_rootGroup;
    QList<DeletedObject> m_deletedObjects;
    QTimer m_modifiedTimer;
    QMutex m_saveMutex;
    QPointer<FileWatcher> m_fileWatcher;
    bool m_modified = false;
    bool m_hasNonDataChange = false;
    QString m_keyError;

    QList<QString> m_commonUsernames;

    QUuid m_uuid;
    static QHash<QUuid, QPointer<Database>> s_uuidMap;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Database::SaveFlags);

#endif // KEEPASSX_DATABASE_H
