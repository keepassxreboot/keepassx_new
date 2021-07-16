/*
 *  Copyright (C) 2017 Vladimir Svyatski <v.unreal@gmail.com>
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

#include "TestDatabase.h"

#include <QRegularExpression>
#include <QSignalSpy>
#include <QTest>

#include "config-keepassx-tests.h"
#include "core/Group.h"
#include "core/Metadata.h"
#include "core/Tools.h"
#include "crypto/Crypto.h"
#include "format/KeePass2Writer.h"
#include "keys/PasswordKey.h"
#include "util/TemporaryFile.h"

QTEST_GUILESS_MAIN(TestDatabase)

static QString dbFileName = QStringLiteral(KEEPASSX_TEST_DATA_DIR).append("/NewDatabase.kdbx");

void TestDatabase::initTestCase()
{
    QVERIFY(Crypto::init());
}

void TestDatabase::testOpen()
{
    auto db = QSharedPointer<Database>::create();
    QVERIFY(!db->isInitialized());
    QVERIFY(!db->isModified());

    auto key = QSharedPointer<CompositeKey>::create();
    key->addKey(QSharedPointer<PasswordKey>::create("a"));

    bool ok = db->open(dbFileName, key);
    QVERIFY(ok);

    QVERIFY(db->isInitialized());
    QVERIFY(!db->isModified());

    db->metadata()->setName("test");
    QVERIFY(db->isModified());
}

void TestDatabase::testSave()
{
    TemporaryFile tempFile;
    QVERIFY(tempFile.copyFromFile(dbFileName));

    auto db = QSharedPointer<Database>::create();
    auto key = QSharedPointer<CompositeKey>::create();
    key->addKey(QSharedPointer<PasswordKey>::create("a"));

    QString error;
    bool ok = db->open(tempFile.fileName(), key, &error);
    QVERIFY(ok);

    // Test safe saves
    db->metadata()->setName("test");
    QVERIFY(db->isModified());
    QVERIFY2(db->save(Database::Atomic, &error), error.toLatin1());
    QVERIFY(!db->isModified());

    // Test temp-file saves
    db->metadata()->setName("test2");
    QVERIFY2(db->save(Database::NonAtomic, &error), error.toLatin1());
    QVERIFY(!db->isModified());

    // Test direct-write saves
    db->metadata()->setName("test3");
    QVERIFY2(db->save(Database::NonAtomic | Database::DirectWrite, &error), error.toLatin1());
    QVERIFY(!db->isModified());

    // Test save backups
    db->metadata()->setName("test4");
    QVERIFY2(db->save(Database::Atomic | Database::Backup, &error), error.toLatin1());
    QVERIFY(!db->isModified());

    // Confirm backup exists and then delete it
    auto re = QRegularExpression("(\\.[^.]+)$");
    auto match = re.match(tempFile.fileName());
    auto backupFilePath = tempFile.fileName();
    backupFilePath = backupFilePath.replace(re, "") + ".old" + match.captured(1);
    QVERIFY(QFile::exists(backupFilePath));
    QFile::remove(backupFilePath);
    QVERIFY(!QFile::exists(backupFilePath));
}

void TestDatabase::testSignals()
{
    TemporaryFile tempFile;
    QVERIFY(tempFile.copyFromFile(dbFileName));

    auto db = QSharedPointer<Database>::create();
    auto key = QSharedPointer<CompositeKey>::create();
    key->addKey(QSharedPointer<PasswordKey>::create("a"));

    QSignalSpy spyFilePathChanged(db.data(), SIGNAL(filePathChanged(const QString&, const QString&)));
    QString error;
    bool ok = db->open(tempFile.fileName(), key, &error);
    QVERIFY(ok);
    QCOMPARE(spyFilePathChanged.count(), 1);

    QSignalSpy spyModified(db.data(), SIGNAL(modified()));
    db->metadata()->setName("test1");
    QTRY_COMPARE(spyModified.count(), 1);

    QSignalSpy spySaved(db.data(), SIGNAL(databaseSaved()));
    QVERIFY(db->save(Database::Atomic, &error));
    QCOMPARE(spySaved.count(), 1);

    // Short delay to allow file system settling to reduce test failures
    Tools::wait(100);

    QSignalSpy spyFileChanged(db.data(), SIGNAL(databaseFileChanged()));
    QVERIFY(tempFile.copyFromFile(dbFileName));
    QTRY_COMPARE(spyFileChanged.count(), 1);
    QTRY_VERIFY(!db->isModified());

    db->metadata()->setName("test2");
    QTRY_VERIFY(db->isModified());

    QSignalSpy spyDiscarded(db.data(), SIGNAL(databaseDiscarded()));
    QVERIFY(db->open(tempFile.fileName(), key, &error));
    QCOMPARE(spyDiscarded.count(), 1);
}

void TestDatabase::testEmptyRecycleBinOnDisabled()
{
    QString filename = QString(KEEPASSX_TEST_DATA_DIR).append("/RecycleBinDisabled.kdbx");
    auto key = QSharedPointer<CompositeKey>::create();
    key->addKey(QSharedPointer<PasswordKey>::create("123"));
    auto db = QSharedPointer<Database>::create();
    QVERIFY(db->open(filename, key, nullptr, false));

    // Explicitly mark DB as read-write in case it was opened from a read-only drive.
    // Prevents assertion failures on CI systems when the data dir is not writable
    db->setReadOnly(false);

    QSignalSpy spyModified(db.data(), SIGNAL(modified()));

    db->emptyRecycleBin();
    // The database must be unmodified in this test after emptying the recycle bin.
    QTRY_COMPARE(spyModified.count(), 0);
}

void TestDatabase::testEmptyRecycleBinOnNotCreated()
{
    QString filename = QString(KEEPASSX_TEST_DATA_DIR).append("/RecycleBinNotYetCreated.kdbx");
    auto key = QSharedPointer<CompositeKey>::create();
    key->addKey(QSharedPointer<PasswordKey>::create("123"));
    auto db = QSharedPointer<Database>::create();
    QVERIFY(db->open(filename, key, nullptr, false));
    db->setReadOnly(false);

    QSignalSpy spyModified(db.data(), SIGNAL(modified()));

    db->emptyRecycleBin();
    // The database must be unmodified in this test after emptying the recycle bin.
    QTRY_COMPARE(spyModified.count(), 0);
}

void TestDatabase::testEmptyRecycleBinOnEmpty()
{
    QString filename = QString(KEEPASSX_TEST_DATA_DIR).append("/RecycleBinEmpty.kdbx");
    auto key = QSharedPointer<CompositeKey>::create();
    key->addKey(QSharedPointer<PasswordKey>::create("123"));
    auto db = QSharedPointer<Database>::create();
    QVERIFY(db->open(filename, key, nullptr, false));
    db->setReadOnly(false);

    QSignalSpy spyModified(db.data(), SIGNAL(modified()));

    db->emptyRecycleBin();
    // The database must be unmodified in this test after emptying the recycle bin.
    QTRY_COMPARE(spyModified.count(), 0);
}

void TestDatabase::testEmptyRecycleBinWithHierarchicalData()
{
    QString filename = QString(KEEPASSX_TEST_DATA_DIR).append("/RecycleBinWithData.kdbx");
    auto key = QSharedPointer<CompositeKey>::create();
    key->addKey(QSharedPointer<PasswordKey>::create("123"));
    auto db = QSharedPointer<Database>::create();
    QVERIFY(db->open(filename, key, nullptr, false));
    db->setReadOnly(false);

    QFile originalFile(filename);
    qint64 initialSize = originalFile.size();

    db->emptyRecycleBin();
    QVERIFY(db->metadata()->recycleBin());
    QVERIFY(db->metadata()->recycleBin()->entries().empty());
    QVERIFY(db->metadata()->recycleBin()->children().empty());

    QTemporaryFile afterCleanup;
    afterCleanup.open();

    KeePass2Writer writer;
    writer.writeDatabase(&afterCleanup, db.data());
    QVERIFY(afterCleanup.size() < initialSize);
}
