/*
 *  Copyright (C) 2010 Felix Geyer <debfx@fobos.de>
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

#include "TestGroup.h"
#include "TestGlobal.h"
#include "mock/MockClock.h"

#include <QScopedPointer>
#include <QSignalSpy>

#include "core/Metadata.h"
#include "crypto/Crypto.h"

QTEST_GUILESS_MAIN(TestGroup)

namespace
{
    MockClock* m_clock = nullptr;
}

void TestGroup::initTestCase()
{
    qRegisterMetaType<Entry*>("Entry*");
    qRegisterMetaType<Group*>("Group*");
    QVERIFY(Crypto::init());
}

void TestGroup::init()
{
    Q_ASSERT(m_clock == nullptr);
    m_clock = new MockClock(2010, 5, 5, 10, 30, 10);
    MockClock::setup(m_clock);
}

void TestGroup::cleanup()
{
    MockClock::teardown();
    m_clock = nullptr;
}

void TestGroup::testParenting()
{
    Database* db = new Database();
    QPointer<Group> rootGroup = db->rootGroup();
    Group* tmpRoot = new Group();

    QPointer<Group> g1 = new Group();
    QPointer<Group> g2 = new Group();
    QPointer<Group> g3 = new Group();
    QPointer<Group> g4 = new Group();

    g1->setParent(tmpRoot);
    g2->setParent(tmpRoot);
    g3->setParent(tmpRoot);
    g4->setParent(tmpRoot);

    g2->setParent(g1);
    g4->setParent(g3);
    g3->setParent(g1);
    g1->setParent(db->rootGroup());

    QVERIFY(g1->parent() == rootGroup);
    QVERIFY(g2->parent() == g1);
    QVERIFY(g3->parent() == g1);
    QVERIFY(g4->parent() == g3);

    QVERIFY(g1->database() == db);
    QVERIFY(g2->database() == db);
    QVERIFY(g3->database() == db);
    QVERIFY(g4->database() == db);

    QCOMPARE(tmpRoot->children().size(), 0);
    QCOMPARE(rootGroup->children().size(), 1);
    QCOMPARE(g1->children().size(), 2);
    QCOMPARE(g2->children().size(), 0);
    QCOMPARE(g3->children().size(), 1);
    QCOMPARE(g4->children().size(), 0);

    QVERIFY(rootGroup->children().at(0) == g1);
    QVERIFY(rootGroup->children().at(0) == g1);
    QVERIFY(g1->children().at(0) == g2);
    QVERIFY(g1->children().at(1) == g3);
    QVERIFY(g3->children().contains(g4));

    Group* g5 = new Group();
    Group* g6 = new Group();
    g5->setParent(db->rootGroup());
    g6->setParent(db->rootGroup());
    QVERIFY(db->rootGroup()->children().at(1) == g5);
    QVERIFY(db->rootGroup()->children().at(2) == g6);

    g5->setParent(db->rootGroup());
    QVERIFY(db->rootGroup()->children().at(1) == g6);
    QVERIFY(db->rootGroup()->children().at(2) == g5);

    QSignalSpy spy(db, SIGNAL(groupDataChanged(Group*)));
    g2->setName("test");
    g4->setName("test");
    g3->setName("test");
    g1->setName("test");
    g3->setIcon(QUuid::createUuid());
    g1->setIcon(2);
    QCOMPARE(spy.count(), 6);
    delete db;

    QVERIFY(rootGroup.isNull());
    QVERIFY(g1.isNull());
    QVERIFY(g2.isNull());
    QVERIFY(g3.isNull());
    QVERIFY(g4.isNull());
    delete tmpRoot;
}

void TestGroup::testSignals()
{
    Database* db = new Database();
    Database* db2 = new Database();
    QPointer<Group> root = db->rootGroup();

    QSignalSpy spyAboutToAdd(db, SIGNAL(groupAboutToAdd(Group*, int)));
    QSignalSpy spyAdded(db, SIGNAL(groupAdded()));
    QSignalSpy spyAboutToRemove(db, SIGNAL(groupAboutToRemove(Group*)));
    QSignalSpy spyRemoved(db, SIGNAL(groupRemoved()));
    QSignalSpy spyAboutToMove(db, SIGNAL(groupAboutToMove(Group*, Group*, int)));
    QSignalSpy spyMoved(db, SIGNAL(groupMoved()));

    QSignalSpy spyAboutToAdd2(db2, SIGNAL(groupAboutToAdd(Group*, int)));
    QSignalSpy spyAdded2(db2, SIGNAL(groupAdded()));
    QSignalSpy spyAboutToRemove2(db2, SIGNAL(groupAboutToRemove(Group*)));
    QSignalSpy spyRemoved2(db2, SIGNAL(groupRemoved()));
    QSignalSpy spyAboutToMove2(db2, SIGNAL(groupAboutToMove(Group*, Group*, int)));
    QSignalSpy spyMoved2(db2, SIGNAL(groupMoved()));

    Group* g1 = new Group();
    Group* g2 = new Group();

    g1->setParent(root);
    QCOMPARE(spyAboutToAdd.count(), 1);
    QCOMPARE(spyAdded.count(), 1);
    QCOMPARE(spyAboutToRemove.count(), 0);
    QCOMPARE(spyRemoved.count(), 0);
    QCOMPARE(spyAboutToMove.count(), 0);
    QCOMPARE(spyMoved.count(), 0);

    g2->setParent(root);
    QCOMPARE(spyAboutToAdd.count(), 2);
    QCOMPARE(spyAdded.count(), 2);
    QCOMPARE(spyAboutToRemove.count(), 0);
    QCOMPARE(spyRemoved.count(), 0);
    QCOMPARE(spyAboutToMove.count(), 0);
    QCOMPARE(spyMoved.count(), 0);

    g2->setParent(root);
    QCOMPARE(spyAboutToAdd.count(), 2);
    QCOMPARE(spyAdded.count(), 2);
    QCOMPARE(spyAboutToRemove.count(), 0);
    QCOMPARE(spyRemoved.count(), 0);
    QCOMPARE(spyAboutToMove.count(), 0);
    QCOMPARE(spyMoved.count(), 0);

    g2->setParent(root, 0);
    QCOMPARE(spyAboutToAdd.count(), 2);
    QCOMPARE(spyAdded.count(), 2);
    QCOMPARE(spyAboutToRemove.count(), 0);
    QCOMPARE(spyRemoved.count(), 0);
    QCOMPARE(spyAboutToMove.count(), 1);
    QCOMPARE(spyMoved.count(), 1);

    g1->setParent(g2);
    QCOMPARE(spyAboutToAdd.count(), 2);
    QCOMPARE(spyAdded.count(), 2);
    QCOMPARE(spyAboutToRemove.count(), 0);
    QCOMPARE(spyRemoved.count(), 0);
    QCOMPARE(spyAboutToMove.count(), 2);
    QCOMPARE(spyMoved.count(), 2);

    delete g1;
    QCOMPARE(spyAboutToAdd.count(), 2);
    QCOMPARE(spyAdded.count(), 2);
    QCOMPARE(spyAboutToRemove.count(), 1);
    QCOMPARE(spyRemoved.count(), 1);
    QCOMPARE(spyAboutToMove.count(), 2);
    QCOMPARE(spyMoved.count(), 2);

    g2->setParent(db2->rootGroup());
    QCOMPARE(spyAboutToAdd.count(), 2);
    QCOMPARE(spyAdded.count(), 2);
    QCOMPARE(spyAboutToRemove.count(), 2);
    QCOMPARE(spyRemoved.count(), 2);
    QCOMPARE(spyAboutToMove.count(), 2);
    QCOMPARE(spyMoved.count(), 2);
    QCOMPARE(spyAboutToAdd2.count(), 1);
    QCOMPARE(spyAdded2.count(), 1);
    QCOMPARE(spyAboutToRemove2.count(), 0);
    QCOMPARE(spyRemoved2.count(), 0);
    QCOMPARE(spyAboutToMove2.count(), 0);
    QCOMPARE(spyMoved2.count(), 0);

    Group* g3 = new Group();
    Group* g4 = new Group();

    g3->setParent(root);
    QCOMPARE(spyAboutToAdd.count(), 3);
    QCOMPARE(spyAdded.count(), 3);
    QCOMPARE(spyAboutToRemove.count(), 2);
    QCOMPARE(spyRemoved.count(), 2);
    QCOMPARE(spyAboutToMove.count(), 2);
    QCOMPARE(spyMoved.count(), 2);

    g4->setParent(root);
    QCOMPARE(spyAboutToAdd.count(), 4);
    QCOMPARE(spyAdded.count(), 4);
    QCOMPARE(spyAboutToRemove.count(), 2);
    QCOMPARE(spyRemoved.count(), 2);
    QCOMPARE(spyAboutToMove.count(), 2);
    QCOMPARE(spyMoved.count(), 2);

    g3->setParent(root);
    QCOMPARE(spyAboutToAdd.count(), 4);
    QCOMPARE(spyAdded.count(), 4);
    QCOMPARE(spyAboutToRemove.count(), 2);
    QCOMPARE(spyRemoved.count(), 2);
    QCOMPARE(spyAboutToMove.count(), 3);
    QCOMPARE(spyMoved.count(), 3);

    delete db;
    delete db2;

    QVERIFY(root.isNull());
}

void TestGroup::testEntries()
{
    Group* group = new Group();

    QPointer<Entry> entry1 = new Entry();
    entry1->setGroup(group);

    QPointer<Entry> entry2 = new Entry();
    entry2->setGroup(group);

    QCOMPARE(group->entries().size(), 2);
    QVERIFY(group->entries().at(0) == entry1);
    QVERIFY(group->entries().at(1) == entry2);

    delete group;

    QVERIFY(entry1.isNull());
    QVERIFY(entry2.isNull());
}

void TestGroup::testDeleteSignals()
{
    QScopedPointer<Database> db(new Database());
    Group* groupRoot = db->rootGroup();
    Group* groupChild = new Group();
    Group* groupChildChild = new Group();
    groupRoot->setObjectName("groupRoot");
    groupChild->setObjectName("groupChild");
    groupChildChild->setObjectName("groupChildChild");
    groupChild->setParent(groupRoot);
    groupChildChild->setParent(groupChild);
    QSignalSpy spyAboutToRemove(db.data(), SIGNAL(groupAboutToRemove(Group*)));
    QSignalSpy spyRemoved(db.data(), SIGNAL(groupRemoved()));

    delete groupChild;
    QVERIFY(groupRoot->children().isEmpty());
    QCOMPARE(spyAboutToRemove.count(), 2);
    QCOMPARE(spyRemoved.count(), 2);

    Group* group = new Group();
    Entry* entry = new Entry();
    entry->setGroup(group);
    QSignalSpy spyEntryAboutToRemove(group, SIGNAL(entryAboutToRemove(Entry*)));
    QSignalSpy spyEntryRemoved(group, SIGNAL(entryRemoved(Entry*)));

    delete entry;
    QVERIFY(group->entries().isEmpty());
    QCOMPARE(spyEntryAboutToRemove.count(), 1);
    QCOMPARE(spyEntryRemoved.count(), 1);
    delete group;

    QScopedPointer<Database> db2(new Database());
    Group* groupRoot2 = db2->rootGroup();
    Group* group2 = new Group();
    group2->setParent(groupRoot2);
    Entry* entry2 = new Entry();
    entry2->setGroup(group2);
    QSignalSpy spyEntryAboutToRemove2(group2, SIGNAL(entryAboutToRemove(Entry*)));
    QSignalSpy spyEntryRemoved2(group2, SIGNAL(entryRemoved(Entry*)));

    delete group2;
    QCOMPARE(spyEntryAboutToRemove2.count(), 1);
    QCOMPARE(spyEntryRemoved2.count(), 1);
}

void TestGroup::testCopyCustomIcon()
{
    QScopedPointer<Database> dbSource(new Database());

    QUuid groupIconUuid = QUuid::createUuid();
    QByteArray groupIcon("group icon");
    dbSource->metadata()->addCustomIcon(groupIconUuid, groupIcon);

    QUuid entryIconUuid = QUuid::createUuid();
    QByteArray entryIcon("entry icon");
    dbSource->metadata()->addCustomIcon(entryIconUuid, entryIcon);

    Group* group = new Group();
    group->setParent(dbSource->rootGroup());
    group->setIcon(groupIconUuid);
    QCOMPARE(group->database()->metadata()->customIcon(groupIconUuid), groupIcon);

    Entry* entry = new Entry();
    entry->setGroup(dbSource->rootGroup());
    entry->setIcon(entryIconUuid);
    QCOMPARE(entry->database()->metadata()->customIcon(entryIconUuid), entryIcon);

    QScopedPointer<Database> dbTarget(new Database());

    group->setParent(dbTarget->rootGroup());
    QVERIFY(dbTarget->metadata()->hasCustomIcon(groupIconUuid));
    QCOMPARE(dbTarget->metadata()->customIcon(groupIconUuid), groupIcon);
    QCOMPARE(group->database()->metadata()->customIcon(groupIconUuid), groupIcon);

    entry->setGroup(dbTarget->rootGroup());
    QVERIFY(dbTarget->metadata()->hasCustomIcon(entryIconUuid));
    QCOMPARE(dbTarget->metadata()->customIcon(entryIconUuid), entryIcon);
    QCOMPARE(entry->database()->metadata()->customIcon(entryIconUuid), entryIcon);
}

void TestGroup::testClone()
{
    QScopedPointer<Database> db(new Database());

    QScopedPointer<Group> originalGroup(new Group());
    originalGroup->setParent(db->rootGroup());
    originalGroup->setName("Group");
    originalGroup->setIcon(42);

    QScopedPointer<Entry> originalGroupEntry(new Entry());
    originalGroupEntry->setGroup(originalGroup.data());
    originalGroupEntry->setTitle("GroupEntryOld");
    originalGroupEntry->setIcon(43);
    originalGroupEntry->beginUpdate();
    originalGroupEntry->setTitle("GroupEntry");
    originalGroupEntry->endUpdate();

    QScopedPointer<Group> subGroup(new Group());
    subGroup->setParent(originalGroup.data());
    subGroup->setName("SubGroup");

    QScopedPointer<Entry> subGroupEntry(new Entry());
    subGroupEntry->setGroup(subGroup.data());
    subGroupEntry->setTitle("SubGroupEntry");

    QScopedPointer<Group> clonedGroup(originalGroup->clone());
    QVERIFY(!clonedGroup->parentGroup());
    QVERIFY(!clonedGroup->database());
    QVERIFY(clonedGroup->uuid() != originalGroup->uuid());
    QCOMPARE(clonedGroup->name(), QString("Group"));
    QCOMPARE(clonedGroup->iconNumber(), 42);
    QCOMPARE(clonedGroup->children().size(), 1);
    QCOMPARE(clonedGroup->entries().size(), 1);

    Entry* clonedGroupEntry = clonedGroup->entries().at(0);
    QVERIFY(clonedGroupEntry->uuid() != originalGroupEntry->uuid());
    QCOMPARE(clonedGroupEntry->title(), QString("GroupEntry"));
    QCOMPARE(clonedGroupEntry->iconNumber(), 43);
    QCOMPARE(clonedGroupEntry->historyItems().size(), 0);

    Group* clonedSubGroup = clonedGroup->children().at(0);
    QVERIFY(clonedSubGroup->uuid() != subGroup->uuid());
    QCOMPARE(clonedSubGroup->name(), QString("SubGroup"));
    QCOMPARE(clonedSubGroup->children().size(), 0);
    QCOMPARE(clonedSubGroup->entries().size(), 1);

    Entry* clonedSubGroupEntry = clonedSubGroup->entries().at(0);
    QVERIFY(clonedSubGroupEntry->uuid() != subGroupEntry->uuid());
    QCOMPARE(clonedSubGroupEntry->title(), QString("SubGroupEntry"));

    QScopedPointer<Group> clonedGroupKeepUuid(originalGroup->clone(Entry::CloneNoFlags));
    QCOMPARE(clonedGroupKeepUuid->entries().at(0)->uuid(), originalGroupEntry->uuid());
    QCOMPARE(clonedGroupKeepUuid->children().at(0)->entries().at(0)->uuid(), subGroupEntry->uuid());

    QScopedPointer<Group> clonedGroupNoFlags(originalGroup->clone(Entry::CloneNoFlags, Group::CloneNoFlags));
    QCOMPARE(clonedGroupNoFlags->entries().size(), 0);
    QVERIFY(clonedGroupNoFlags->uuid() == originalGroup->uuid());

    QScopedPointer<Group> clonedGroupNewUuid(originalGroup->clone(Entry::CloneNoFlags, Group::CloneNewUuid));
    QCOMPARE(clonedGroupNewUuid->entries().size(), 0);
    QVERIFY(clonedGroupNewUuid->uuid() != originalGroup->uuid());

    // Making sure the new modification date is not the same.
    m_clock->advanceSecond(1);

    QScopedPointer<Group> clonedGroupResetTimeInfo(
        originalGroup->clone(Entry::CloneNoFlags, Group::CloneNewUuid | Group::CloneResetTimeInfo));
    QCOMPARE(clonedGroupResetTimeInfo->entries().size(), 0);
    QVERIFY(clonedGroupResetTimeInfo->uuid() != originalGroup->uuid());
    QVERIFY(clonedGroupResetTimeInfo->timeInfo().lastModificationTime()
            != originalGroup->timeInfo().lastModificationTime());
}

void TestGroup::testCopyCustomIcons()
{
    QScopedPointer<Database> dbSource(new Database());
    QScopedPointer<Database> dbTarget(new Database());

    QByteArray iconImage1("icon 1");

    QByteArray iconImage2("icon 2");

    QScopedPointer<Group> group1(new Group());
    group1->setParent(dbSource->rootGroup());
    QUuid group1Icon = QUuid::createUuid();
    dbSource->metadata()->addCustomIcon(group1Icon, iconImage1);
    group1->setIcon(group1Icon);

    QScopedPointer<Group> group2(new Group());
    group2->setParent(group1.data());
    QUuid group2Icon = QUuid::createUuid();
    dbSource->metadata()->addCustomIcon(group2Icon, iconImage1);
    group2->setIcon(group2Icon);

    QScopedPointer<Entry> entry1(new Entry());
    entry1->setGroup(group2.data());
    QUuid entry1IconOld = QUuid::createUuid();
    dbSource->metadata()->addCustomIcon(entry1IconOld, iconImage1);
    entry1->setIcon(entry1IconOld);

    // add history item
    entry1->beginUpdate();
    QUuid entry1IconNew = QUuid::createUuid();
    dbSource->metadata()->addCustomIcon(entry1IconNew, iconImage1);
    entry1->setIcon(entry1IconNew);
    entry1->endUpdate();

    // test that we don't overwrite icons
    dbTarget->metadata()->addCustomIcon(group2Icon, iconImage2);

    dbTarget->metadata()->copyCustomIcons(group1->customIconsRecursive(), dbSource->metadata());

    Metadata* metaTarget = dbTarget->metadata();

    QCOMPARE(metaTarget->customIconsOrder().size(), 4);
    QVERIFY(metaTarget->hasCustomIcon(group1Icon));
    QVERIFY(metaTarget->hasCustomIcon(group2Icon));
    QVERIFY(metaTarget->hasCustomIcon(entry1IconOld));
    QVERIFY(metaTarget->hasCustomIcon(entry1IconNew));

    QCOMPARE(metaTarget->customIcon(group1Icon), iconImage1);
    QCOMPARE(metaTarget->customIcon(group2Icon), iconImage2);
}

void TestGroup::testFindEntry()
{
    QScopedPointer<Database> db(new Database());

    Entry* entry1 = new Entry();
    entry1->setTitle(QString("entry1"));
    entry1->setGroup(db->rootGroup());
    entry1->setUuid(QUuid::createUuid());

    Group* group1 = new Group();
    group1->setName("group1");

    Entry* entry2 = new Entry();

    entry2->setTitle(QString("entry2"));
    entry2->setGroup(group1);
    entry2->setUuid(QUuid::createUuid());

    group1->setParent(db->rootGroup());

    Entry* entry;

    entry = db->rootGroup()->findEntryByUuid(entry1->uuid());
    QVERIFY(entry);
    QCOMPARE(entry->title(), QString("entry1"));

    entry = db->rootGroup()->findEntryByPath(QString("entry1"));
    QVERIFY(entry);
    QCOMPARE(entry->title(), QString("entry1"));

    // We also can find the entry with the leading slash.
    entry = db->rootGroup()->findEntryByPath(QString("/entry1"));
    QVERIFY(entry);
    QCOMPARE(entry->title(), QString("entry1"));

    // But two slashes should not be accepted.
    entry = db->rootGroup()->findEntryByPath(QString("//entry1"));
    QVERIFY(!entry);

    entry = db->rootGroup()->findEntryByUuid(entry2->uuid());
    QVERIFY(entry);
    QCOMPARE(entry->title(), QString("entry2"));

    entry = db->rootGroup()->findEntryByPath(QString("group1/entry2"));
    QVERIFY(entry);
    QCOMPARE(entry->title(), QString("entry2"));

    entry = db->rootGroup()->findEntryByPath(QString("/entry2"));
    QVERIFY(!entry);

    // We also can find the entry with the leading slash.
    entry = db->rootGroup()->findEntryByPath(QString("/group1/entry2"));
    QVERIFY(entry);
    QCOMPARE(entry->title(), QString("entry2"));

    // Should also find the entry only by title.
    entry = db->rootGroup()->findEntryByPath(QString("entry2"));
    QVERIFY(entry);
    QCOMPARE(entry->title(), QString("entry2"));

    entry = db->rootGroup()->findEntryByPath(QString("invalid/path/to/entry2"));
    QVERIFY(!entry);

    entry = db->rootGroup()->findEntryByPath(QString("entry27"));
    QVERIFY(!entry);

    // A valid UUID that does not exist in this database.
    entry = db->rootGroup()->findEntryByUuid(QUuid("febfb01ebcdf9dbd90a3f1579dc75281"));
    QVERIFY(!entry);

    // An invalid UUID.
    entry = db->rootGroup()->findEntryByUuid(QUuid("febfb01ebcdf9dbd90a3f1579dc"));
    QVERIFY(!entry);

    // Empty strings
    entry = db->rootGroup()->findEntryByUuid({});
    QVERIFY(!entry);

    entry = db->rootGroup()->findEntryByPath({});
    QVERIFY(!entry);
}

void TestGroup::testFindGroupByPath()
{
    QScopedPointer<Database> db(new Database());

    Group* group1 = new Group();
    group1->setName("group1");
    group1->setParent(db->rootGroup());

    Group* group2 = new Group();
    group2->setName("group2");
    group2->setParent(group1);

    Group* group;

    group = db->rootGroup()->findGroupByPath("/");
    QVERIFY(group);
    QCOMPARE(group->uuid(), db->rootGroup()->uuid());

    // We also accept it if the leading slash is missing.
    group = db->rootGroup()->findGroupByPath("");
    QVERIFY(group);
    QCOMPARE(group->uuid(), db->rootGroup()->uuid());

    group = db->rootGroup()->findGroupByPath("/group1/");
    QVERIFY(group);
    QCOMPARE(group->uuid(), group1->uuid());

    // We also accept it if the leading slash is missing.
    group = db->rootGroup()->findGroupByPath("group1/");
    QVERIFY(group);
    QCOMPARE(group->uuid(), group1->uuid());

    // Too many slashes at the end
    group = db->rootGroup()->findGroupByPath("group1//");
    QVERIFY(!group);

    // Missing a slash at the end.
    group = db->rootGroup()->findGroupByPath("/group1");
    QVERIFY(group);
    QCOMPARE(group->uuid(), group1->uuid());

    // Too many slashes at the start
    group = db->rootGroup()->findGroupByPath("//group1");
    QVERIFY(!group);

    group = db->rootGroup()->findGroupByPath("/group1/group2/");
    QVERIFY(group);
    QCOMPARE(group->uuid(), group2->uuid());

    // We also accept it if the leading slash is missing.
    group = db->rootGroup()->findGroupByPath("group1/group2/");
    QVERIFY(group);
    QCOMPARE(group->uuid(), group2->uuid());

    group = db->rootGroup()->findGroupByPath("group1/group2");
    QVERIFY(group);
    QCOMPARE(group->uuid(), group2->uuid());

    group = db->rootGroup()->findGroupByPath("invalid");
    QVERIFY(!group);
}

void TestGroup::testPrint()
{
    QScopedPointer<Database> db(new Database());

    QString output = db->rootGroup()->print();
    QCOMPARE(output, QString("[empty]\n"));

    output = db->rootGroup()->print(true);
    QCOMPARE(output, QString("[empty]\n"));

    Entry* entry1 = new Entry();
    entry1->setTitle(QString("entry1"));
    entry1->setGroup(db->rootGroup());
    entry1->setUuid(QUuid::createUuid());

    output = db->rootGroup()->print();
    QCOMPARE(output, QString("entry1\n"));

    Group* group1 = new Group();
    group1->setName("group1");
    group1->setParent(db->rootGroup());

    Entry* entry2 = new Entry();
    entry2->setTitle(QString("entry2"));
    entry2->setGroup(group1);
    entry2->setUuid(QUuid::createUuid());

    Group* group2 = new Group();
    group2->setName("group2");
    group2->setParent(db->rootGroup());

    Group* subGroup = new Group();
    subGroup->setName("subgroup");
    subGroup->setParent(group2);

    Entry* entry3 = new Entry();
    entry3->setTitle(QString("entry3"));
    entry3->setGroup(subGroup);
    entry3->setUuid(QUuid::createUuid());

    output = db->rootGroup()->print();
    QVERIFY(output.contains(QString("entry1\n")));
    QVERIFY(output.contains(QString("group1/\n")));
    QVERIFY(!output.contains(QString("  entry2\n")));
    QVERIFY(output.contains(QString("group2/\n")));
    QVERIFY(!output.contains(QString("  subgroup\n")));

    output = db->rootGroup()->print(true);
    QVERIFY(output.contains(QString("entry1\n")));
    QVERIFY(output.contains(QString("group1/\n")));
    QVERIFY(output.contains(QString("  entry2\n")));
    QVERIFY(output.contains(QString("group2/\n")));
    QVERIFY(output.contains(QString("  subgroup/\n")));
    QVERIFY(output.contains(QString("    entry3\n")));

    output = db->rootGroup()->print(true, true);
    QVERIFY(output.contains(QString("entry1\n")));
    QVERIFY(output.contains(QString("group1/\n")));
    QVERIFY(output.contains(QString("group1/entry2\n")));
    QVERIFY(output.contains(QString("group2/\n")));
    QVERIFY(output.contains(QString("group2/subgroup/\n")));
    QVERIFY(output.contains(QString("group2/subgroup/entry3\n")));

    output = group1->print();
    QVERIFY(!output.contains(QString("group1/\n")));
    QVERIFY(output.contains(QString("entry2\n")));

    output = group2->print(true, true);
    QVERIFY(!output.contains(QString("group2/\n")));
    QVERIFY(output.contains(QString("subgroup/\n")));
    QVERIFY(output.contains(QString("subgroup/entry3\n")));
}

void TestGroup::testLocate()
{
    Database* db = new Database();

    Entry* entry1 = new Entry();
    entry1->setTitle("entry1");
    entry1->setGroup(db->rootGroup());

    Entry* entry2 = new Entry();
    entry2->setTitle("entry2");
    entry2->setGroup(db->rootGroup());

    Group* group1 = new Group();
    group1->setName("group1");
    group1->setParent(db->rootGroup());

    Group* group2 = new Group();
    group2->setName("group2");
    group2->setParent(group1);

    Entry* entry3 = new Entry();
    entry3->setTitle("entry3");
    entry3->setGroup(group1);

    Entry* entry43 = new Entry();
    entry43->setTitle("entry43");
    entry43->setGroup(group1);

    Entry* google = new Entry();
    google->setTitle("Google");
    google->setGroup(group2);

    QStringList results = db->rootGroup()->locate("entry");
    QVERIFY(results.size() == 4);
    QVERIFY(results.contains("/group1/entry43"));

    results = db->rootGroup()->locate("entry1");
    QVERIFY(results.size() == 1);
    QVERIFY(results.contains("/entry1"));

    results = db->rootGroup()->locate("Entry1");
    QVERIFY(results.size() == 1);
    QVERIFY(results.contains("/entry1"));

    results = db->rootGroup()->locate("invalid");
    QVERIFY(results.isEmpty());

    results = db->rootGroup()->locate("google");
    QVERIFY(results.size() == 1);
    QVERIFY(results.contains("/group1/group2/Google"));

    results = db->rootGroup()->locate("group1");
    QVERIFY(results.size() == 3);
    QVERIFY(results.contains("/group1/entry3"));
    QVERIFY(results.contains("/group1/entry43"));
    QVERIFY(results.contains("/group1/group2/Google"));

    delete db;
}

void TestGroup::testAddEntryWithPath()
{
    Database* db = new Database();

    Group* group1 = new Group();
    group1->setName("group1");
    group1->setParent(db->rootGroup());

    Group* group2 = new Group();
    group2->setName("group2");
    group2->setParent(group1);

    Entry* entry = db->rootGroup()->addEntryWithPath("entry1");
    QVERIFY(entry);
    QVERIFY(!entry->uuid().isNull());

    entry = db->rootGroup()->addEntryWithPath("entry1");
    QVERIFY(!entry);

    entry = db->rootGroup()->addEntryWithPath("/entry1");
    QVERIFY(!entry);

    entry = db->rootGroup()->addEntryWithPath("entry2");
    QVERIFY(entry);
    QVERIFY(entry->title() == "entry2");
    QVERIFY(!entry->uuid().isNull());

    entry = db->rootGroup()->addEntryWithPath("/entry3");
    QVERIFY(entry);
    QVERIFY(entry->title() == "entry3");
    QVERIFY(!entry->uuid().isNull());

    entry = db->rootGroup()->addEntryWithPath("/group1/entry4");
    QVERIFY(entry);
    QVERIFY(entry->title() == "entry4");
    QVERIFY(!entry->uuid().isNull());

    entry = db->rootGroup()->addEntryWithPath("/group1/group2/entry5");
    QVERIFY(entry);
    QVERIFY(entry->title() == "entry5");
    QVERIFY(!entry->uuid().isNull());

    entry = db->rootGroup()->addEntryWithPath("/group1/invalid_group/entry6");
    QVERIFY(!entry);

    delete db;
}

void TestGroup::testIsRecycled()
{
    Database db;
    db.metadata()->setRecycleBinEnabled(true);

    Group* group1 = new Group();
    group1->setName("group1");
    group1->setParent(db.rootGroup());

    Group* group2 = new Group();
    group2->setName("group2");
    group2->setParent(db.rootGroup());

    Group* group3 = new Group();
    group3->setName("group3");
    group3->setParent(group2);

    Group* group4 = new Group();
    group4->setName("group4");
    group4->setParent(db.rootGroup());

    db.recycleGroup(group2);

    QVERIFY(!group1->isRecycled());
    QVERIFY(group2->isRecycled());
    QVERIFY(group3->isRecycled());
    QVERIFY(!group4->isRecycled());

    db.recycleGroup(group4);
    QVERIFY(group4->isRecycled());
}

void TestGroup::testCopyDataFrom()
{
    QScopedPointer<Group> group(new Group());
    group->setName("TestGroup");

    QScopedPointer<Group> group2(new Group());
    group2->setName("TestGroup2");

    QScopedPointer<Group> group3(new Group());
    group3->setName("TestGroup3");
    group3->customData()->set("testKey", "value");

    QSignalSpy spyGroupModified(group.data(), SIGNAL(groupModified()));
    QSignalSpy spyGroupDataChanged(group.data(), SIGNAL(groupDataChanged(Group*)));

    group->copyDataFrom(group2.data());
    QCOMPARE(spyGroupModified.count(), 1);
    QCOMPARE(spyGroupDataChanged.count(), 1);

    // if no change, no signals
    spyGroupModified.clear();
    spyGroupDataChanged.clear();
    group->copyDataFrom(group2.data());
    QCOMPARE(spyGroupModified.count(), 0);
    QCOMPARE(spyGroupDataChanged.count(), 0);

    // custom data change triggers a separate modified signal
    spyGroupModified.clear();
    spyGroupDataChanged.clear();
    group->copyDataFrom(group3.data());
    QCOMPARE(spyGroupDataChanged.count(), 1);
    QCOMPARE(spyGroupModified.count(), 2);
}

void TestGroup::testEquals()
{
    QScopedPointer<Group> group(new Group());
    group->setName("TestGroup");

    QVERIFY(group->equals(group.data(), CompareItemDefault));
}

void TestGroup::testChildrenSort()
{
    auto createTestGroupWithUnorderedChildren = []() -> Group* {
        Group* parent = new Group();

        Group* group1 = new Group();
        group1->setName("B");
        group1->setParent(parent);
        Group* group2 = new Group();
        group2->setName("e");
        group2->setParent(parent);
        Group* group3 = new Group();
        group3->setName("Test999");
        group3->setParent(parent);
        Group* group4 = new Group();
        group4->setName("A");
        group4->setParent(parent);
        Group* group5 = new Group();
        group5->setName("z");
        group5->setParent(parent);
        Group* group6 = new Group();
        group6->setName("045");
        group6->setParent(parent);
        Group* group7 = new Group();
        group7->setName("60");
        group7->setParent(parent);
        Group* group8 = new Group();
        group8->setName("04test");
        group8->setParent(parent);
        Group* group9 = new Group();
        group9->setName("Test12");
        group9->setParent(parent);
        Group* group10 = new Group();
        group10->setName("i");
        group10->setParent(parent);

        Group* subGroup1 = new Group();
        subGroup1->setName("sub_xte");
        subGroup1->setParent(group10);
        Group* subGroup2 = new Group();
        subGroup2->setName("sub_010");
        subGroup2->setParent(group10);
        Group* subGroup3 = new Group();
        subGroup3->setName("sub_000");
        subGroup3->setParent(group10);
        Group* subGroup4 = new Group();
        subGroup4->setName("sub_M");
        subGroup4->setParent(group10);
        Group* subGroup5 = new Group();
        subGroup5->setName("sub_p");
        subGroup5->setParent(group10);
        Group* subGroup6 = new Group();
        subGroup6->setName("sub_45p");
        subGroup6->setParent(group10);
        Group* subGroup7 = new Group();
        subGroup7->setName("sub_6p");
        subGroup7->setParent(group10);
        Group* subGroup8 = new Group();
        subGroup8->setName("sub_tt");
        subGroup8->setParent(group10);
        Group* subGroup9 = new Group();
        subGroup9->setName("sub_t0");
        subGroup9->setParent(group10);

        return parent;
    };

    Group* parent = createTestGroupWithUnorderedChildren();
    Group* subParent = parent->children().last();
    parent->sortChildrenRecursively();
    QList<Group*> children = parent->children();
    QCOMPARE(children.size(), 10);
    QCOMPARE(children[0]->name(), QString("045"));
    QCOMPARE(children[1]->name(), QString("04test"));
    QCOMPARE(children[2]->name(), QString("60"));
    QCOMPARE(children[3]->name(), QString("A"));
    QCOMPARE(children[4]->name(), QString("B"));
    QCOMPARE(children[5]->name(), QString("e"));
    QCOMPARE(children[6]->name(), QString("i"));
    QCOMPARE(children[7]->name(), QString("Test12"));
    QCOMPARE(children[8]->name(), QString("Test999"));
    QCOMPARE(children[9]->name(), QString("z"));
    children = subParent->children();
    QCOMPARE(children.size(), 9);
    QCOMPARE(children[0]->name(), QString("sub_000"));
    QCOMPARE(children[1]->name(), QString("sub_010"));
    QCOMPARE(children[2]->name(), QString("sub_45p"));
    QCOMPARE(children[3]->name(), QString("sub_6p"));
    QCOMPARE(children[4]->name(), QString("sub_M"));
    QCOMPARE(children[5]->name(), QString("sub_p"));
    QCOMPARE(children[6]->name(), QString("sub_t0"));
    QCOMPARE(children[7]->name(), QString("sub_tt"));
    QCOMPARE(children[8]->name(), QString("sub_xte"));
    delete parent;

    parent = createTestGroupWithUnorderedChildren();
    subParent = parent->children().last();
    parent->sortChildrenRecursively(true);
    children = parent->children();
    QCOMPARE(children.size(), 10);
    QCOMPARE(children[0]->name(), QString("z"));
    QCOMPARE(children[1]->name(), QString("Test999"));
    QCOMPARE(children[2]->name(), QString("Test12"));
    QCOMPARE(children[3]->name(), QString("i"));
    QCOMPARE(children[4]->name(), QString("e"));
    QCOMPARE(children[5]->name(), QString("B"));
    QCOMPARE(children[6]->name(), QString("A"));
    QCOMPARE(children[7]->name(), QString("60"));
    QCOMPARE(children[8]->name(), QString("04test"));
    QCOMPARE(children[9]->name(), QString("045"));
    children = subParent->children();
    QCOMPARE(children.size(), 9);
    QCOMPARE(children[0]->name(), QString("sub_xte"));
    QCOMPARE(children[1]->name(), QString("sub_tt"));
    QCOMPARE(children[2]->name(), QString("sub_t0"));
    QCOMPARE(children[3]->name(), QString("sub_p"));
    QCOMPARE(children[4]->name(), QString("sub_M"));
    QCOMPARE(children[5]->name(), QString("sub_6p"));
    QCOMPARE(children[6]->name(), QString("sub_45p"));
    QCOMPARE(children[7]->name(), QString("sub_010"));
    QCOMPARE(children[8]->name(), QString("sub_000"));
    delete parent;

    parent = createTestGroupWithUnorderedChildren();
    subParent = parent->children().last();
    subParent->sortChildrenRecursively();
    children = parent->children();
    QCOMPARE(children.size(), 10);
    QCOMPARE(children[0]->name(), QString("B"));
    QCOMPARE(children[1]->name(), QString("e"));
    QCOMPARE(children[2]->name(), QString("Test999"));
    QCOMPARE(children[3]->name(), QString("A"));
    QCOMPARE(children[4]->name(), QString("z"));
    QCOMPARE(children[5]->name(), QString("045"));
    QCOMPARE(children[6]->name(), QString("60"));
    QCOMPARE(children[7]->name(), QString("04test"));
    QCOMPARE(children[8]->name(), QString("Test12"));
    QCOMPARE(children[9]->name(), QString("i"));
    children = subParent->children();
    QCOMPARE(children.size(), 9);
    QCOMPARE(children[0]->name(), QString("sub_000"));
    QCOMPARE(children[1]->name(), QString("sub_010"));
    QCOMPARE(children[2]->name(), QString("sub_45p"));
    QCOMPARE(children[3]->name(), QString("sub_6p"));
    QCOMPARE(children[4]->name(), QString("sub_M"));
    QCOMPARE(children[5]->name(), QString("sub_p"));
    QCOMPARE(children[6]->name(), QString("sub_t0"));
    QCOMPARE(children[7]->name(), QString("sub_tt"));
    QCOMPARE(children[8]->name(), QString("sub_xte"));
    delete parent;

    parent = createTestGroupWithUnorderedChildren();
    subParent = parent->children().last();
    subParent->sortChildrenRecursively(true);
    children = parent->children();
    QCOMPARE(children.size(), 10);
    QCOMPARE(children[0]->name(), QString("B"));
    QCOMPARE(children[1]->name(), QString("e"));
    QCOMPARE(children[2]->name(), QString("Test999"));
    QCOMPARE(children[3]->name(), QString("A"));
    QCOMPARE(children[4]->name(), QString("z"));
    QCOMPARE(children[5]->name(), QString("045"));
    QCOMPARE(children[6]->name(), QString("60"));
    QCOMPARE(children[7]->name(), QString("04test"));
    QCOMPARE(children[8]->name(), QString("Test12"));
    QCOMPARE(children[9]->name(), QString("i"));
    children = subParent->children();
    QCOMPARE(children.size(), 9);
    QCOMPARE(children[0]->name(), QString("sub_xte"));
    QCOMPARE(children[1]->name(), QString("sub_tt"));
    QCOMPARE(children[2]->name(), QString("sub_t0"));
    QCOMPARE(children[3]->name(), QString("sub_p"));
    QCOMPARE(children[4]->name(), QString("sub_M"));
    QCOMPARE(children[5]->name(), QString("sub_6p"));
    QCOMPARE(children[6]->name(), QString("sub_45p"));
    QCOMPARE(children[7]->name(), QString("sub_010"));
    QCOMPARE(children[8]->name(), QString("sub_000"));
    delete parent;
}

void TestGroup::testHierarchy()
{
    Group group1;
    group1.setName("group1");

    Group* group2 = new Group();
    group2->setName("group2");
    group2->setParent(&group1);

    Group* group3 = new Group();
    group3->setName("group3");
    group3->setParent(group2);

    QStringList hierarchy = group3->hierarchy();
    QVERIFY(hierarchy.size() == 3);
    QVERIFY(hierarchy.contains("group1"));
    QVERIFY(hierarchy.contains("group2"));
    QVERIFY(hierarchy.contains("group3"));

    hierarchy = group3->hierarchy(0);
    QVERIFY(hierarchy.isEmpty());

    hierarchy = group3->hierarchy(1);
    QVERIFY(hierarchy.size() == 1);
    QVERIFY(hierarchy.contains("group3"));

    hierarchy = group3->hierarchy(2);
    QVERIFY(hierarchy.size() == 2);
    QVERIFY(hierarchy.contains("group2"));
    QVERIFY(hierarchy.contains("group3"));
}

void TestGroup::testApplyGroupIconRecursively()
{
    // Create a database with two nested groups with one entry each
    Database database;

    Group* subgroup = new Group();
    subgroup->setName("Subgroup");
    subgroup->setParent(database.rootGroup());
    QVERIFY(subgroup);

    Group* subsubgroup = new Group();
    subsubgroup->setName("Subsubgroup");
    subsubgroup->setParent(subgroup);
    QVERIFY(subsubgroup);

    Entry* subgroupEntry = subgroup->addEntryWithPath("Subgroup entry");
    QVERIFY(subgroupEntry);
    subgroup->setIcon(1);

    Entry* subsubgroupEntry = subsubgroup->addEntryWithPath("Subsubgroup entry");
    QVERIFY(subsubgroupEntry);
    subsubgroup->setIcon(2);

    // Set an icon per number to the root group and apply recursively
    // -> all groups and entries have the same icon
    const int rootIconNumber = 42;
    database.rootGroup()->setIcon(rootIconNumber);
    QVERIFY(database.rootGroup()->iconNumber() == rootIconNumber);
    database.rootGroup()->applyGroupIconToChildGroups();
    database.rootGroup()->applyGroupIconToChildEntries();
    QVERIFY(subgroup->iconNumber() == rootIconNumber);
    QVERIFY(subgroupEntry->iconNumber() == rootIconNumber);
    QVERIFY(subsubgroup->iconNumber() == rootIconNumber);
    QVERIFY(subsubgroupEntry->iconNumber() == rootIconNumber);

    // Set an icon per number to the subsubgroup and apply recursively
    // -> only the subsubgroup related groups and entries have updated icons
    const int subsubgroupIconNumber = 24;
    subsubgroup->setIcon(subsubgroupIconNumber);
    QVERIFY(subsubgroup->iconNumber() == subsubgroupIconNumber);
    subsubgroup->applyGroupIconToChildGroups();
    subsubgroup->applyGroupIconToChildEntries();
    QVERIFY(database.rootGroup()->iconNumber() == rootIconNumber);
    QVERIFY(subgroup->iconNumber() == rootIconNumber);
    QVERIFY(subgroupEntry->iconNumber() == rootIconNumber);
    QVERIFY(subsubgroup->iconNumber() == subsubgroupIconNumber);
    QVERIFY(subsubgroupEntry->iconNumber() == subsubgroupIconNumber);

    // Set an icon per UUID to the subgroup and apply recursively
    // -> all groups and entries except the root group have the same icon
    const QUuid subgroupIconUuid = QUuid::createUuid();
    QByteArray subgroupIcon("subgroup icon");

    database.metadata()->addCustomIcon(subgroupIconUuid, subgroupIcon);
    subgroup->setIcon(subgroupIconUuid);
    subgroup->applyGroupIconToChildGroups();
    subgroup->applyGroupIconToChildEntries();
    QVERIFY(database.rootGroup()->iconNumber() == rootIconNumber);
    QCOMPARE(subgroup->iconUuid(), subgroupIconUuid);
    QCOMPARE(subgroupEntry->iconUuid(), subgroupIconUuid);
    QCOMPARE(subsubgroup->iconUuid(), subgroupIconUuid);
    QCOMPARE(subsubgroupEntry->iconUuid(), subgroupIconUuid);
    QCOMPARE(subgroup->database()->metadata()->customIcon(subgroupIconUuid), subgroupIcon);

    // Reset all icons to root icon
    database.rootGroup()->setIcon(rootIconNumber);
    QVERIFY(database.rootGroup()->iconNumber() == rootIconNumber);
    database.rootGroup()->applyGroupIconToChildGroups();
    database.rootGroup()->applyGroupIconToChildEntries();
    QVERIFY(subgroup->iconNumber() == rootIconNumber);
    QVERIFY(subgroupEntry->iconNumber() == rootIconNumber);
    QVERIFY(subsubgroup->iconNumber() == rootIconNumber);
    QVERIFY(subsubgroupEntry->iconNumber() == rootIconNumber);

    // Apply only for child groups
    const int iconForGroups = 10;
    database.rootGroup()->setIcon(iconForGroups);
    QVERIFY(database.rootGroup()->iconNumber() == iconForGroups);
    database.rootGroup()->applyGroupIconToChildGroups();
    QVERIFY(database.rootGroup()->iconNumber() == iconForGroups);
    QVERIFY(subgroup->iconNumber() == iconForGroups);
    QVERIFY(subgroupEntry->iconNumber() == rootIconNumber);
    QVERIFY(subsubgroup->iconNumber() == iconForGroups);
    QVERIFY(subsubgroupEntry->iconNumber() == rootIconNumber);

    // Apply only for child entries
    const int iconForEntries = 20;
    database.rootGroup()->setIcon(iconForEntries);
    QVERIFY(database.rootGroup()->iconNumber() == iconForEntries);
    database.rootGroup()->applyGroupIconToChildEntries();
    QVERIFY(database.rootGroup()->iconNumber() == iconForEntries);
    QVERIFY(subgroup->iconNumber() == iconForGroups);
    QVERIFY(subgroupEntry->iconNumber() == iconForEntries);
    QVERIFY(subsubgroup->iconNumber() == iconForGroups);
    QVERIFY(subsubgroupEntry->iconNumber() == iconForEntries);
}

void TestGroup::testUsernamesRecursive()
{
    Database database;

    // Create a subgroup
    Group* subgroup = new Group();
    subgroup->setName("Subgroup");
    subgroup->setParent(database.rootGroup());

    // Generate entries in the root group and the subgroup
    Entry* rootGroupEntry = database.rootGroup()->addEntryWithPath("Root group entry");
    rootGroupEntry->setUsername("Name1");

    Entry* subgroupEntry = subgroup->addEntryWithPath("Subgroup entry");
    subgroupEntry->setUsername("Name2");

    Entry* subgroupEntryReusingUsername = subgroup->addEntryWithPath("Another subgroup entry");
    subgroupEntryReusingUsername->setUsername("Name2");

    QList<QString> usernames = database.rootGroup()->usernamesRecursive();
    QCOMPARE(usernames.size(), 2);
    QVERIFY(usernames.contains("Name1"));
    QVERIFY(usernames.contains("Name2"));
    QVERIFY(usernames.indexOf("Name2") < usernames.indexOf("Name1"));
}

void TestGroup::testMove()
{
    Database database;
    Group* root = database.rootGroup();
    QVERIFY(root);

    Entry* entry0 = new Entry();
    QVERIFY(entry0);
    entry0->setGroup(root);
    Entry* entry1 = new Entry();
    QVERIFY(entry1);
    entry1->setGroup(root);
    Entry* entry2 = new Entry();
    QVERIFY(entry2);
    entry2->setGroup(root);
    Entry* entry3 = new Entry();
    QVERIFY(entry3);
    entry3->setGroup(root);
    // default order, straight
    QCOMPARE(root->entries().at(0), entry0);
    QCOMPARE(root->entries().at(1), entry1);
    QCOMPARE(root->entries().at(2), entry2);
    QCOMPARE(root->entries().at(3), entry3);

    root->moveEntryDown(entry0);
    QCOMPARE(root->entries().at(0), entry1);
    QCOMPARE(root->entries().at(1), entry0);
    QCOMPARE(root->entries().at(2), entry2);
    QCOMPARE(root->entries().at(3), entry3);

    root->moveEntryDown(entry0);
    QCOMPARE(root->entries().at(0), entry1);
    QCOMPARE(root->entries().at(1), entry2);
    QCOMPARE(root->entries().at(2), entry0);
    QCOMPARE(root->entries().at(3), entry3);

    root->moveEntryDown(entry0);
    QCOMPARE(root->entries().at(0), entry1);
    QCOMPARE(root->entries().at(1), entry2);
    QCOMPARE(root->entries().at(2), entry3);
    QCOMPARE(root->entries().at(3), entry0);

    // no effect
    root->moveEntryDown(entry0);
    QCOMPARE(root->entries().at(0), entry1);
    QCOMPARE(root->entries().at(1), entry2);
    QCOMPARE(root->entries().at(2), entry3);
    QCOMPARE(root->entries().at(3), entry0);

    root->moveEntryUp(entry0);
    QCOMPARE(root->entries().at(0), entry1);
    QCOMPARE(root->entries().at(1), entry2);
    QCOMPARE(root->entries().at(2), entry0);
    QCOMPARE(root->entries().at(3), entry3);

    root->moveEntryUp(entry0);
    QCOMPARE(root->entries().at(0), entry1);
    QCOMPARE(root->entries().at(1), entry0);
    QCOMPARE(root->entries().at(2), entry2);
    QCOMPARE(root->entries().at(3), entry3);

    root->moveEntryUp(entry0);
    QCOMPARE(root->entries().at(0), entry0);
    QCOMPARE(root->entries().at(1), entry1);
    QCOMPARE(root->entries().at(2), entry2);
    QCOMPARE(root->entries().at(3), entry3);

    // no effect
    root->moveEntryUp(entry0);
    QCOMPARE(root->entries().at(0), entry0);
    QCOMPARE(root->entries().at(1), entry1);
    QCOMPARE(root->entries().at(2), entry2);
    QCOMPARE(root->entries().at(3), entry3);

    root->moveEntryUp(entry2);
    QCOMPARE(root->entries().at(0), entry0);
    QCOMPARE(root->entries().at(1), entry2);
    QCOMPARE(root->entries().at(2), entry1);
    QCOMPARE(root->entries().at(3), entry3);

    root->moveEntryDown(entry0);
    QCOMPARE(root->entries().at(0), entry2);
    QCOMPARE(root->entries().at(1), entry0);
    QCOMPARE(root->entries().at(2), entry1);
    QCOMPARE(root->entries().at(3), entry3);

    root->moveEntryUp(entry3);
    QCOMPARE(root->entries().at(0), entry2);
    QCOMPARE(root->entries().at(1), entry0);
    QCOMPARE(root->entries().at(2), entry3);
    QCOMPARE(root->entries().at(3), entry1);

    root->moveEntryUp(entry3);
    QCOMPARE(root->entries().at(0), entry2);
    QCOMPARE(root->entries().at(1), entry3);
    QCOMPARE(root->entries().at(2), entry0);
    QCOMPARE(root->entries().at(3), entry1);

    root->moveEntryDown(entry2);
    QCOMPARE(root->entries().at(0), entry3);
    QCOMPARE(root->entries().at(1), entry2);
    QCOMPARE(root->entries().at(2), entry0);
    QCOMPARE(root->entries().at(3), entry1);

    root->moveEntryUp(entry1);
    QCOMPARE(root->entries().at(0), entry3);
    QCOMPARE(root->entries().at(1), entry2);
    QCOMPARE(root->entries().at(2), entry1);
    QCOMPARE(root->entries().at(3), entry0);
}
