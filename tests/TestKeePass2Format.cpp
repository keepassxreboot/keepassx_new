/*
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

#include "TestKeePass2Format.h"
#include "TestGlobal.h"
#include "mock/MockClock.h"

#include "core/Metadata.h"
#include "crypto/Crypto.h"
#include "format/KdbxXmlReader.h"
#include "keys/FileKey.h"
#include "keys/PasswordKey.h"
#include "mock/MockChallengeResponseKey.h"

#include "FailDevice.h"
#include "config-keepassx-tests.h"

void TestKeePass2Format::initTestCase()
{
    QVERIFY(Crypto::init());

    // read raw XML database
    bool hasError;
    QString errorString;
    m_xmlDb = readXml(QString(KEEPASSX_TEST_DATA_DIR).append("/NewDatabase.xml"), true, hasError, errorString);
    if (hasError) {
        QFAIL(qPrintable(QString("Error while reading XML: ").append(errorString)));
    }
    QVERIFY(m_xmlDb.data());

    // construct and write KDBX to buffer
    auto key = QSharedPointer<CompositeKey>::create();
    key->addKey(QSharedPointer<PasswordKey>::create("test"));

    m_kdbxSourceDb = QSharedPointer<Database>::create();
    m_kdbxSourceDb->setKey(key);
    m_kdbxSourceDb->metadata()->setName("TESTDB");
    Group* group = m_kdbxSourceDb->rootGroup();
    group->setUuid(QUuid::createUuid());
    group->setNotes("I'm a note!");
    auto entry = new Entry();
    entry->setPassword(QString::fromUtf8("\xc3\xa4\xa3\xb6\xc3\xbc\xe9\x9b\xbb\xe7\xb4\x85"));
    entry->setUuid(QUuid::createUuid());
    entry->attributes()->set("test", "protectedTest", true);
    QVERIFY(entry->attributes()->isProtected("test"));
    entry->attachments()->set("myattach.txt", QByteArray("this is an attachment"));
    entry->attachments()->set("aaa.txt", QByteArray("also an attachment"));
    entry->setGroup(group);
    auto groupNew = new Group();
    groupNew->setUuid(QUuid::createUuid());
    groupNew->setName("TESTGROUP");
    groupNew->setNotes("I'm a sub group note!");
    groupNew->setParent(group);

    m_kdbxTargetBuffer.open(QBuffer::ReadWrite);
    writeKdbx(&m_kdbxTargetBuffer, m_kdbxSourceDb.data(), hasError, errorString);
    if (hasError) {
        QFAIL(qPrintable(QString("Error while writing database: ").append(errorString)));
    }

    // call sub class init method
    initTestCaseImpl();
}

void TestKeePass2Format::testXmlMetadata()
{
    QCOMPARE(m_xmlDb->metadata()->generator(), QString("KeePass"));
    QCOMPARE(m_xmlDb->metadata()->name(), QString("ANAME"));
    QCOMPARE(m_xmlDb->metadata()->nameChanged(), MockClock::datetimeUtc(2010, 8, 8, 17, 24, 53));
    QCOMPARE(m_xmlDb->metadata()->description(), QString("ADESC"));
    QCOMPARE(m_xmlDb->metadata()->descriptionChanged(), MockClock::datetimeUtc(2010, 8, 8, 17, 27, 12));
    QCOMPARE(m_xmlDb->metadata()->defaultUserName(), QString("DEFUSERNAME"));
    QCOMPARE(m_xmlDb->metadata()->defaultUserNameChanged(), MockClock::datetimeUtc(2010, 8, 8, 17, 27, 45));
    QCOMPARE(m_xmlDb->metadata()->maintenanceHistoryDays(), 127);
    QCOMPARE(m_xmlDb->metadata()->color(), QString("#FFEF00"));
    QCOMPARE(m_xmlDb->metadata()->databaseKeyChanged(), MockClock::datetimeUtc(2012, 4, 5, 17, 9, 34));
    QCOMPARE(m_xmlDb->metadata()->databaseKeyChangeRec(), 101);
    QCOMPARE(m_xmlDb->metadata()->databaseKeyChangeForce(), -1);
    QCOMPARE(m_xmlDb->metadata()->protectTitle(), false);
    QCOMPARE(m_xmlDb->metadata()->protectUsername(), true);
    QCOMPARE(m_xmlDb->metadata()->protectPassword(), false);
    QCOMPARE(m_xmlDb->metadata()->protectUrl(), true);
    QCOMPARE(m_xmlDb->metadata()->protectNotes(), false);
    QCOMPARE(m_xmlDb->metadata()->recycleBinEnabled(), true);
    QVERIFY(m_xmlDb->metadata()->recycleBin() != nullptr);
    QCOMPARE(m_xmlDb->metadata()->recycleBin()->name(), QString("Recycle Bin"));
    QCOMPARE(m_xmlDb->metadata()->recycleBinChanged(), MockClock::datetimeUtc(2010, 8, 25, 16, 12, 57));
    QVERIFY(m_xmlDb->metadata()->entryTemplatesGroup() == nullptr);
    QCOMPARE(m_xmlDb->metadata()->entryTemplatesGroupChanged(), MockClock::datetimeUtc(2010, 8, 8, 17, 24, 19));
    QVERIFY(m_xmlDb->metadata()->lastSelectedGroup() != nullptr);
    QCOMPARE(m_xmlDb->metadata()->lastSelectedGroup()->name(), QString("NewDatabase"));
    QVERIFY(m_xmlDb->metadata()->lastTopVisibleGroup() == m_xmlDb->metadata()->lastSelectedGroup());
    QCOMPARE(m_xmlDb->metadata()->historyMaxItems(), -1);
    QCOMPARE(m_xmlDb->metadata()->historyMaxSize(), 5242880);
}

void TestKeePass2Format::testXmlCustomIcons()
{
    QCOMPARE(m_xmlDb->metadata()->customIconsOrder().size(), 1);
    QUuid uuid = QUuid::fromRfc4122(QByteArray::fromBase64("++vyI+daLk6omox4a6kQGA=="));
    QVERIFY(m_xmlDb->metadata()->hasCustomIcon(uuid));
    QByteArray icon = m_xmlDb->metadata()->customIcon(uuid);

    QVERIFY(icon.startsWith(
        "\x89PNG\r\n\x1A\n\x00\x00\x00\rIHDR\x00\x00\x00\x10\x00\x00\x00\x10\b\x06\x00\x00\x00\x1F\xF3\xFF"));
}

void TestKeePass2Format::testXmlGroupRoot()
{
    const Group* group = m_xmlDb->rootGroup();
    QVERIFY(group);
    QCOMPARE(group->uuid(), QUuid::fromRfc4122(QByteArray::fromBase64("lmU+9n0aeESKZvcEze+bRg==")));
    QCOMPARE(group->name(), QString("NewDatabase"));
    QCOMPARE(group->notes(), QString(""));
    QCOMPARE(group->iconNumber(), 49);
    QCOMPARE(group->iconUuid(), QUuid());
    QVERIFY(group->isExpanded());
    TimeInfo ti = group->timeInfo();
    QCOMPARE(ti.lastModificationTime(), MockClock::datetimeUtc(2010, 8, 8, 17, 24, 27));
    QCOMPARE(ti.creationTime(), MockClock::datetimeUtc(2010, 8, 7, 17, 24, 27));
    QCOMPARE(ti.lastAccessTime(), MockClock::datetimeUtc(2010, 8, 9, 9, 9, 44));
    QCOMPARE(ti.expiryTime(), MockClock::datetimeUtc(2010, 8, 8, 17, 24, 17));
    QVERIFY(!ti.expires());
    QCOMPARE(ti.usageCount(), 52);
    QCOMPARE(ti.locationChanged(), MockClock::datetimeUtc(2010, 8, 8, 17, 24, 27));
    QCOMPARE(group->defaultAutoTypeSequence(), QString(""));
    QCOMPARE(group->autoTypeEnabled(), Group::Inherit);
    QCOMPARE(group->searchingEnabled(), Group::Inherit);
    QCOMPARE(group->lastTopVisibleEntry()->uuid(),
             QUuid::fromRfc4122(QByteArray::fromBase64("+wSUOv6qf0OzW8/ZHAs2sA==")));
    QCOMPARE(group->children().size(), 3);
    QVERIFY(m_xmlDb->metadata()->recycleBin() == m_xmlDb->rootGroup()->children().at(2));

    QCOMPARE(group->entries().size(), 2);
}

void TestKeePass2Format::testXmlGroup1()
{
    const Group* group = m_xmlDb->rootGroup()->children().at(0);

    QCOMPARE(group->uuid(), QUuid::fromRfc4122(QByteArray::fromBase64("AaUYVdXsI02h4T1RiAlgtg==")));
    QCOMPARE(group->name(), QString("General"));
    QCOMPARE(group->notes(), QString("Group Notez"));
    QCOMPARE(group->iconNumber(), 48);
    QCOMPARE(group->iconUuid(), QUuid());
    QCOMPARE(group->isExpanded(), true);
    QCOMPARE(group->defaultAutoTypeSequence(), QString("{Password}{ENTER}"));
    QCOMPARE(group->autoTypeEnabled(), Group::Enable);
    QCOMPARE(group->searchingEnabled(), Group::Disable);
    QVERIFY(!group->lastTopVisibleEntry());
}

void TestKeePass2Format::testXmlGroup2()
{
    const Group* group = m_xmlDb->rootGroup()->children().at(1);

    QCOMPARE(group->uuid(), QUuid::fromRfc4122(QByteArray::fromBase64("1h4NtL5DK0yVyvaEnN//4A==")));
    QCOMPARE(group->name(), QString("Windows"));
    QCOMPARE(group->isExpanded(), false);

    QCOMPARE(group->children().size(), 1);
    const Group* child = group->children().first();

    QCOMPARE(child->uuid(), QUuid::fromRfc4122(QByteArray::fromBase64("HoYE/BjLfUSW257pCHJ/eA==")));
    QCOMPARE(child->name(), QString("Subsub"));
    QCOMPARE(child->entries().size(), 1);

    const Entry* entry = child->entries().first();
    QCOMPARE(entry->uuid(), QUuid::fromRfc4122(QByteArray::fromBase64("GZpdQvGXOU2kaKRL/IVAGg==")));
    QCOMPARE(entry->title(), QString("Subsub Entry"));
}

void TestKeePass2Format::testXmlEntry1()
{
    const Entry* entry = m_xmlDb->rootGroup()->entries().at(0);

    QCOMPARE(entry->uuid(), QUuid::fromRfc4122(QByteArray::fromBase64("+wSUOv6qf0OzW8/ZHAs2sA==")));
    QCOMPARE(entry->historyItems().size(), 2);
    QCOMPARE(entry->iconNumber(), 0);
    QCOMPARE(entry->iconUuid(), QUuid());
    QVERIFY(entry->foregroundColor().isEmpty());
    QVERIFY(entry->backgroundColor().isEmpty());
    QCOMPARE(entry->overrideUrl(), QString(""));
    QCOMPARE(entry->tags(), QString("a b c"));

    const TimeInfo ti = entry->timeInfo();
    QCOMPARE(ti.lastModificationTime(), MockClock::datetimeUtc(2010, 8, 25, 16, 19, 25));
    QCOMPARE(ti.creationTime(), MockClock::datetimeUtc(2010, 8, 25, 16, 13, 54));
    QCOMPARE(ti.lastAccessTime(), MockClock::datetimeUtc(2010, 8, 25, 16, 19, 25));
    QCOMPARE(ti.expiryTime(), MockClock::datetimeUtc(2010, 8, 25, 16, 12, 57));
    QVERIFY(!ti.expires());
    QCOMPARE(ti.usageCount(), 8);
    QCOMPARE(ti.locationChanged(), MockClock::datetimeUtc(2010, 8, 25, 16, 13, 54));

    QList<QString> attrs = entry->attributes()->keys();
    QCOMPARE(entry->attributes()->value("Notes"), QString("Notes"));
    QVERIFY(!entry->attributes()->isProtected("Notes"));
    QVERIFY(attrs.removeOne("Notes"));
    QCOMPARE(entry->attributes()->value("Password"), QString("Password"));
    QVERIFY(!entry->attributes()->isProtected("Password"));
    QVERIFY(attrs.removeOne("Password"));
    QCOMPARE(entry->attributes()->value("Title"), QString("Sample Entry 1"));
    QVERIFY(!entry->attributes()->isProtected("Title"));
    QVERIFY(attrs.removeOne("Title"));
    QCOMPARE(entry->attributes()->value("URL"), QString(""));
    QVERIFY(entry->attributes()->isProtected("URL"));
    QVERIFY(attrs.removeOne("URL"));
    QCOMPARE(entry->attributes()->value("UserName"), QString("User Name"));
    QVERIFY(entry->attributes()->isProtected("UserName"));
    QVERIFY(attrs.removeOne("UserName"));
    QVERIFY(attrs.isEmpty());

    QCOMPARE(entry->title(), entry->attributes()->value("Title"));
    QCOMPARE(entry->url(), entry->attributes()->value("URL"));
    QCOMPARE(entry->username(), entry->attributes()->value("UserName"));
    QCOMPARE(entry->password(), entry->attributes()->value("Password"));
    QCOMPARE(entry->notes(), entry->attributes()->value("Notes"));

    QCOMPARE(entry->attachments()->keys().size(), 1);
    QCOMPARE(entry->attachments()->value("myattach.txt"), QByteArray("abcdefghijk"));
    QCOMPARE(entry->historyItems().at(0)->attachments()->keys().size(), 1);
    QCOMPARE(entry->historyItems().at(0)->attachments()->value("myattach.txt"), QByteArray("0123456789"));
    QCOMPARE(entry->historyItems().at(1)->attachments()->keys().size(), 1);
    QCOMPARE(entry->historyItems().at(1)->attachments()->value("myattach.txt"), QByteArray("abcdefghijk"));

    QCOMPARE(entry->autoTypeEnabled(), false);
    QCOMPARE(entry->autoTypeObfuscation(), 0);
    QCOMPARE(entry->defaultAutoTypeSequence(), QString(""));
    QCOMPARE(entry->autoTypeAssociations()->size(), 1);
    const AutoTypeAssociations::Association assoc1 = entry->autoTypeAssociations()->get(0);
    QCOMPARE(assoc1.window, QString("Target Window"));
    QCOMPARE(assoc1.sequence, QString(""));
}

void TestKeePass2Format::testXmlEntry2()
{
    const Entry* entry = m_xmlDb->rootGroup()->entries().at(1);

    QCOMPARE(entry->uuid(), QUuid::fromRfc4122(QByteArray::fromBase64("4jbADG37hkiLh2O0qUdaOQ==")));
    QCOMPARE(entry->iconNumber(), 0);
    QCOMPARE(entry->iconUuid(), QUuid::fromRfc4122(QByteArray::fromBase64("++vyI+daLk6omox4a6kQGA==")));
    // TODO: test entry->icon()
    QCOMPARE(entry->foregroundColor(), QString("#FF0000"));
    QCOMPARE(entry->backgroundColor(), QString("#FFFF00"));
    QCOMPARE(entry->overrideUrl(), QString("http://override.net/"));
    QCOMPARE(entry->tags(), QString(""));

    const TimeInfo ti = entry->timeInfo();
    QCOMPARE(ti.usageCount(), 7);

    QList<QString> attrs = entry->attributes()->keys();
    QCOMPARE(entry->attributes()->value("CustomString"), QString("isavalue"));
    QVERIFY(attrs.removeOne("CustomString"));
    QCOMPARE(entry->attributes()->value("Notes"), QString(""));
    QVERIFY(attrs.removeOne("Notes"));
    QCOMPARE(entry->attributes()->value("Password"), QString("Jer60Hz8o9XHvxBGcRqT"));
    QVERIFY(attrs.removeOne("Password"));
    QCOMPARE(entry->attributes()->value("Protected String"), QString("y")); // TODO: should have a protection attribute
    QVERIFY(attrs.removeOne("Protected String"));
    QCOMPARE(entry->attributes()->value("Title"), QString("Sample Entry 2"));
    QVERIFY(attrs.removeOne("Title"));
    QCOMPARE(entry->attributes()->value("URL"), QString("http://www.keepassx.org/"));
    QVERIFY(attrs.removeOne("URL"));
    QCOMPARE(entry->attributes()->value("UserName"), QString("notDEFUSERNAME"));
    QVERIFY(attrs.removeOne("UserName"));
    QVERIFY(attrs.isEmpty());

    QCOMPARE(entry->attachments()->keys().size(), 1);
    QCOMPARE(QString::fromLatin1(entry->attachments()->value("myattach.txt")), QString("abcdefghijk"));

    QCOMPARE(entry->autoTypeEnabled(), true);
    QCOMPARE(entry->autoTypeObfuscation(), 1);
    QCOMPARE(entry->defaultAutoTypeSequence(), QString("{USERNAME}{TAB}{PASSWORD}{ENTER}"));
    QCOMPARE(entry->autoTypeAssociations()->size(), 2);
    const AutoTypeAssociations::Association assoc1 = entry->autoTypeAssociations()->get(0);
    QCOMPARE(assoc1.window, QString("Target Window"));
    QCOMPARE(assoc1.sequence, QString("{Title}{UserName}"));
    const AutoTypeAssociations::Association assoc2 = entry->autoTypeAssociations()->get(1);
    QCOMPARE(assoc2.window, QString("Target Window 2"));
    QCOMPARE(assoc2.sequence, QString("{Title}{UserName} test"));
}

void TestKeePass2Format::testXmlEntryHistory()
{
    const Entry* entryMain = m_xmlDb->rootGroup()->entries().at(0);
    QCOMPARE(entryMain->historyItems().size(), 2);

    {
        const Entry* entry = entryMain->historyItems().at(0);
        QCOMPARE(entry->uuid(), entryMain->uuid());
        QVERIFY(!entry->parent());
        QCOMPARE(entry->timeInfo().lastModificationTime(), MockClock::datetimeUtc(2010, 8, 25, 16, 13, 54));
        QCOMPARE(entry->timeInfo().usageCount(), 3);
        QCOMPARE(entry->title(), QString("Sample Entry"));
        QCOMPARE(entry->url(), QString("http://www.somesite.com/"));
    }

    {
        const Entry* entry = entryMain->historyItems().at(1);
        QCOMPARE(entry->uuid(), entryMain->uuid());
        QVERIFY(!entry->parent());
        QCOMPARE(entry->timeInfo().lastModificationTime(), MockClock::datetimeUtc(2010, 8, 25, 16, 15, 43));
        QCOMPARE(entry->timeInfo().usageCount(), 7);
        QCOMPARE(entry->title(), QString("Sample Entry 1"));
        QCOMPARE(entry->url(), QString("http://www.somesite.com/"));
    }
}

void TestKeePass2Format::testXmlDeletedObjects()
{
    QList<DeletedObject> objList = m_xmlDb->deletedObjects();
    DeletedObject delObj;

    delObj = objList.takeFirst();
    QCOMPARE(delObj.uuid, QUuid::fromRfc4122(QByteArray::fromBase64("5K/bzWCSmkCv5OZxYl4N/w==")));
    QCOMPARE(delObj.deletionTime, MockClock::datetimeUtc(2010, 8, 25, 16, 14, 12));

    delObj = objList.takeFirst();
    QCOMPARE(delObj.uuid, QUuid::fromRfc4122(QByteArray::fromBase64("80h8uSNWgkKhKCp1TgXF7g==")));
    QCOMPARE(delObj.deletionTime, MockClock::datetimeUtc(2010, 8, 25, 16, 14, 14));

    QVERIFY(objList.isEmpty());
}

void TestKeePass2Format::testXmlBroken()
{
    QFETCH(QString, baseName);
    QFETCH(bool, strictMode);
    QFETCH(bool, expectError);

    QString xmlFile = QString("%1/%2.xml").arg(KEEPASSX_TEST_DATA_DIR, baseName);
    QVERIFY(QFile::exists(xmlFile));
    bool hasError;
    QString errorString;
    auto db = readXml(xmlFile, strictMode, hasError, errorString);
    if (hasError) {
        qWarning("Reader error: %s", qPrintable(errorString));
    }
    QCOMPARE(hasError, expectError);
}

// clang-format off
void TestKeePass2Format::testXmlBroken_data()
{
    QTest::addColumn<QString>("baseName");
    QTest::addColumn<bool>("strictMode");
    QTest::addColumn<bool>("expectError");

    //                                                                testfile                            strict?  error?
    QTest::newRow("BrokenNoGroupUuid                   (strict)") << "BrokenNoGroupUuid"               << true  << true;
    QTest::newRow("BrokenNoGroupUuid               (not strict)") << "BrokenNoGroupUuid"               << false << false;
    QTest::newRow("BrokenNoEntryUuid                   (strict)") << "BrokenNoEntryUuid"               << true  << true;
    QTest::newRow("BrokenNoEntryUuid               (not strict)") << "BrokenNoEntryUuid"               << false << false;
    QTest::newRow("BrokenNoRootGroup                   (strict)") << "BrokenNoRootGroup"               << true  << true;
    QTest::newRow("BrokenNoRootGroup               (not strict)") << "BrokenNoRootGroup"               << false << true;
    QTest::newRow("BrokenTwoRoots                      (strict)") << "BrokenTwoRoots"                  << true  << true;
    QTest::newRow("BrokenTwoRoots                  (not strict)") << "BrokenTwoRoots"                  << false << true;
    QTest::newRow("BrokenTwoRootGroups                 (strict)") << "BrokenTwoRootGroups"             << true  << true;
    QTest::newRow("BrokenTwoRootGroups             (not strict)") << "BrokenTwoRootGroups"             << false << true;
    QTest::newRow("BrokenGroupReference                (strict)") << "BrokenGroupReference"            << true  << false;
    QTest::newRow("BrokenGroupReference            (not strict)") << "BrokenGroupReference"            << false << false;
    QTest::newRow("BrokenDeletedObjects                (strict)") << "BrokenDeletedObjects"            << true  << true;
    QTest::newRow("BrokenDeletedObjects            (not strict)") << "BrokenDeletedObjects"            << false << false;
    QTest::newRow("BrokenDifferentEntryHistoryUuid     (strict)") << "BrokenDifferentEntryHistoryUuid" << true  << true;
    QTest::newRow("BrokenDifferentEntryHistoryUuid (not strict)") << "BrokenDifferentEntryHistoryUuid" << false << false;
}
// clang-format on

void TestKeePass2Format::testXmlEmptyUuids()
{

    QString xmlFile = QString("%1/%2.xml").arg(KEEPASSX_TEST_DATA_DIR, "EmptyUuids");
    QVERIFY(QFile::exists(xmlFile));
    bool hasError;
    QString errorString;
    auto db = readXml(xmlFile, true, hasError, errorString);
    if (hasError) {
        qWarning("Reader error: %s", qPrintable(errorString));
    }
    QVERIFY(!hasError);
}

void TestKeePass2Format::testXmlInvalidXmlChars()
{
    QScopedPointer<Database> dbWrite(new Database());

    QString strPlainInvalid =
        QString().append(QChar(0x02)).append(QChar(0x19)).append(QChar(0xFFFE)).append(QChar(0xFFFF));
    QString strPlainValid = QString()
                                .append(QChar(0x09))
                                .append(QChar(0x0A))
                                .append(QChar(0x20))
                                .append(QChar(0xD7FF))
                                .append(QChar(0xE000))
                                .append(QChar(0xFFFD));
    // U+10437 in UTF-16: D801 DC37
    //                    high low  surrogate
    QString strSingleHighSurrogate1 = QString().append(QChar(0xD801));
    QString strSingleHighSurrogate2 = QString().append(QChar(0x31)).append(QChar(0xD801)).append(QChar(0x32));
    QString strHighHighSurrogate = QString().append(QChar(0xD801)).append(QChar(0xD801));
    QString strSingleLowSurrogate1 = QString().append(QChar(0xDC37));
    QString strSingleLowSurrogate2 = QString().append(QChar((0x31))).append(QChar(0xDC37)).append(QChar(0x32));
    QString strLowLowSurrogate = QString().append(QChar(0xDC37)).append(QChar(0xDC37));
    QString strSurrogateValid1 = QString().append(QChar(0xD801)).append(QChar(0xDC37));
    QString strSurrogateValid2 =
        QString().append(QChar(0x31)).append(QChar(0xD801)).append(QChar(0xDC37)).append(QChar(0x32));

    auto entry = new Entry();
    entry->setUuid(QUuid::createUuid());
    entry->setGroup(dbWrite->rootGroup());
    entry->attributes()->set("PlainInvalid", strPlainInvalid);
    entry->attributes()->set("PlainValid", strPlainValid);
    entry->attributes()->set("SingleHighSurrogate1", strSingleHighSurrogate1);
    entry->attributes()->set("SingleHighSurrogate2", strSingleHighSurrogate2);
    entry->attributes()->set("HighHighSurrogate", strHighHighSurrogate);
    entry->attributes()->set("SingleLowSurrogate1", strSingleLowSurrogate1);
    entry->attributes()->set("SingleLowSurrogate2", strSingleLowSurrogate2);
    entry->attributes()->set("LowLowSurrogate", strLowLowSurrogate);
    entry->attributes()->set("SurrogateValid1", strSurrogateValid1);
    entry->attributes()->set("SurrogateValid2", strSurrogateValid2);

    QBuffer buffer;
    buffer.open(QIODevice::ReadWrite);
    bool hasError;
    QString errorString;
    writeXml(&buffer, dbWrite.data(), hasError, errorString);
    QVERIFY(!hasError);
    buffer.seek(0);

    auto dbRead = readXml(&buffer, true, hasError, errorString);
    if (hasError) {
        qWarning("Database read error: %s", qPrintable(errorString));
    }
    QVERIFY(!hasError);
    QVERIFY(dbRead.data());
    QCOMPARE(dbRead->rootGroup()->entries().size(), 1);
    Entry* entryRead = dbRead->rootGroup()->entries().at(0);
    EntryAttributes* attrRead = entryRead->attributes();

    QCOMPARE(attrRead->value("PlainInvalid"), QString());
    QCOMPARE(attrRead->value("PlainValid"), strPlainValid);
    QCOMPARE(attrRead->value("SingleHighSurrogate1"), QString());
    QCOMPARE(attrRead->value("SingleHighSurrogate2"), QString("12"));
    QCOMPARE(attrRead->value("HighHighSurrogate"), QString());
    QCOMPARE(attrRead->value("SingleLowSurrogate1"), QString());
    QCOMPARE(attrRead->value("SingleLowSurrogate2"), QString("12"));
    QCOMPARE(attrRead->value("LowLowSurrogate"), QString());
    QCOMPARE(attrRead->value("SurrogateValid1"), strSurrogateValid1);
    QCOMPARE(attrRead->value("SurrogateValid2"), strSurrogateValid2);
}

void TestKeePass2Format::testXmlRepairUuidHistoryItem()
{
    QString xmlFile = QString("%1/%2.xml").arg(KEEPASSX_TEST_DATA_DIR, "BrokenDifferentEntryHistoryUuid");
    QVERIFY(QFile::exists(xmlFile));
    bool hasError;
    QString errorString;
    auto db = readXml(xmlFile, false, hasError, errorString);
    if (hasError) {
        qWarning("Database read error: %s", qPrintable(errorString));
    }
    QVERIFY(!hasError);

    QList<Entry*> entries = db->rootGroup()->entries();
    QCOMPARE(entries.size(), 1);
    Entry* entry = entries.at(0);

    QList<Entry*> historyItems = entry->historyItems();
    QCOMPARE(historyItems.size(), 1);
    Entry* historyItem = historyItems.at(0);

    QVERIFY(!entry->uuid().isNull());
    QVERIFY(!historyItem->uuid().isNull());
    QCOMPARE(historyItem->uuid(), entry->uuid());
}

void TestKeePass2Format::testReadBackTargetDb()
{
    // read back previously constructed KDBX
    auto key = QSharedPointer<CompositeKey>::create();
    key->addKey(QSharedPointer<PasswordKey>::create("test"));

    bool hasError;
    QString errorString;

    m_kdbxTargetBuffer.seek(0);
    m_kdbxTargetDb = QSharedPointer<Database>::create();
    readKdbx(&m_kdbxTargetBuffer, key, m_kdbxTargetDb, hasError, errorString);
    if (hasError) {
        QFAIL(qPrintable(QString("Error while reading database: ").append(errorString)));
    }
    QVERIFY(m_kdbxTargetDb.data());
}

void TestKeePass2Format::testKdbxBasic()
{
    QCOMPARE(m_kdbxTargetDb->metadata()->name(), m_kdbxSourceDb->metadata()->name());
    QVERIFY(m_kdbxTargetDb->rootGroup());
    QCOMPARE(m_kdbxTargetDb->rootGroup()->children()[0]->name(), m_kdbxSourceDb->rootGroup()->children()[0]->name());
    QCOMPARE(m_kdbxTargetDb->rootGroup()->notes(), m_kdbxSourceDb->rootGroup()->notes());
    QCOMPARE(m_kdbxTargetDb->rootGroup()->children()[0]->notes(), m_kdbxSourceDb->rootGroup()->children()[0]->notes());
}

void TestKeePass2Format::testKdbxProtectedAttributes()
{
    QCOMPARE(m_kdbxTargetDb->rootGroup()->entries().size(), 1);
    Entry* entry = m_kdbxTargetDb->rootGroup()->entries().at(0);
    QCOMPARE(entry->attributes()->value("test"), QString("protectedTest"));
    QCOMPARE(entry->attributes()->isProtected("test"), true);
}

void TestKeePass2Format::testKdbxAttachments()
{
    Entry* entry = m_kdbxTargetDb->rootGroup()->entries().at(0);
    QCOMPARE(entry->attachments()->keys().size(), 2);
    QCOMPARE(entry->attachments()->value("myattach.txt"), QByteArray("this is an attachment"));
    QCOMPARE(entry->attachments()->value("aaa.txt"), QByteArray("also an attachment"));
}

void TestKeePass2Format::testKdbxNonAsciiPasswords()
{
    QCOMPARE(m_kdbxTargetDb->rootGroup()->entries()[0]->password(),
             m_kdbxSourceDb->rootGroup()->entries()[0]->password());
}

void TestKeePass2Format::testKdbxDeviceFailure()
{
    auto key = QSharedPointer<CompositeKey>::create();
    key->addKey(QSharedPointer<PasswordKey>::create("test"));
    QScopedPointer<Database> db(new Database());
    db->setKey(key);
    // Disable compression so we write a predictable number of bytes.
    db->setCompressionAlgorithm(Database::CompressionNone);

    auto entry = new Entry();
    entry->setParent(db->rootGroup());
    QByteArray attachment(4096, 'Z');
    entry->attachments()->set("test", attachment);

    FailDevice failDevice(512);
    QVERIFY(failDevice.open(QIODevice::WriteOnly));
    bool hasError;
    QString errorString;
    writeKdbx(&failDevice, db.data(), hasError, errorString);
    QVERIFY(hasError);
    QCOMPARE(errorString, QString("FAILDEVICE"));
}

Q_DECLARE_METATYPE(QSharedPointer<CompositeKey>)

void TestKeePass2Format::testKdbxKeyChange()
{
    QFETCH(QSharedPointer<CompositeKey>, key1);
    QFETCH(QSharedPointer<CompositeKey>, key2);

    bool hasError;
    QString errorString;

    // write new database
    QBuffer buffer;
    buffer.open(QBuffer::ReadWrite);
    buffer.seek(0);
    QSharedPointer<Database> db(new Database());
    db->changeKdf(fastKdf(KeePass2::uuidToKdf(m_kdbxSourceDb->kdf()->uuid())));
    db->setRootGroup(m_kdbxSourceDb->rootGroup()->clone(Entry::CloneNoFlags, Group::CloneIncludeEntries));

    db->setKey(key1);
    writeKdbx(&buffer, db.data(), hasError, errorString);
    QVERIFY(!hasError);

    // read database
    db = QSharedPointer<Database>::create();
    buffer.seek(0);
    readKdbx(&buffer, key1, db, hasError, errorString);
    if (hasError) {
        QFAIL(qPrintable(QStringLiteral("Error while reading database: ").append(errorString)));
    }
    QVERIFY(db.data());

    // change key
    db->setKey(key2);

    // write database
    buffer.seek(0);
    writeKdbx(&buffer, db.data(), hasError, errorString);
    QVERIFY(!hasError);

    // read database
    db = QSharedPointer<Database>::create();
    buffer.seek(0);
    readKdbx(&buffer, key2, db, hasError, errorString);
    if (hasError) {
        QFAIL(qPrintable(QStringLiteral("Error while reading database: ").append(errorString)));
    }
    QVERIFY(db.data());
    QVERIFY(db->rootGroup() != m_kdbxSourceDb->rootGroup());
    QVERIFY(db->rootGroup()->uuid() == m_kdbxSourceDb->rootGroup()->uuid());
}

void TestKeePass2Format::testKdbxKeyChange_data()
{
    QTest::addColumn<QSharedPointer<CompositeKey>>("key1");
    QTest::addColumn<QSharedPointer<CompositeKey>>("key2");

    auto passwordKey1 = QSharedPointer<PasswordKey>::create("abc");
    auto passwordKey2 = QSharedPointer<PasswordKey>::create("def");

    QByteArray fileKeyBytes1("uvw");
    QBuffer fileKeyBuffer1(&fileKeyBytes1);
    fileKeyBuffer1.open(QBuffer::ReadOnly);
    auto fileKey1 = QSharedPointer<FileKey>::create();
    fileKey1->load(&fileKeyBuffer1);

    QByteArray fileKeyBytes2("xzy");
    QBuffer fileKeyBuffer2(&fileKeyBytes1);
    fileKeyBuffer2.open(QBuffer::ReadOnly);
    auto fileKey2 = QSharedPointer<FileKey>::create();
    fileKey2->load(&fileKeyBuffer2);

    auto crKey1 = QSharedPointer<MockChallengeResponseKey>::create(QByteArray("123"));
    auto crKey2 = QSharedPointer<MockChallengeResponseKey>::create(QByteArray("456"));

    // empty key
    auto compositeKey0 = QSharedPointer<CompositeKey>::create();

    // all in
    auto compositeKey1_1 = QSharedPointer<CompositeKey>::create();
    compositeKey1_1->addKey(passwordKey1);
    compositeKey1_1->addKey(fileKey1);
    compositeKey1_1->addChallengeResponseKey(crKey1);
    auto compositeKey1_2 = QSharedPointer<CompositeKey>::create();
    compositeKey1_2->addKey(passwordKey2);
    compositeKey1_2->addKey(fileKey2);
    compositeKey1_2->addChallengeResponseKey(crKey2);

    QTest::newRow("Change:  Empty Key -> Full Key") << compositeKey0 << compositeKey1_1;
    QTest::newRow("Change:   Full Key -> Empty Key") << compositeKey1_1 << compositeKey0;
    QTest::newRow("Change: Full Key 1 -> Full Key 2") << compositeKey1_1 << compositeKey1_2;

    // only password
    auto compositeKey2_1 = QSharedPointer<CompositeKey>::create();
    compositeKey2_1->addKey(passwordKey1);
    auto compositeKey2_2 = QSharedPointer<CompositeKey>::create();
    compositeKey2_2->addKey(passwordKey2);

    QTest::newRow("Change:   Password -> Empty Key") << compositeKey2_1 << compositeKey0;
    QTest::newRow("Change:  Empty Key -> Password") << compositeKey0 << compositeKey2_1;
    QTest::newRow("Change:   Full Key -> Password 1") << compositeKey1_1 << compositeKey2_1;
    QTest::newRow("Change:   Full Key -> Password 2") << compositeKey1_1 << compositeKey2_2;
    QTest::newRow("Change: Password 1 -> Full Key") << compositeKey2_1 << compositeKey1_1;
    QTest::newRow("Change: Password 2 -> Full Key") << compositeKey2_2 << compositeKey1_1;
    QTest::newRow("Change: Password 1 -> Password 2") << compositeKey2_1 << compositeKey2_2;

    // only key file
    auto compositeKey3_1 = QSharedPointer<CompositeKey>::create();
    compositeKey3_1->addKey(fileKey1);
    auto compositeKey3_2 = QSharedPointer<CompositeKey>::create();
    compositeKey3_2->addKey(fileKey2);

    QTest::newRow("Change:   Key File -> Empty Key") << compositeKey3_1 << compositeKey0;
    QTest::newRow("Change:  Empty Key -> Key File") << compositeKey0 << compositeKey3_1;
    QTest::newRow("Change:   Full Key -> Key File 1") << compositeKey1_1 << compositeKey3_1;
    QTest::newRow("Change:   Full Key -> Key File 2") << compositeKey1_1 << compositeKey3_2;
    QTest::newRow("Change: Key File 1 -> Full Key") << compositeKey3_1 << compositeKey1_1;
    QTest::newRow("Change: Key File 2 -> Full Key") << compositeKey3_2 << compositeKey1_1;
    QTest::newRow("Change: Key File 1 -> Key File 2") << compositeKey3_1 << compositeKey3_2;

    // only cr key
    auto compositeKey4_1 = QSharedPointer<CompositeKey>::create();
    compositeKey4_1->addChallengeResponseKey(crKey1);
    auto compositeKey4_2 = QSharedPointer<CompositeKey>::create();
    compositeKey4_2->addChallengeResponseKey(crKey2);

    QTest::newRow("Change:    CR Key -> Empty Key") << compositeKey4_1 << compositeKey0;
    QTest::newRow("Change: Empty Key -> CR Key") << compositeKey0 << compositeKey4_1;
    QTest::newRow("Change:  Full Key -> CR Key 1") << compositeKey1_1 << compositeKey4_1;
    QTest::newRow("Change:  Full Key -> CR Key 2") << compositeKey1_1 << compositeKey4_2;
    QTest::newRow("Change:  CR Key 1 -> Full Key") << compositeKey4_1 << compositeKey1_1;
    QTest::newRow("Change:  CR Key 2 -> Full Key") << compositeKey4_2 << compositeKey1_1;
    QTest::newRow("Change:  CR Key 1 -> CR Key 2") << compositeKey4_1 << compositeKey4_2;

    // rotate
    QTest::newRow("Change: Password -> Key File") << compositeKey2_1 << compositeKey3_1;
    QTest::newRow("Change: Key File -> Password") << compositeKey3_1 << compositeKey2_1;
    QTest::newRow("Change: Password -> Key File") << compositeKey2_1 << compositeKey3_1;
    QTest::newRow("Change: Key File -> Password") << compositeKey3_1 << compositeKey2_1;
    QTest::newRow("Change: Password -> CR Key") << compositeKey2_1 << compositeKey4_1;
    QTest::newRow("Change:   CR Key -> Password") << compositeKey4_1 << compositeKey2_1;
    QTest::newRow("Change: Key File -> CR Key") << compositeKey3_1 << compositeKey4_1;
    QTest::newRow("Change:   CR Key -> Key File") << compositeKey4_1 << compositeKey3_1;

    // leave one out
    auto compositeKey5_1 = QSharedPointer<CompositeKey>::create();
    compositeKey5_1->addKey(fileKey1);
    compositeKey5_1->addChallengeResponseKey(crKey1);
    auto compositeKey5_2 = QSharedPointer<CompositeKey>::create();
    compositeKey5_2->addKey(passwordKey1);
    compositeKey5_2->addChallengeResponseKey(crKey1);
    auto compositeKey5_3 = QSharedPointer<CompositeKey>::create();
    compositeKey5_3->addKey(passwordKey1);
    compositeKey5_3->addKey(fileKey1);

    QTest::newRow("Change:    Full Key -> No Password") << compositeKey1_1 << compositeKey5_1;
    QTest::newRow("Change: No Password -> Full Key") << compositeKey5_1 << compositeKey1_1;
    QTest::newRow("Change:    Full Key -> No Key File") << compositeKey1_1 << compositeKey5_2;
    QTest::newRow("Change: No Key File -> Full Key") << compositeKey5_2 << compositeKey1_1;
    QTest::newRow("Change:    Full Key -> No CR Key") << compositeKey1_1 << compositeKey5_3;
    QTest::newRow("Change:   No CR Key -> Full Key") << compositeKey5_3 << compositeKey1_1;
}

/**
 * Test for catching mapping errors with duplicate attachments.
 */
void TestKeePass2Format::testDuplicateAttachments()
{
    auto db = QSharedPointer<Database>::create();
    db->setKey(QSharedPointer<CompositeKey>::create());

    const QByteArray attachment1("abc");
    const QByteArray attachment2("def");
    const QByteArray attachment3("ghi");

    auto entry1 = new Entry();
    entry1->setGroup(db->rootGroup());
    entry1->setUuid(QUuid::fromRfc4122("aaaaaaaaaaaaaaaa"));
    entry1->attachments()->set("a", attachment1);

    auto entry2 = new Entry();
    entry2->setGroup(db->rootGroup());
    entry2->setUuid(QUuid::fromRfc4122("bbbbbbbbbbbbbbbb"));
    entry2->attachments()->set("b1", attachment1);
    entry2->beginUpdate();
    entry2->attachments()->set("b2", attachment1);
    entry2->endUpdate();
    entry2->beginUpdate();
    entry2->attachments()->set("b3", attachment2);
    entry2->endUpdate();
    entry2->beginUpdate();
    entry2->attachments()->set("b4", attachment2);
    entry2->endUpdate();

    auto entry3 = new Entry();
    entry3->setGroup(db->rootGroup());
    entry3->setUuid(QUuid::fromRfc4122("cccccccccccccccc"));
    entry3->attachments()->set("c1", attachment2);
    entry3->attachments()->set("c2", attachment2);
    entry3->attachments()->set("c3", attachment3);

    QBuffer buffer;
    buffer.open(QBuffer::ReadWrite);

    bool hasError = false;
    QString errorString;
    writeKdbx(&buffer, db.data(), hasError, errorString);
    if (hasError) {
        QFAIL(qPrintable(QString("Error while writing database: %1").arg(errorString)));
    }

    buffer.seek(0);
    readKdbx(&buffer, QSharedPointer<CompositeKey>::create(), db, hasError, errorString);
    if (hasError) {
        QFAIL(qPrintable(QString("Error while reading database: %1").arg(errorString)));
    }

    QCOMPARE(db->rootGroup()->entries()[0]->attachments()->value("a"), attachment1);

    QCOMPARE(db->rootGroup()->entries()[1]->attachments()->value("b1"), attachment1);
    QCOMPARE(db->rootGroup()->entries()[1]->attachments()->value("b2"), attachment1);
    QCOMPARE(db->rootGroup()->entries()[1]->attachments()->value("b3"), attachment2);
    QCOMPARE(db->rootGroup()->entries()[1]->attachments()->value("b4"), attachment2);
    QCOMPARE(db->rootGroup()->entries()[1]->historyItems()[0]->attachments()->value("b1"), attachment1);
    QCOMPARE(db->rootGroup()->entries()[1]->historyItems()[1]->attachments()->value("b1"), attachment1);
    QCOMPARE(db->rootGroup()->entries()[1]->historyItems()[1]->attachments()->value("b2"), attachment1);
    QCOMPARE(db->rootGroup()->entries()[1]->historyItems()[2]->attachments()->value("b1"), attachment1);
    QCOMPARE(db->rootGroup()->entries()[1]->historyItems()[2]->attachments()->value("b2"), attachment1);
    QCOMPARE(db->rootGroup()->entries()[1]->historyItems()[2]->attachments()->value("b3"), attachment2);

    QCOMPARE(db->rootGroup()->entries()[2]->attachments()->value("c1"), attachment2);
    QCOMPARE(db->rootGroup()->entries()[2]->attachments()->value("c2"), attachment2);
    QCOMPARE(db->rootGroup()->entries()[2]->attachments()->value("c3"), attachment3);
}

/**
 * @return fast "dummy" KDF
 */
QSharedPointer<Kdf> TestKeePass2Format::fastKdf(QSharedPointer<Kdf> kdf) const
{
    kdf->setRounds(1);

    if (kdf->uuid() == KeePass2::KDF_ARGON2D) {
        kdf->processParameters({{KeePass2::KDFPARAM_ARGON2_MEMORY, 1024}, {KeePass2::KDFPARAM_ARGON2_PARALLELISM, 1}});
    }

    return kdf;
}
