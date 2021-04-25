/*
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

#include "TestKeePass1Reader.h"
#include "TestGlobal.h"

#include <QBuffer>

#include "config-keepassx-tests.h"
#include "core/Metadata.h"
#include "crypto/Crypto.h"
#include "format/KeePass1Reader.h"
#include "format/KeePass2Reader.h"
#include "format/KeePass2Writer.h"
#include "keys/FileKey.h"
#include "keys/PasswordKey.h"

QTEST_GUILESS_MAIN(TestKeePass1Reader)

void TestKeePass1Reader::initTestCase()
{
    QVERIFY(Crypto::init());

    QString filename = QString(KEEPASSX_TEST_DATA_DIR).append("/basic.kdb");

    KeePass1Reader reader;
    m_db = reader.readDatabase(filename, "masterpw", 0);
    QVERIFY(m_db);
    QVERIFY(!reader.hasError());
}

void TestKeePass1Reader::testBasic()
{
    QVERIFY(m_db->deletedObjects().isEmpty());

    QCOMPARE(m_db->rootGroup()->children().size(), 2);

    Group* group1 = m_db->rootGroup()->children().at(0);
    QVERIFY(!group1->uuid().isNull());
    QCOMPARE(group1->name(), QString("Internet"));
    QCOMPARE(group1->children().size(), 2);
    QCOMPARE(group1->entries().size(), 2);
    QCOMPARE(group1->iconNumber(), 1);

    Entry* entry11 = group1->entries().at(0);
    QVERIFY(!entry11->uuid().isNull());
    QCOMPARE(entry11->title(), QString("Test entry"));
    QCOMPARE(entry11->iconNumber(), 1);
    QCOMPARE(entry11->username(), QString("I"));
    QCOMPARE(entry11->url(), QString("http://example.com/"));
    QCOMPARE(entry11->password(), QString("secretpassword"));
    QCOMPARE(entry11->notes(), QString("Lorem ipsum\ndolor sit amet"));
    QVERIFY(entry11->timeInfo().expires());
    QCOMPARE(entry11->timeInfo().expiryTime(), genDT(2012, 5, 9, 10, 32));
    QCOMPARE(entry11->attachments()->keys().size(), 1);
    QCOMPARE(entry11->attachments()->keys().at(0), QString("attachment.txt"));
    QCOMPARE(entry11->attachments()->value("attachment.txt"), QByteArray("hello world\n"));

    Entry* entry12 = group1->entries().at(1);
    QCOMPARE(entry12->title(), QString(""));
    QCOMPARE(entry12->iconNumber(), 0);
    QCOMPARE(entry12->username(), QString(""));
    QCOMPARE(entry12->url(), QString(""));
    QCOMPARE(entry12->password(), QString(""));
    QCOMPARE(entry12->notes(), QString(""));
    QVERIFY(!entry12->timeInfo().expires());
    QCOMPARE(entry12->attachments()->keys().size(), 0);

    Group* group11 = group1->children().at(0);
    QCOMPARE(group11->name(), QString("Subgroup 1"));
    QCOMPARE(group11->children().size(), 1);

    Group* group111 = group11->children().at(0);
    QCOMPARE(group111->name(), QString("Unexpanded"));
    QCOMPARE(group111->children().size(), 1);

    Group* group1111 = group111->children().at(0);
    QCOMPARE(group1111->name(), QString("abc"));
    QCOMPARE(group1111->children().size(), 0);

    Group* group12 = group1->children().at(1);
    QCOMPARE(group12->name(), QString("Subgroup 2"));
    QCOMPARE(group12->children().size(), 0);

    Group* group2 = m_db->rootGroup()->children().at(1);
    QCOMPARE(group2->name(), QString("eMail"));
    QCOMPARE(group2->entries().size(), 1);
    QCOMPARE(group2->iconNumber(), 19);

    reopenDatabase(m_db, "masterpw", QString());
}

void TestKeePass1Reader::testMasterKey()
{
    QVERIFY(m_db->isInitialized());
    QCOMPARE(m_db->kdf()->rounds(), 713);
}

void TestKeePass1Reader::testCustomIcons()
{
    QCOMPARE(m_db->metadata()->customIconsOrder().size(), 1);
    QUuid uuid = m_db->metadata()->customIconsOrder().at(0);
    QVERIFY(m_db->metadata()->hasCustomIcon(uuid));
    QByteArray icon = m_db->metadata()->customIcon(uuid);

    QVERIFY(icon.startsWith(
        "\x89PNG\r\n\x1A\n\x00\x00\x00\rIHDR\x00\x00\x00\x10\x00\x00\x00\x10\b\x06\x00\x00\x00\x1F\xF3\xFF"));
}

void TestKeePass1Reader::testGroupExpanded()
{
    QCOMPARE(m_db->rootGroup()->children().at(0)->isExpanded(), true);
    QCOMPARE(m_db->rootGroup()->children().at(0)->children().at(0)->isExpanded(), true);
    QCOMPARE(m_db->rootGroup()->children().at(0)->children().at(0)->children().at(0)->isExpanded(), false);
}

void TestKeePass1Reader::testAutoType()
{
    Group* group = m_db->rootGroup()->children().at(0)->children().at(0);
    QCOMPARE(group->entries().size(), 2);

    Entry* entry1 = group->entries().at(0);
    QCOMPARE(entry1->notes(), QString("last line"));
    QCOMPARE(entry1->defaultAutoTypeSequence(), QString("{USERNAME}{ENTER}"));
    QCOMPARE(entry1->autoTypeAssociations()->size(), 5);
    QCOMPARE(entry1->autoTypeAssociations()->get(0).sequence, QString(""));
    QCOMPARE(entry1->autoTypeAssociations()->get(0).window, QString("a window"));
    QCOMPARE(entry1->autoTypeAssociations()->get(1).sequence, QString(""));
    QCOMPARE(entry1->autoTypeAssociations()->get(1).window, QString("a second window"));
    QCOMPARE(entry1->autoTypeAssociations()->get(2).sequence, QString("{PASSWORD}{ENTER}"));
    QCOMPARE(entry1->autoTypeAssociations()->get(2).window, QString("Window Nr 1a"));
    QCOMPARE(entry1->autoTypeAssociations()->get(3).sequence, QString("{PASSWORD}{ENTER}"));
    QCOMPARE(entry1->autoTypeAssociations()->get(3).window, QString("Window Nr 1b"));
    QCOMPARE(entry1->autoTypeAssociations()->get(4).sequence, QString(""));
    QCOMPARE(entry1->autoTypeAssociations()->get(4).window, QString("Window 2"));

    Entry* entry2 = group->entries().at(1);
    QCOMPARE(entry2->notes(), QString("start line\nend line"));
    QCOMPARE(entry2->defaultAutoTypeSequence(), QString(""));
    QCOMPARE(entry2->autoTypeAssociations()->size(), 2);
    QCOMPARE(entry2->autoTypeAssociations()->get(0).sequence, QString(""));
    QCOMPARE(entry2->autoTypeAssociations()->get(0).window, QString("Main Window"));
    QCOMPARE(entry2->autoTypeAssociations()->get(1).sequence, QString(""));
    QCOMPARE(entry2->autoTypeAssociations()->get(1).window, QString("Test Window"));
}

void TestKeePass1Reader::testFileKey()
{
    QFETCH(QString, type);

    QString name = QString("FileKey").append(type);

    KeePass1Reader reader;

    QString dbFilename = QString("%1/%2.kdb").arg(QString(KEEPASSX_TEST_DATA_DIR), name);
    QString keyFilename = QString("%1/%2.key").arg(QString(KEEPASSX_TEST_DATA_DIR), name);

    auto db = reader.readDatabase(dbFilename, QString(), keyFilename);
    QVERIFY(db);
    QVERIFY(!reader.hasError());
    QCOMPARE(db->rootGroup()->children().size(), 1);
    QCOMPARE(db->rootGroup()->children().at(0)->name(), name);

    reopenDatabase(db, QString(), keyFilename);
}

void TestKeePass1Reader::testFileKey_data()
{
    QTest::addColumn<QString>("type");
    QTest::newRow("Binary") << QString("Binary");
    QTest::newRow("Hex") << QString("Hex");
    QTest::newRow("Hashed") << QString("Hashed");
}

void TestKeePass1Reader::testCompositeKey()
{
    QString name = "CompositeKey";

    KeePass1Reader reader;

    QString dbFilename = QString("%1/%2.kdb").arg(QString(KEEPASSX_TEST_DATA_DIR), name);
    QString keyFilename = QString("%1/FileKeyHex.key").arg(QString(KEEPASSX_TEST_DATA_DIR));

    auto db = reader.readDatabase(dbFilename, "mypassword", keyFilename);
    QVERIFY(db);
    QVERIFY(!reader.hasError());
    QCOMPARE(db->rootGroup()->children().size(), 1);
    QCOMPARE(db->rootGroup()->children().at(0)->name(), name);

    reopenDatabase(db, "mypassword", keyFilename);
}

void TestKeePass1Reader::testTwofish()
{
    QString name = "Twofish";

    KeePass1Reader reader;

    QString dbFilename = QString("%1/%2.kdb").arg(QString(KEEPASSX_TEST_DATA_DIR), name);

    auto db = reader.readDatabase(dbFilename, "masterpw", 0);
    QVERIFY(db);
    QVERIFY(!reader.hasError());
    QCOMPARE(db->rootGroup()->children().size(), 1);
    QCOMPARE(db->rootGroup()->children().at(0)->name(), name);
}

void TestKeePass1Reader::testCP1252Password()
{
    QString name = "CP-1252";

    KeePass1Reader reader;

    QString dbFilename = QString("%1/%2.kdb").arg(QString(KEEPASSX_TEST_DATA_DIR), name);
    QString password = QString::fromUtf8("\xe2\x80\x9e\x70\x61\x73\x73\x77\x6f\x72\x64\xe2\x80\x9d");

    auto db = reader.readDatabase(dbFilename, password, 0);
    QVERIFY(db);
    QVERIFY(!reader.hasError());
    QCOMPARE(db->rootGroup()->children().size(), 1);
    QCOMPARE(db->rootGroup()->children().at(0)->name(), name);
}

void TestKeePass1Reader::cleanupTestCase()
{
}

QDateTime TestKeePass1Reader::genDT(int year, int month, int day, int hour, int min)
{
    QDate date(year, month, day);
    QTime time(hour, min, 0);
    return QDateTime(date, time, Qt::UTC);
}

void TestKeePass1Reader::reopenDatabase(QSharedPointer<Database> db,
                                        const QString& password,
                                        const QString& keyfileName)
{
    QBuffer buffer;
    buffer.open(QIODevice::ReadWrite);

    KeePass2Writer writer;
    writer.writeDatabase(&buffer, db.data());
    QVERIFY(!writer.hasError());
    QVERIFY(buffer.seek(0));

    auto key = QSharedPointer<CompositeKey>::create();
    if (!password.isEmpty()) {
        key->addKey(QSharedPointer<PasswordKey>::create(password));
    }
    if (!keyfileName.isEmpty()) {
        auto fileKey = QSharedPointer<FileKey>::create();
        QVERIFY(fileKey->load(keyfileName));
        key->addKey(fileKey);
    }

    KeePass2Reader reader;
    auto newDb = QSharedPointer<Database>::create();
    QVERIFY(reader.readDatabase(&buffer, key, newDb.data()));
    QVERIFY(!reader.hasError());
}
