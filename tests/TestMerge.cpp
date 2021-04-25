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

#include "TestMerge.h"
#include "TestGlobal.h"
#include "mock/MockClock.h"

#include "core/Merger.h"
#include "core/Metadata.h"
#include "crypto/Crypto.h"

#include <QSignalSpy>

QTEST_GUILESS_MAIN(TestMerge)

namespace
{
    TimeInfo modificationTime(TimeInfo timeInfo, int years, int months, int days)
    {
        const QDateTime time = timeInfo.lastModificationTime();
        timeInfo.setLastModificationTime(time.addYears(years).addMonths(months).addDays(days));
        return timeInfo;
    }

    MockClock* m_clock = nullptr;
} // namespace

void TestMerge::initTestCase()
{
    qRegisterMetaType<Entry*>("Entry*");
    qRegisterMetaType<Group*>("Group*");
    QVERIFY(Crypto::init());
}

void TestMerge::init()
{
    Q_ASSERT(m_clock == nullptr);
    m_clock = new MockClock(2010, 5, 5, 10, 30, 10);
    MockClock::setup(m_clock);
}

void TestMerge::cleanup()
{
    MockClock::teardown();
    m_clock = nullptr;
}

/**
 * Merge an existing database into a new one.
 * All the entries of the existing should end
 * up in the new one.
 */
void TestMerge::testMergeIntoNew()
{
    QScopedPointer<Database> dbSource(createTestDatabase());
    QScopedPointer<Database> dbDestination(new Database());

    Merger merger(dbSource.data(), dbDestination.data());
    merger.merge();

    QCOMPARE(dbDestination->rootGroup()->children().size(), 2);
    QCOMPARE(dbDestination->rootGroup()->children().at(0)->entries().size(), 2);
    // Test for retention of history
    QCOMPARE(dbDestination->rootGroup()->children().at(0)->entries().at(0)->historyItems().isEmpty(), false);
}

/**
 * Merging when no changes occured should not
 * have any side effect.
 */
void TestMerge::testMergeNoChanges()
{
    QScopedPointer<Database> dbDestination(createTestDatabase());
    QScopedPointer<Database> dbSource(
        createTestDatabaseStructureClone(dbDestination.data(), Entry::CloneNoFlags, Group::CloneIncludeEntries));

    QCOMPARE(dbDestination->rootGroup()->entriesRecursive().size(), 2);
    QCOMPARE(dbSource->rootGroup()->entriesRecursive().size(), 2);

    m_clock->advanceSecond(1);

    Merger merger1(dbSource.data(), dbDestination.data());
    merger1.merge();

    QCOMPARE(dbDestination->rootGroup()->entriesRecursive().size(), 2);
    QCOMPARE(dbSource->rootGroup()->entriesRecursive().size(), 2);

    m_clock->advanceSecond(1);

    Merger merger2(dbSource.data(), dbDestination.data());
    merger2.merge();

    QCOMPARE(dbDestination->rootGroup()->entriesRecursive().size(), 2);
    QCOMPARE(dbSource->rootGroup()->entriesRecursive().size(), 2);
}

/**
 * If the entry is updated in the source database, the update
 * should propagate in the destination database.
 */
void TestMerge::testResolveConflictNewer()
{
    QScopedPointer<Database> dbDestination(createTestDatabase());
    QScopedPointer<Database> dbSource(
        createTestDatabaseStructureClone(dbDestination.data(), Entry::CloneNoFlags, Group::CloneIncludeEntries));

    // sanity check
    QPointer<Group> groupSourceInitial = dbSource->rootGroup()->findChildByName("group1");
    QVERIFY(groupSourceInitial != nullptr);
    QCOMPARE(groupSourceInitial->entries().size(), 2);

    QPointer<Group> groupDestinationInitial = dbSource->rootGroup()->findChildByName("group1");
    QVERIFY(groupDestinationInitial != nullptr);
    QCOMPARE(groupDestinationInitial->entries().size(), 2);

    QPointer<Entry> entrySourceInitial = dbSource->rootGroup()->findEntryByPath("entry1");
    QVERIFY(entrySourceInitial != nullptr);
    QVERIFY(entrySourceInitial->group() == groupSourceInitial);

    const TimeInfo entrySourceInitialTimeInfo = entrySourceInitial->timeInfo();
    const TimeInfo groupSourceInitialTimeInfo = groupSourceInitial->timeInfo();
    const TimeInfo groupDestinationInitialTimeInfo = groupDestinationInitial->timeInfo();

    // Make sure the two changes have a different timestamp.
    m_clock->advanceSecond(1);
    // make this entry newer than in destination db
    entrySourceInitial->beginUpdate();
    entrySourceInitial->setPassword("password");
    entrySourceInitial->endUpdate();

    const TimeInfo entrySourceUpdatedTimeInfo = entrySourceInitial->timeInfo();
    const TimeInfo groupSourceUpdatedTimeInfo = groupSourceInitial->timeInfo();

    QVERIFY(entrySourceInitialTimeInfo != entrySourceUpdatedTimeInfo);
    QVERIFY(groupSourceInitialTimeInfo == groupSourceUpdatedTimeInfo);
    QVERIFY(groupSourceInitialTimeInfo == groupDestinationInitialTimeInfo);

    // Make sure the merge changes have a different timestamp.
    m_clock->advanceSecond(1);

    Merger merger(dbSource.data(), dbDestination.data());
    merger.merge();

    // sanity check
    QPointer<Group> groupDestinationMerged = dbDestination->rootGroup()->findChildByName("group1");
    QVERIFY(groupDestinationMerged != nullptr);
    QCOMPARE(groupDestinationMerged->entries().size(), 2);
    QCOMPARE(groupDestinationMerged->timeInfo(), groupDestinationInitialTimeInfo);

    QPointer<Entry> entryDestinationMerged = dbDestination->rootGroup()->findEntryByPath("entry1");
    QVERIFY(entryDestinationMerged != nullptr);
    QVERIFY(entryDestinationMerged->group() != nullptr);
    QCOMPARE(entryDestinationMerged->password(), QString("password"));
    QCOMPARE(entryDestinationMerged->timeInfo(), entrySourceUpdatedTimeInfo);

    // When updating an entry, it should not end up in the
    // deleted objects.
    for (DeletedObject deletedObject : dbDestination->deletedObjects()) {
        QVERIFY(deletedObject.uuid != entryDestinationMerged->uuid());
    }
}

/**
 * If the entry is updated in the source database, and the
 * destination database after, the entry should remain the
 * same.
 */
void TestMerge::testResolveConflictExisting()
{
    QScopedPointer<Database> dbDestination(createTestDatabase());
    QScopedPointer<Database> dbSource(
        createTestDatabaseStructureClone(dbDestination.data(), Entry::CloneNoFlags, Group::CloneIncludeEntries));

    // sanity check
    QPointer<Group> groupSourceInitial = dbSource->rootGroup()->findChildByName("group1");
    QVERIFY(groupSourceInitial != nullptr);
    QCOMPARE(groupSourceInitial->entries().size(), 2);

    QPointer<Group> groupDestinationInitial = dbDestination->rootGroup()->findChildByName("group1");
    QVERIFY(groupDestinationInitial != nullptr);
    QCOMPARE(groupSourceInitial->entries().size(), 2);

    QPointer<Entry> entrySourceInitial = dbSource->rootGroup()->findEntryByPath("entry1");
    QVERIFY(entrySourceInitial != nullptr);
    QVERIFY(entrySourceInitial->group() == groupSourceInitial);

    const TimeInfo entrySourceInitialTimeInfo = entrySourceInitial->timeInfo();
    const TimeInfo groupSourceInitialTimeInfo = groupSourceInitial->timeInfo();
    const TimeInfo groupDestinationInitialTimeInfo = groupDestinationInitial->timeInfo();

    // Make sure the two changes have a different timestamp.
    m_clock->advanceSecond(1);
    // make this entry older than in destination db
    entrySourceInitial->beginUpdate();
    entrySourceInitial->setPassword("password1");
    entrySourceInitial->endUpdate();

    const TimeInfo entrySourceUpdatedOlderTimeInfo = entrySourceInitial->timeInfo();
    const TimeInfo groupSourceUpdatedOlderTimeInfo = groupSourceInitial->timeInfo();

    QPointer<Group> groupDestinationUpdated = dbDestination->rootGroup()->findChildByName("group1");
    QVERIFY(groupDestinationUpdated != nullptr);
    QCOMPARE(groupDestinationUpdated->entries().size(), 2);
    QPointer<Entry> entryDestinationUpdated = dbDestination->rootGroup()->findEntryByPath("entry1");
    QVERIFY(entryDestinationUpdated != nullptr);
    QVERIFY(entryDestinationUpdated->group() == groupDestinationUpdated);

    // Make sure the two changes have a different timestamp.
    m_clock->advanceSecond(1);
    // make this entry newer than in source db
    entryDestinationUpdated->beginUpdate();
    entryDestinationUpdated->setPassword("password2");
    entryDestinationUpdated->endUpdate();

    const TimeInfo entryDestinationUpdatedNewerTimeInfo = entryDestinationUpdated->timeInfo();
    const TimeInfo groupDestinationUpdatedNewerTimeInfo = groupDestinationUpdated->timeInfo();
    QVERIFY(entrySourceUpdatedOlderTimeInfo != entrySourceInitialTimeInfo);
    QVERIFY(entrySourceUpdatedOlderTimeInfo != entryDestinationUpdatedNewerTimeInfo);
    QVERIFY(groupSourceInitialTimeInfo == groupSourceUpdatedOlderTimeInfo);
    QVERIFY(groupDestinationInitialTimeInfo == groupDestinationUpdatedNewerTimeInfo);
    QVERIFY(groupSourceInitialTimeInfo == groupDestinationInitialTimeInfo);

    // Make sure the merge changes have a different timestamp.
    m_clock->advanceSecond(1);

    Merger merger(dbSource.data(), dbDestination.data());
    merger.merge();

    // sanity check
    QPointer<Group> groupDestinationMerged = dbDestination->rootGroup()->findChildByName("group1");
    QVERIFY(groupDestinationMerged != nullptr);
    QCOMPARE(groupDestinationMerged->entries().size(), 2);
    QCOMPARE(groupDestinationMerged->timeInfo(), groupDestinationUpdatedNewerTimeInfo);

    QPointer<Entry> entryDestinationMerged = dbDestination->rootGroup()->findEntryByPath("entry1");
    QVERIFY(entryDestinationMerged != nullptr);
    QCOMPARE(entryDestinationMerged->password(), QString("password2"));
    QCOMPARE(entryDestinationMerged->timeInfo(), entryDestinationUpdatedNewerTimeInfo);

    // When updating an entry, it should not end up in the
    // deleted objects.
    for (DeletedObject deletedObject : dbDestination->deletedObjects()) {
        QVERIFY(deletedObject.uuid != entryDestinationMerged->uuid());
    }
}

/**
 * Tests the KeepBoth merge mode.
 */
void TestMerge::testResolveConflictDuplicate()
{
    QScopedPointer<Database> dbDestination(createTestDatabase());
    QScopedPointer<Database> dbSource(
        createTestDatabaseStructureClone(dbDestination.data(), Entry::CloneIncludeHistory, Group::CloneIncludeEntries));

    // sanity check
    QCOMPARE(dbDestination->rootGroup()->children().at(0)->entries().size(), 2);

    // make this entry newer than in original db
    QPointer<Entry> updatedDestinationEntry = dbDestination->rootGroup()->children().at(0)->entries().at(0);
    const TimeInfo initialEntryTimeInfo = updatedDestinationEntry->timeInfo();
    const TimeInfo updatedEntryTimeInfo = modificationTime(initialEntryTimeInfo, 1, 0, 0);

    updatedDestinationEntry->setTimeInfo(updatedEntryTimeInfo);

    dbDestination->rootGroup()->setMergeMode(Group::MergeMode::Duplicate);

    // Make sure the merge changes have a different timestamp.
    m_clock->advanceSecond(1);

    Merger merger(dbSource.data(), dbDestination.data());
    merger.merge();

    // one entry is duplicated because of mode
    QCOMPARE(dbDestination->rootGroup()->children().at(0)->entries().size(), 3);
    QCOMPARE(dbDestination->rootGroup()->children().at(0)->entries().at(0)->historyItems().isEmpty(), false);
    // the older entry was merged from the other db as last in the group
    QPointer<Entry> newerEntry = dbDestination->rootGroup()->children().at(0)->entries().at(0);
    QPointer<Entry> olderEntry = dbDestination->rootGroup()->children().at(0)->entries().at(2);
    QVERIFY(newerEntry->title() == olderEntry->title());
    QVERIFY2(!newerEntry->attributes()->hasKey("merged"), "newer entry is not marked with an attribute \"merged\"");
    QVERIFY2(olderEntry->attributes()->hasKey("merged"), "older entry is marked with an attribute \"merged\"");
    QCOMPARE(olderEntry->historyItems().isEmpty(), false);
    QCOMPARE(newerEntry->timeInfo(), updatedEntryTimeInfo);
    // TODO HNH: this may be subject to discussions since the entry itself is newer but represents an older one
    // QCOMPARE(olderEntry->timeInfo(), initialEntryTimeInfo);
    QVERIFY2(olderEntry->uuidToHex() != updatedDestinationEntry->uuidToHex(),
             "KeepBoth should not reuse the UUIDs when cloning.");
}

void TestMerge::testResolveConflictTemplate(
    int mergeMode,
    std::function<void(Database*, const QMap<const char*, QDateTime>&)> verification)
{
    QMap<const char*, QDateTime> timestamps;
    timestamps["initialTime"] = m_clock->currentDateTimeUtc();
    QScopedPointer<Database> dbDestination(createTestDatabase());

    Entry* deletedEntry1 = new Entry();
    deletedEntry1->setUuid(QUuid::createUuid());

    deletedEntry1->beginUpdate();
    deletedEntry1->setGroup(dbDestination->rootGroup());
    deletedEntry1->setTitle("deletedDestination");
    deletedEntry1->endUpdate();

    Entry* deletedEntry2 = new Entry();
    deletedEntry2->setUuid(QUuid::createUuid());

    deletedEntry2->beginUpdate();
    deletedEntry2->setGroup(dbDestination->rootGroup());
    deletedEntry2->setTitle("deletedSource");
    deletedEntry2->endUpdate();

    QScopedPointer<Database> dbSource(
        createTestDatabaseStructureClone(dbDestination.data(), Entry::CloneIncludeHistory, Group::CloneIncludeEntries));

    timestamps["oldestCommonHistoryTime"] = m_clock->currentDateTimeUtc();

    // sanity check
    QCOMPARE(dbDestination->rootGroup()->children().at(0)->entries().size(), 2);
    QCOMPARE(dbDestination->rootGroup()->children().at(0)->entries().at(0)->historyItems().count(), 1);
    QCOMPARE(dbDestination->rootGroup()->children().at(0)->entries().at(1)->historyItems().count(), 1);
    QCOMPARE(dbSource->rootGroup()->children().at(0)->entries().size(), 2);
    QCOMPARE(dbSource->rootGroup()->children().at(0)->entries().at(0)->historyItems().count(), 1);
    QCOMPARE(dbSource->rootGroup()->children().at(0)->entries().at(1)->historyItems().count(), 1);

    // simulate some work in the dbs (manipulate the history)
    QPointer<Entry> destinationEntry1 = dbDestination->rootGroup()->children().at(0)->entries().at(0);
    QPointer<Entry> destinationEntry2 = dbDestination->rootGroup()->children().at(0)->entries().at(1);
    QPointer<Entry> sourceEntry1 = dbSource->rootGroup()->children().at(0)->entries().at(0);
    QPointer<Entry> sourceEntry2 = dbSource->rootGroup()->children().at(0)->entries().at(1);

    timestamps["newestCommonHistoryTime"] = m_clock->advanceMinute(1);

    destinationEntry1->beginUpdate();
    destinationEntry1->setNotes("1 Common");
    destinationEntry1->endUpdate();
    destinationEntry2->beginUpdate();
    destinationEntry2->setNotes("1 Common");
    destinationEntry2->endUpdate();
    sourceEntry1->beginUpdate();
    sourceEntry1->setNotes("1 Common");
    sourceEntry1->endUpdate();
    sourceEntry2->beginUpdate();
    sourceEntry2->setNotes("1 Common");
    sourceEntry2->endUpdate();

    timestamps["oldestDivergingHistoryTime"] = m_clock->advanceSecond(1);

    destinationEntry2->beginUpdate();
    destinationEntry2->setNotes("2 Destination");
    destinationEntry2->endUpdate();
    sourceEntry1->beginUpdate();
    sourceEntry1->setNotes("2 Source");
    sourceEntry1->endUpdate();

    timestamps["newestDivergingHistoryTime"] = m_clock->advanceHour(1);

    destinationEntry1->beginUpdate();
    destinationEntry1->setNotes("3 Destination");
    destinationEntry1->endUpdate();
    sourceEntry2->beginUpdate();
    sourceEntry2->setNotes("3 Source");
    sourceEntry2->endUpdate();

    // sanity check
    QCOMPARE(dbDestination->rootGroup()->children().at(0)->entries().at(0)->historyItems().count(), 3);
    QCOMPARE(dbDestination->rootGroup()->children().at(0)->entries().at(1)->historyItems().count(), 3);
    QCOMPARE(dbSource->rootGroup()->children().at(0)->entries().at(0)->historyItems().count(), 3);
    QCOMPARE(dbSource->rootGroup()->children().at(0)->entries().at(1)->historyItems().count(), 3);

    m_clock->advanceMinute(1);

    QPointer<Entry> deletedEntryDestination = dbDestination->rootGroup()->findEntryByPath("deletedDestination");
    dbDestination->recycleEntry(deletedEntryDestination);
    QPointer<Entry> deletedEntrySource = dbSource->rootGroup()->findEntryByPath("deletedSource");
    dbSource->recycleEntry(deletedEntrySource);

    m_clock->advanceMinute(1);

    Entry* destinationEntrySingle = new Entry();
    destinationEntrySingle->setUuid(QUuid::createUuid());

    destinationEntrySingle->beginUpdate();
    destinationEntrySingle->setGroup(dbDestination->rootGroup()->children().at(1));
    destinationEntrySingle->setTitle("entryDestination");
    destinationEntrySingle->endUpdate();

    Entry* sourceEntrySingle = new Entry();
    sourceEntrySingle->setUuid(QUuid::createUuid());

    sourceEntrySingle->beginUpdate();
    sourceEntrySingle->setGroup(dbSource->rootGroup()->children().at(1));
    sourceEntrySingle->setTitle("entrySource");
    sourceEntrySingle->endUpdate();

    dbDestination->rootGroup()->setMergeMode(static_cast<Group::MergeMode>(mergeMode));

    // Make sure the merge changes have a different timestamp.
    timestamps["mergeTime"] = m_clock->advanceSecond(1);

    Merger merger(dbSource.data(), dbDestination.data());
    merger.merge();

    QPointer<Group> mergedRootGroup = dbDestination->rootGroup();
    QCOMPARE(mergedRootGroup->entries().size(), 0);
    // Both databases contain their own generated recycleBin - just one is considered a real recycleBin, the other
    // exists as normal group, therefore only one entry is considered deleted
    QCOMPARE(dbDestination->metadata()->recycleBin()->entries().size(), 1);
    QPointer<Group> mergedGroup1 = mergedRootGroup->children().at(0);
    QPointer<Group> mergedGroup2 = mergedRootGroup->children().at(1);
    QVERIFY(mergedGroup1);
    QVERIFY(mergedGroup2);
    QCOMPARE(mergedGroup2->entries().size(), 2);
    QVERIFY(mergedGroup1->entries().at(0));
    QVERIFY(mergedGroup1->entries().at(1));

    verification(dbDestination.data(), timestamps);

    QVERIFY(dbDestination->rootGroup()->findEntryByPath("entryDestination"));
    QVERIFY(dbDestination->rootGroup()->findEntryByPath("entrySource"));
}

void TestMerge::testDeletionConflictTemplate(int mergeMode,
                                             std::function<void(Database*, const QMap<QString, QUuid>&)> verification)
{
    QMap<QString, QUuid> identifiers;
    m_clock->currentDateTimeUtc();
    QScopedPointer<Database> dbDestination(createTestDatabase());

    // scenarios:
    //   entry directly deleted in source before updated in target
    //   entry directly deleted in source after updated in target
    //   entry directly deleted in target before updated in source
    //   entry directly deleted in target after updated in source

    //   entry indirectly deleted in source before updated in target
    //   entry indirectly deleted in source after updated in target
    //   entry indirectly deleted in target before updated in source
    //   entry indirectly deleted in target after updated in source

    auto createGroup = [&](const char* name, Group* parent) {
        Group* group = new Group();
        group->setUuid(QUuid::createUuid());
        group->setName(name);
        group->setParent(parent, 0);
        identifiers[group->name()] = group->uuid();
        return group;
    };
    auto createEntry = [&](const char* title, Group* parent) {
        Entry* entry = new Entry();
        entry->setUuid(QUuid::createUuid());
        entry->setTitle(title);
        entry->setGroup(parent);
        identifiers[entry->title()] = entry->uuid();
        return entry;
    };
    auto changeEntry = [](Entry* entry) {
        entry->beginUpdate();
        entry->setNotes("Change");
        entry->endUpdate();
    };

    Group* directlyDeletedEntryGroup = createGroup("DirectlyDeletedEntries", dbDestination->rootGroup());
    createEntry("EntryDeletedInSourceBeforeChangedInTarget", directlyDeletedEntryGroup);
    createEntry("EntryDeletedInSourceAfterChangedInTarget", directlyDeletedEntryGroup);
    createEntry("EntryDeletedInTargetBeforeChangedInSource", directlyDeletedEntryGroup);
    createEntry("EntryDeletedInTargetAfterChangedInSource", directlyDeletedEntryGroup);

    Group* groupDeletedInSourceBeforeEntryUpdatedInTarget =
        createGroup("GroupDeletedInSourceBeforeEntryUpdatedInTarget", dbDestination->rootGroup());
    createEntry("EntryDeletedInSourceBeforeEntryUpdatedInTarget", groupDeletedInSourceBeforeEntryUpdatedInTarget);

    Group* groupDeletedInSourceAfterEntryUpdatedInTarget =
        createGroup("GroupDeletedInSourceAfterEntryUpdatedInTarget", dbDestination->rootGroup());
    createEntry("EntryDeletedInSourceAfterEntryUpdatedInTarget", groupDeletedInSourceAfterEntryUpdatedInTarget);

    Group* groupDeletedInTargetBeforeEntryUpdatedInSource =
        createGroup("GroupDeletedInTargetBeforeEntryUpdatedInSource", dbDestination->rootGroup());
    createEntry("EntryDeletedInTargetBeforeEntryUpdatedInSource", groupDeletedInTargetBeforeEntryUpdatedInSource);

    Group* groupDeletedInTargetAfterEntryUpdatedInSource =
        createGroup("GroupDeletedInTargetAfterEntryUpdatedInSource", dbDestination->rootGroup());
    createEntry("EntryDeletedInTargetAfterEntryUpdatedInSource", groupDeletedInTargetAfterEntryUpdatedInSource);

    QScopedPointer<Database> dbSource(
        createTestDatabaseStructureClone(dbDestination.data(), Entry::CloneIncludeHistory, Group::CloneIncludeEntries));

    QPointer<Entry> sourceEntryDeletedInSourceBeforeChangedInTarget =
        dbSource->rootGroup()->findEntryByUuid(identifiers["EntryDeletedInSourceBeforeChangedInTarget"]);
    QPointer<Entry> targetEntryDeletedInSourceBeforeChangedInTarget =
        dbDestination->rootGroup()->findEntryByUuid(identifiers["EntryDeletedInSourceBeforeChangedInTarget"]);

    QPointer<Entry> sourceEntryDeletedInSourceAfterChangedInTarget =
        dbSource->rootGroup()->findEntryByUuid(identifiers["EntryDeletedInSourceAfterChangedInTarget"]);
    QPointer<Entry> targetEntryDeletedInSourceAfterChangedInTarget =
        dbDestination->rootGroup()->findEntryByUuid(identifiers["EntryDeletedInSourceAfterChangedInTarget"]);

    QPointer<Entry> sourceEntryDeletedInTargetBeforeChangedInSource =
        dbSource->rootGroup()->findEntryByUuid(identifiers["EntryDeletedInTargetBeforeChangedInSource"]);
    QPointer<Entry> targetEntryDeletedInTargetBeforeChangedInSource =
        dbDestination->rootGroup()->findEntryByUuid(identifiers["EntryDeletedInTargetBeforeChangedInSource"]);

    QPointer<Entry> sourceEntryDeletedInTargetAfterChangedInSource =
        dbSource->rootGroup()->findEntryByUuid(identifiers["EntryDeletedInTargetAfterChangedInSource"]);
    QPointer<Entry> targetEntryDeletedInTargetAfterChangedInSource =
        dbDestination->rootGroup()->findEntryByUuid(identifiers["EntryDeletedInTargetAfterChangedInSource"]);

    QPointer<Group> sourceGroupDeletedInSourceBeforeEntryUpdatedInTarget =
        dbSource->rootGroup()->findGroupByUuid(identifiers["GroupDeletedInSourceBeforeEntryUpdatedInTarget"]);
    QPointer<Entry> targetEntryDeletedInSourceBeforeEntryUpdatedInTarget =
        dbDestination->rootGroup()->findEntryByUuid(identifiers["EntryDeletedInSourceBeforeEntryUpdatedInTarget"]);

    QPointer<Group> sourceGroupDeletedInSourceAfterEntryUpdatedInTarget =
        dbSource->rootGroup()->findGroupByUuid(identifiers["GroupDeletedInSourceAfterEntryUpdatedInTarget"]);
    QPointer<Entry> targetEntryDeletedInSourceAfterEntryUpdatedInTarget =
        dbDestination->rootGroup()->findEntryByUuid(identifiers["EntryDeletedInSourceAfterEntryUpdatedInTarget"]);

    QPointer<Group> targetGroupDeletedInTargetBeforeEntryUpdatedInSource =
        dbDestination->rootGroup()->findGroupByUuid(identifiers["GroupDeletedInTargetBeforeEntryUpdatedInSource"]);
    QPointer<Entry> sourceEntryDeletedInTargetBeforeEntryUpdatedInSource =
        dbSource->rootGroup()->findEntryByUuid(identifiers["EntryDeletedInTargetBeforeEntryUpdatedInSource"]);

    QPointer<Group> targetGroupDeletedInTargetAfterEntryUpdatedInSource =
        dbDestination->rootGroup()->findGroupByUuid(identifiers["GroupDeletedInTargetAfterEntryUpdatedInSource"]);
    QPointer<Entry> sourceEntryDeletedInTargetAfterEntryUpdatedInSoruce =
        dbSource->rootGroup()->findEntryByUuid(identifiers["EntryDeletedInTargetAfterEntryUpdatedInSource"]);

    // simulate some work in the dbs (manipulate the history)
    m_clock->advanceMinute(1);

    delete sourceEntryDeletedInSourceBeforeChangedInTarget.data();
    changeEntry(targetEntryDeletedInSourceAfterChangedInTarget);
    delete targetEntryDeletedInTargetBeforeChangedInSource.data();
    changeEntry(sourceEntryDeletedInTargetAfterChangedInSource);

    delete sourceGroupDeletedInSourceBeforeEntryUpdatedInTarget.data();
    changeEntry(targetEntryDeletedInSourceAfterEntryUpdatedInTarget);
    delete targetGroupDeletedInTargetBeforeEntryUpdatedInSource.data();
    changeEntry(sourceEntryDeletedInTargetAfterEntryUpdatedInSoruce);

    m_clock->advanceMinute(1);

    changeEntry(targetEntryDeletedInSourceBeforeChangedInTarget);
    delete sourceEntryDeletedInSourceAfterChangedInTarget.data();
    changeEntry(sourceEntryDeletedInTargetBeforeChangedInSource);
    delete targetEntryDeletedInTargetAfterChangedInSource.data();

    changeEntry(targetEntryDeletedInSourceBeforeEntryUpdatedInTarget);
    delete sourceGroupDeletedInSourceAfterEntryUpdatedInTarget.data();
    changeEntry(sourceEntryDeletedInTargetBeforeEntryUpdatedInSource);
    delete targetGroupDeletedInTargetAfterEntryUpdatedInSource.data();
    m_clock->advanceMinute(1);

    dbDestination->rootGroup()->setMergeMode(static_cast<Group::MergeMode>(mergeMode));

    Merger merger(dbSource.data(), dbDestination.data());
    merger.merge();

    verification(dbDestination.data(), identifiers);
}

void TestMerge::assertDeletionNewerOnly(Database* db, const QMap<QString, QUuid>& identifiers)
{
    QPointer<Group> mergedRootGroup = db->rootGroup();
    // newer change in target prevents deletion
    QVERIFY(mergedRootGroup->findEntryByUuid(identifiers["EntryDeletedInSourceBeforeChangedInTarget"]));
    QVERIFY(!db->containsDeletedObject(identifiers["EntryDeletedInSourceBeforeChangedInTarget"]));
    // newer deletion in source forces deletion
    QVERIFY(!mergedRootGroup->findEntryByUuid(identifiers["EntryDeletedInSourceAfterChangedInTarget"]));
    QVERIFY(db->containsDeletedObject(identifiers["EntryDeletedInSourceAfterChangedInTarget"]));
    // newer change in source privents deletion
    QVERIFY(mergedRootGroup->findEntryByUuid(identifiers["EntryDeletedInTargetBeforeChangedInSource"]));
    QVERIFY(!db->containsDeletedObject(identifiers["EntryDeletedInTargetBeforeChangedInSource"]));
    // newer deletion in target forces deletion
    QVERIFY(!mergedRootGroup->findEntryByUuid(identifiers["EntryDeletedInTargetAfterChangedInSource"]));
    QVERIFY(db->containsDeletedObject(identifiers["EntryDeletedInTargetAfterChangedInSource"]));
    // newer change in target prevents deletion
    QVERIFY(mergedRootGroup->findGroupByUuid(identifiers["GroupDeletedInSourceBeforeEntryUpdatedInTarget"]));
    QVERIFY(!db->containsDeletedObject(identifiers["GroupDeletedInSourceBeforeEntryUpdatedInTarget"]));
    QVERIFY(mergedRootGroup->findEntryByUuid(identifiers["EntryDeletedInSourceBeforeEntryUpdatedInTarget"]));
    QVERIFY(!db->containsDeletedObject(identifiers["EntryDeletedInSourceBeforeEntryUpdatedInTarget"]));
    // newer deletion in source forces deletion
    QVERIFY(!mergedRootGroup->findGroupByUuid(identifiers["GroupDeletedInSourceAfterEntryUpdatedInTarget"]));
    QVERIFY(db->containsDeletedObject(identifiers["GroupDeletedInSourceAfterEntryUpdatedInTarget"]));
    QVERIFY(!mergedRootGroup->findEntryByUuid(identifiers["EntryDeletedInSourceAfterEntryUpdatedInTarget"]));
    QVERIFY(db->containsDeletedObject(identifiers["EntryDeletedInSourceAfterEntryUpdatedInTarget"]));
    // newer change in source privents deletion
    QVERIFY(mergedRootGroup->findGroupByUuid(identifiers["GroupDeletedInTargetBeforeEntryUpdatedInSource"]));
    QVERIFY(!db->containsDeletedObject(identifiers["GroupDeletedInTargetBeforeEntryUpdatedInSource"]));
    QVERIFY(mergedRootGroup->findEntryByUuid(identifiers["EntryDeletedInTargetBeforeEntryUpdatedInSource"]));
    QVERIFY(!db->containsDeletedObject(identifiers["EntryDeletedInTargetBeforeEntryUpdatedInSource"]));
    // newer deletion in target forces deletion
    QVERIFY(!mergedRootGroup->findGroupByUuid(identifiers["GroupDeletedInTargetAfterEntryUpdatedInSource"]));
    QVERIFY(db->containsDeletedObject(identifiers["GroupDeletedInTargetAfterEntryUpdatedInSource"]));
    QVERIFY(!mergedRootGroup->findEntryByUuid(identifiers["EntryDeletedInTargetAfterEntryUpdatedInSource"]));
    QVERIFY(db->containsDeletedObject(identifiers["EntryDeletedInTargetAfterEntryUpdatedInSource"]));
}

void TestMerge::assertDeletionLocalOnly(Database* db, const QMap<QString, QUuid>& identifiers)
{
    QPointer<Group> mergedRootGroup = db->rootGroup();

    QVERIFY(mergedRootGroup->findEntryByUuid(identifiers["EntryDeletedInSourceBeforeChangedInTarget"]));
    QVERIFY(!db->containsDeletedObject(identifiers["EntryDeletedInSourceBeforeChangedInTarget"]));

    QVERIFY(mergedRootGroup->findEntryByUuid(identifiers["EntryDeletedInSourceAfterChangedInTarget"]));
    QVERIFY(!db->containsDeletedObject(identifiers["EntryDeletedInSourceAfterChangedInTarget"]));

    // Uuids in db and deletedObjects is intended according to KeePass #1752
    QVERIFY(mergedRootGroup->findEntryByUuid(identifiers["EntryDeletedInTargetBeforeChangedInSource"]));
    QVERIFY(db->containsDeletedObject(identifiers["EntryDeletedInTargetBeforeChangedInSource"]));

    QVERIFY(mergedRootGroup->findEntryByUuid(identifiers["EntryDeletedInTargetAfterChangedInSource"]));
    QVERIFY(db->containsDeletedObject(identifiers["EntryDeletedInTargetAfterChangedInSource"]));

    QVERIFY(mergedRootGroup->findGroupByUuid(identifiers["GroupDeletedInSourceBeforeEntryUpdatedInTarget"]));
    QVERIFY(!db->containsDeletedObject(identifiers["GroupDeletedInSourceBeforeEntryUpdatedInTarget"]));
    QVERIFY(mergedRootGroup->findEntryByUuid(identifiers["EntryDeletedInSourceBeforeEntryUpdatedInTarget"]));
    QVERIFY(!db->containsDeletedObject(identifiers["EntryDeletedInSourceBeforeEntryUpdatedInTarget"]));

    QVERIFY(mergedRootGroup->findGroupByUuid(identifiers["GroupDeletedInSourceAfterEntryUpdatedInTarget"]));
    QVERIFY(!db->containsDeletedObject(identifiers["GroupDeletedInSourceAfterEntryUpdatedInTarget"]));
    QVERIFY(mergedRootGroup->findEntryByUuid(identifiers["EntryDeletedInSourceAfterEntryUpdatedInTarget"]));
    QVERIFY(!db->containsDeletedObject(identifiers["EntryDeletedInSourceAfterEntryUpdatedInTarget"]));

    QVERIFY(mergedRootGroup->findGroupByUuid(identifiers["GroupDeletedInTargetBeforeEntryUpdatedInSource"]));
    QVERIFY(db->containsDeletedObject(identifiers["GroupDeletedInTargetBeforeEntryUpdatedInSource"]));
    QVERIFY(mergedRootGroup->findEntryByUuid(identifiers["EntryDeletedInTargetBeforeEntryUpdatedInSource"]));
    QVERIFY(db->containsDeletedObject(identifiers["EntryDeletedInTargetBeforeEntryUpdatedInSource"]));

    QVERIFY(mergedRootGroup->findGroupByUuid(identifiers["GroupDeletedInTargetAfterEntryUpdatedInSource"]));
    QVERIFY(db->containsDeletedObject(identifiers["GroupDeletedInTargetAfterEntryUpdatedInSource"]));
    QVERIFY(mergedRootGroup->findEntryByUuid(identifiers["EntryDeletedInTargetAfterEntryUpdatedInSource"]));
    QVERIFY(db->containsDeletedObject(identifiers["EntryDeletedInTargetAfterEntryUpdatedInSource"]));
}

void TestMerge::assertUpdateMergedEntry1(Entry* mergedEntry1, const QMap<const char*, QDateTime>& timestamps)
{
    QCOMPARE(mergedEntry1->historyItems().count(), 4);
    QCOMPARE(mergedEntry1->historyItems().at(0)->notes(), QString(""));
    QCOMPARE(mergedEntry1->historyItems().at(0)->timeInfo().lastModificationTime(), timestamps["initialTime"]);
    QCOMPARE(mergedEntry1->historyItems().at(1)->notes(), QString(""));
    QCOMPARE(mergedEntry1->historyItems().at(1)->timeInfo().lastModificationTime(),
             timestamps["oldestCommonHistoryTime"]);
    QCOMPARE(mergedEntry1->historyItems().at(2)->notes(), QString("1 Common"));
    QCOMPARE(mergedEntry1->historyItems().at(2)->timeInfo().lastModificationTime(),
             timestamps["newestCommonHistoryTime"]);
    QCOMPARE(mergedEntry1->historyItems().at(3)->notes(), QString("2 Source"));
    QCOMPARE(mergedEntry1->historyItems().at(3)->timeInfo().lastModificationTime(),
             timestamps["oldestDivergingHistoryTime"]);
    QCOMPARE(mergedEntry1->notes(), QString("3 Destination"));
    QCOMPARE(mergedEntry1->timeInfo().lastModificationTime(), timestamps["newestDivergingHistoryTime"]);
}

void TestMerge::assertUpdateReappliedEntry2(Entry* mergedEntry2, const QMap<const char*, QDateTime>& timestamps)
{
    QCOMPARE(mergedEntry2->historyItems().count(), 5);
    QCOMPARE(mergedEntry2->historyItems().at(0)->notes(), QString(""));
    QCOMPARE(mergedEntry2->historyItems().at(0)->timeInfo().lastModificationTime(), timestamps["initialTime"]);
    QCOMPARE(mergedEntry2->historyItems().at(1)->notes(), QString(""));
    QCOMPARE(mergedEntry2->historyItems().at(1)->timeInfo().lastModificationTime(),
             timestamps["oldestCommonHistoryTime"]);
    QCOMPARE(mergedEntry2->historyItems().at(2)->notes(), QString("1 Common"));
    QCOMPARE(mergedEntry2->historyItems().at(2)->timeInfo().lastModificationTime(),
             timestamps["newestCommonHistoryTime"]);
    QCOMPARE(mergedEntry2->historyItems().at(3)->notes(), QString("2 Destination"));
    QCOMPARE(mergedEntry2->historyItems().at(3)->timeInfo().lastModificationTime(),
             timestamps["oldestDivergingHistoryTime"]);
    QCOMPARE(mergedEntry2->historyItems().at(4)->notes(), QString("3 Source"));
    QCOMPARE(mergedEntry2->historyItems().at(4)->timeInfo().lastModificationTime(),
             timestamps["newestDivergingHistoryTime"]);
    QCOMPARE(mergedEntry2->notes(), QString("2 Destination"));
    QCOMPARE(mergedEntry2->timeInfo().lastModificationTime(), timestamps["mergeTime"]);
}

void TestMerge::assertUpdateReappliedEntry1(Entry* mergedEntry1, const QMap<const char*, QDateTime>& timestamps)
{
    QCOMPARE(mergedEntry1->historyItems().count(), 5);
    QCOMPARE(mergedEntry1->historyItems().at(0)->notes(), QString(""));
    QCOMPARE(mergedEntry1->historyItems().at(0)->timeInfo().lastModificationTime(), timestamps["initialTime"]);
    QCOMPARE(mergedEntry1->historyItems().at(1)->notes(), QString(""));
    QCOMPARE(mergedEntry1->historyItems().at(1)->timeInfo().lastModificationTime(),
             timestamps["oldestCommonHistoryTime"]);
    QCOMPARE(mergedEntry1->historyItems().at(2)->notes(), QString("1 Common"));
    QCOMPARE(mergedEntry1->historyItems().at(2)->timeInfo().lastModificationTime(),
             timestamps["newestCommonHistoryTime"]);
    QCOMPARE(mergedEntry1->historyItems().at(3)->notes(), QString("2 Source"));
    QCOMPARE(mergedEntry1->historyItems().at(3)->timeInfo().lastModificationTime(),
             timestamps["oldestDivergingHistoryTime"]);
    QCOMPARE(mergedEntry1->historyItems().at(4)->notes(), QString("3 Destination"));
    QCOMPARE(mergedEntry1->historyItems().at(4)->timeInfo().lastModificationTime(),
             timestamps["newestDivergingHistoryTime"]);
    QCOMPARE(mergedEntry1->notes(), QString("2 Source"));
    QCOMPARE(mergedEntry1->timeInfo().lastModificationTime(), timestamps["mergeTime"]);
}

void TestMerge::assertUpdateMergedEntry2(Entry* mergedEntry2, const QMap<const char*, QDateTime>& timestamps)
{
    QCOMPARE(mergedEntry2->historyItems().count(), 4);
    QCOMPARE(mergedEntry2->historyItems().at(0)->notes(), QString(""));
    QCOMPARE(mergedEntry2->historyItems().at(0)->timeInfo().lastModificationTime(), timestamps["initialTime"]);
    QCOMPARE(mergedEntry2->historyItems().at(1)->notes(), QString(""));
    QCOMPARE(mergedEntry2->historyItems().at(1)->timeInfo().lastModificationTime(),
             timestamps["oldestCommonHistoryTime"]);
    QCOMPARE(mergedEntry2->historyItems().at(2)->notes(), QString("1 Common"));
    QCOMPARE(mergedEntry2->historyItems().at(2)->timeInfo().lastModificationTime(),
             timestamps["newestCommonHistoryTime"]);
    QCOMPARE(mergedEntry2->historyItems().at(3)->notes(), QString("2 Destination"));
    QCOMPARE(mergedEntry2->historyItems().at(3)->timeInfo().lastModificationTime(),
             timestamps["oldestDivergingHistoryTime"]);
    QCOMPARE(mergedEntry2->notes(), QString("3 Source"));
    QCOMPARE(mergedEntry2->timeInfo().lastModificationTime(), timestamps["newestDivergingHistoryTime"]);
}

void TestMerge::testDeletionConflictEntry_Synchronized()
{
    testDeletionConflictTemplate(Group::Synchronize, &TestMerge::assertDeletionNewerOnly);
}

void TestMerge::testDeletionConflictEntry_KeepLocal()
{
    testDeletionConflictTemplate(Group::KeepLocal, &TestMerge::assertDeletionLocalOnly);
}

void TestMerge::testDeletionConflictEntry_KeepRemote()
{
    testDeletionConflictTemplate(Group::KeepRemote, &TestMerge::assertDeletionLocalOnly);
}

void TestMerge::testDeletionConflictEntry_KeepNewer()
{
    testDeletionConflictTemplate(Group::KeepNewer, &TestMerge::assertDeletionLocalOnly);
}

void TestMerge::testDeletionConflictEntry_Duplicate()
{
    testDeletionConflictTemplate(Group::Duplicate, &TestMerge::assertDeletionLocalOnly);
}

/**
 * Tests the KeepNewer mode concerning history.
 */
void TestMerge::testResolveConflictEntry_Synchronize()
{
    testResolveConflictTemplate(Group::Synchronize, [](Database* db, const QMap<const char*, QDateTime>& timestamps) {
        QPointer<Group> mergedRootGroup = db->rootGroup();
        QPointer<Group> mergedGroup1 = mergedRootGroup->children().at(0);
        TestMerge::assertUpdateMergedEntry1(mergedGroup1->entries().at(0), timestamps);
        TestMerge::assertUpdateMergedEntry2(mergedGroup1->entries().at(1), timestamps);
    });
}

/**
 * Tests the KeepExisting mode concerning history.
 */
void TestMerge::testResolveConflictEntry_KeepLocal()
{
    testResolveConflictTemplate(Group::KeepLocal, [](Database* db, const QMap<const char*, QDateTime>& timestamps) {
        QPointer<Group> mergedRootGroup = db->rootGroup();
        QPointer<Group> mergedGroup1 = mergedRootGroup->children().at(0);
        TestMerge::assertUpdateMergedEntry1(mergedGroup1->entries().at(0), timestamps);
        TestMerge::assertUpdateReappliedEntry2(mergedGroup1->entries().at(1), timestamps);
    });
}

void TestMerge::testResolveConflictEntry_KeepRemote()
{
    testResolveConflictTemplate(Group::KeepRemote, [](Database* db, const QMap<const char*, QDateTime>& timestamps) {
        QPointer<Group> mergedRootGroup = db->rootGroup();
        QPointer<Group> mergedGroup1 = mergedRootGroup->children().at(0);
        TestMerge::assertUpdateReappliedEntry1(mergedGroup1->entries().at(0), timestamps);
        TestMerge::assertUpdateMergedEntry2(mergedGroup1->entries().at(1), timestamps);
    });
}

void TestMerge::testResolveConflictEntry_KeepNewer()
{
    testResolveConflictTemplate(Group::KeepNewer, [](Database* db, const QMap<const char*, QDateTime>& timestamps) {
        QPointer<Group> mergedRootGroup = db->rootGroup();
        QPointer<Group> mergedGroup1 = mergedRootGroup->children().at(0);
        TestMerge::assertUpdateMergedEntry1(mergedGroup1->entries().at(0), timestamps);
        TestMerge::assertUpdateMergedEntry2(mergedGroup1->entries().at(1), timestamps);
    });
}

/**
 * The location of an entry should be updated in the
 * destination database.
 */
void TestMerge::testMoveEntry()
{
    QScopedPointer<Database> dbDestination(createTestDatabase());
    QScopedPointer<Database> dbSource(
        createTestDatabaseStructureClone(dbDestination.data(), Entry::CloneNoFlags, Group::CloneIncludeEntries));

    QPointer<Entry> entrySourceInitial = dbSource->rootGroup()->findEntryByPath("entry1");
    QVERIFY(entrySourceInitial != nullptr);

    QPointer<Group> groupSourceInitial = dbSource->rootGroup()->findChildByName("group2");
    QVERIFY(groupSourceInitial != nullptr);

    // Make sure the two changes have a different timestamp.
    m_clock->advanceSecond(1);

    entrySourceInitial->setGroup(groupSourceInitial);
    QCOMPARE(entrySourceInitial->group()->name(), QString("group2"));

    m_clock->advanceSecond(1);

    Merger merger(dbSource.data(), dbDestination.data());
    merger.merge();

    QPointer<Entry> entryDestinationMerged = dbDestination->rootGroup()->findEntryByPath("entry1");
    QVERIFY(entryDestinationMerged != nullptr);
    QCOMPARE(entryDestinationMerged->group()->name(), QString("group2"));
    QCOMPARE(dbDestination->rootGroup()->entriesRecursive().size(), 2);
}

/**
 * The location of an entry should be updated in the
 * destination database, but changes from the destination
 * database should be preserved.
 */
void TestMerge::testMoveEntryPreserveChanges()
{
    QScopedPointer<Database> dbDestination(createTestDatabase());
    QScopedPointer<Database> dbSource(
        createTestDatabaseStructureClone(dbDestination.data(), Entry::CloneNoFlags, Group::CloneIncludeEntries));

    QPointer<Entry> entrySourceInitial = dbSource->rootGroup()->findEntryByPath("entry1");
    QVERIFY(entrySourceInitial != nullptr);

    QPointer<Group> group2Source = dbSource->rootGroup()->findChildByName("group2");
    QVERIFY(group2Source != nullptr);

    m_clock->advanceSecond(1);

    entrySourceInitial->setGroup(group2Source);
    QCOMPARE(entrySourceInitial->group()->name(), QString("group2"));

    QPointer<Entry> entryDestinationInitial = dbDestination->rootGroup()->findEntryByPath("entry1");
    QVERIFY(entryDestinationInitial != nullptr);

    m_clock->advanceSecond(1);

    entryDestinationInitial->beginUpdate();
    entryDestinationInitial->setPassword("password");
    entryDestinationInitial->endUpdate();

    m_clock->advanceSecond(1);

    Merger merger(dbSource.data(), dbDestination.data());
    merger.merge();

    QPointer<Entry> entryDestinationMerged = dbDestination->rootGroup()->findEntryByPath("entry1");
    QVERIFY(entryDestinationMerged != nullptr);
    QCOMPARE(entryDestinationMerged->group()->name(), QString("group2"));
    QCOMPARE(dbDestination->rootGroup()->entriesRecursive().size(), 2);
    QCOMPARE(entryDestinationMerged->password(), QString("password"));
}

void TestMerge::testCreateNewGroups()
{
    QScopedPointer<Database> dbDestination(createTestDatabase());
    QScopedPointer<Database> dbSource(
        createTestDatabaseStructureClone(dbDestination.data(), Entry::CloneNoFlags, Group::CloneIncludeEntries));

    m_clock->advanceSecond(1);

    Group* groupSourceCreated = new Group();
    groupSourceCreated->setName("group3");
    groupSourceCreated->setUuid(QUuid::createUuid());
    groupSourceCreated->setParent(dbSource->rootGroup());

    m_clock->advanceSecond(1);

    Merger merger(dbSource.data(), dbDestination.data());
    merger.merge();

    QPointer<Group> groupDestinationMerged = dbDestination->rootGroup()->findChildByName("group3");
    QVERIFY(groupDestinationMerged != nullptr);
    QCOMPARE(groupDestinationMerged->name(), QString("group3"));
}

void TestMerge::testMoveEntryIntoNewGroup()
{
    QScopedPointer<Database> dbDestination(createTestDatabase());
    QScopedPointer<Database> dbSource(
        createTestDatabaseStructureClone(dbDestination.data(), Entry::CloneNoFlags, Group::CloneIncludeEntries));

    m_clock->advanceSecond(1);

    Group* groupSourceCreated = new Group();
    groupSourceCreated->setName("group3");
    groupSourceCreated->setUuid(QUuid::createUuid());
    groupSourceCreated->setParent(dbSource->rootGroup());

    QPointer<Entry> entrySourceMoved = dbSource->rootGroup()->findEntryByPath("entry1");
    entrySourceMoved->setGroup(groupSourceCreated);

    m_clock->advanceSecond(1);

    Merger merger(dbSource.data(), dbDestination.data());
    merger.merge();

    QCOMPARE(dbDestination->rootGroup()->entriesRecursive().size(), 2);

    QPointer<Group> groupDestinationMerged = dbDestination->rootGroup()->findChildByName("group3");
    QVERIFY(groupDestinationMerged != nullptr);
    QCOMPARE(groupDestinationMerged->name(), QString("group3"));
    QCOMPARE(groupDestinationMerged->entries().size(), 1);

    QPointer<Entry> entryDestinationMerged = dbDestination->rootGroup()->findEntryByPath("entry1");
    QVERIFY(entryDestinationMerged != nullptr);
    QCOMPARE(entryDestinationMerged->group()->name(), QString("group3"));
}

/**
 * Even though the entries' locations are no longer
 * the same, we will keep associating them.
 */
void TestMerge::testUpdateEntryDifferentLocation()
{
    QScopedPointer<Database> dbDestination(createTestDatabase());
    QScopedPointer<Database> dbSource(
        createTestDatabaseStructureClone(dbDestination.data(), Entry::CloneNoFlags, Group::CloneIncludeEntries));

    Group* groupDestinationCreated = new Group();
    groupDestinationCreated->setName("group3");
    groupDestinationCreated->setUuid(QUuid::createUuid());
    groupDestinationCreated->setParent(dbDestination->rootGroup());

    m_clock->advanceSecond(1);

    QPointer<Entry> entryDestinationMoved = dbDestination->rootGroup()->findEntryByPath("entry1");
    QVERIFY(entryDestinationMoved != nullptr);
    entryDestinationMoved->setGroup(groupDestinationCreated);
    QUuid uuidBeforeSyncing = entryDestinationMoved->uuid();
    QDateTime destinationLocationChanged = entryDestinationMoved->timeInfo().locationChanged();

    // Change the entry in the source db.
    m_clock->advanceSecond(1);

    QPointer<Entry> entrySourceMoved = dbSource->rootGroup()->findEntryByPath("entry1");
    QVERIFY(entrySourceMoved != nullptr);
    entrySourceMoved->beginUpdate();
    entrySourceMoved->setUsername("username");
    entrySourceMoved->endUpdate();
    QDateTime sourceLocationChanged = entrySourceMoved->timeInfo().locationChanged();

    QVERIFY(destinationLocationChanged > sourceLocationChanged);

    m_clock->advanceSecond(1);

    Merger merger(dbSource.data(), dbDestination.data());
    merger.merge();

    QCOMPARE(dbDestination->rootGroup()->entriesRecursive().size(), 2);

    QPointer<Entry> entryDestinationMerged = dbDestination->rootGroup()->findEntryByPath("entry1");
    QVERIFY(entryDestinationMerged != nullptr);
    QVERIFY(entryDestinationMerged->group() != nullptr);
    QCOMPARE(entryDestinationMerged->username(), QString("username"));
    QCOMPARE(entryDestinationMerged->group()->name(), QString("group3"));
    QCOMPARE(uuidBeforeSyncing, entryDestinationMerged->uuid());
    // default merge strategie is KeepNewer - therefore the older location is used!
    QCOMPARE(entryDestinationMerged->timeInfo().locationChanged(), sourceLocationChanged);
}

/**
 * Groups should be updated using the uuids.
 */
void TestMerge::testUpdateGroup()
{
    QScopedPointer<Database> dbDestination(createTestDatabase());
    QScopedPointer<Database> dbSource(
        createTestDatabaseStructureClone(dbDestination.data(), Entry::CloneNoFlags, Group::CloneIncludeEntries));

    m_clock->advanceSecond(1);

    QPointer<Group> groupSourceInitial = dbSource->rootGroup()->findChildByName("group2");
    groupSourceInitial->setName("group2 renamed");
    groupSourceInitial->setNotes("updated notes");
    QUuid customIconId = QUuid::createUuid();
    dbSource->metadata()->addCustomIcon(customIconId, QString("custom icon").toLocal8Bit());
    groupSourceInitial->setIcon(customIconId);

    QPointer<Entry> entrySourceInitial = dbSource->rootGroup()->findEntryByPath("entry1");
    QVERIFY(entrySourceInitial != nullptr);
    entrySourceInitial->setGroup(groupSourceInitial);
    entrySourceInitial->setTitle("entry1 renamed");
    QUuid uuidBeforeSyncing = entrySourceInitial->uuid();

    m_clock->advanceSecond(1);

    Merger merger(dbSource.data(), dbDestination.data());
    merger.merge();

    QCOMPARE(dbDestination->rootGroup()->entriesRecursive().size(), 2);

    QPointer<Entry> entryDestinationMerged = dbDestination->rootGroup()->findEntryByPath("entry1 renamed");
    QVERIFY(entryDestinationMerged != nullptr);
    QVERIFY(entryDestinationMerged->group() != nullptr);
    QCOMPARE(entryDestinationMerged->group()->name(), QString("group2 renamed"));
    QCOMPARE(uuidBeforeSyncing, entryDestinationMerged->uuid());

    QPointer<Group> groupMerged = dbDestination->rootGroup()->findChildByName("group2 renamed");
    QCOMPARE(groupMerged->notes(), QString("updated notes"));
    QCOMPARE(groupMerged->iconUuid(), customIconId);
}

void TestMerge::testUpdateGroupLocation()
{
    QScopedPointer<Database> dbDestination(createTestDatabase());
    Group* group3DestinationCreated = new Group();
    QUuid group3Uuid = QUuid::createUuid();
    group3DestinationCreated->setUuid(group3Uuid);
    group3DestinationCreated->setName("group3");
    group3DestinationCreated->setParent(dbDestination->rootGroup()->findChildByName("group1"));

    QScopedPointer<Database> dbSource(
        createTestDatabaseStructureClone(dbDestination.data(), Entry::CloneNoFlags, Group::CloneIncludeEntries));

    // Sanity check
    QPointer<Group> group3SourceInitial = dbSource->rootGroup()->findGroupByUuid(group3Uuid);
    QVERIFY(group3DestinationCreated != nullptr);

    QDateTime initialLocationChanged = group3SourceInitial->timeInfo().locationChanged();

    m_clock->advanceSecond(1);

    QPointer<Group> group3SourceMoved = dbSource->rootGroup()->findGroupByUuid(group3Uuid);
    QVERIFY(group3SourceMoved != nullptr);
    group3SourceMoved->setParent(dbSource->rootGroup()->findChildByName("group2"));

    QDateTime movedLocaltionChanged = group3SourceMoved->timeInfo().locationChanged();
    QVERIFY(initialLocationChanged < movedLocaltionChanged);

    m_clock->advanceSecond(1);

    Merger merger1(dbSource.data(), dbDestination.data());
    merger1.merge();

    QPointer<Group> group3DestinationMerged1 = dbDestination->rootGroup()->findGroupByUuid(group3Uuid);
    QVERIFY(group3DestinationMerged1 != nullptr);
    QCOMPARE(group3DestinationMerged1->parent(), dbDestination->rootGroup()->findChildByName("group2"));
    QCOMPARE(group3DestinationMerged1->timeInfo().locationChanged(), movedLocaltionChanged);

    m_clock->advanceSecond(1);

    Merger merger2(dbSource.data(), dbDestination.data());
    merger2.merge();

    QPointer<Group> group3DestinationMerged2 = dbDestination->rootGroup()->findGroupByUuid(group3Uuid);
    QVERIFY(group3DestinationMerged2 != nullptr);
    QCOMPARE(group3DestinationMerged2->parent(), dbDestination->rootGroup()->findChildByName("group2"));
    QCOMPARE(group3DestinationMerged1->timeInfo().locationChanged(), movedLocaltionChanged);
}

/**
 * The first merge should create new entries, the
 * second should only sync them, since they have
 * been created with the same UUIDs.
 */
void TestMerge::testMergeAndSync()
{
    QScopedPointer<Database> dbDestination(new Database());
    QScopedPointer<Database> dbSource(createTestDatabase());

    QCOMPARE(dbDestination->rootGroup()->entriesRecursive().size(), 0);

    m_clock->advanceSecond(1);

    Merger merger1(dbSource.data(), dbDestination.data());
    merger1.merge();

    QCOMPARE(dbDestination->rootGroup()->entriesRecursive().size(), 2);

    m_clock->advanceSecond(1);

    Merger merger2(dbSource.data(), dbDestination.data());
    merger2.merge();

    // Still only 2 entries, since now we detect which are already present.
    QCOMPARE(dbDestination->rootGroup()->entriesRecursive().size(), 2);
}

/**
 * Custom icons should be brought over when merging.
 */
void TestMerge::testMergeCustomIcons()
{
    QScopedPointer<Database> dbDestination(new Database());
    QScopedPointer<Database> dbSource(createTestDatabase());

    m_clock->advanceSecond(1);

    QUuid customIconId = QUuid::createUuid();

    dbSource->metadata()->addCustomIcon(customIconId, QString("custom icon").toLocal8Bit());
    // Sanity check.
    QVERIFY(dbSource->metadata()->hasCustomIcon(customIconId));

    m_clock->advanceSecond(1);

    Merger merger(dbSource.data(), dbDestination.data());
    merger.merge();

    QVERIFY(dbDestination->metadata()->hasCustomIcon(customIconId));
}

/**
 * No duplicate icons should be created
 */
void TestMerge::testMergeDuplicateCustomIcons()
{
    QScopedPointer<Database> dbDestination(new Database());
    QScopedPointer<Database> dbSource(createTestDatabase());

    m_clock->advanceSecond(1);

    QUuid customIconId = QUuid::createUuid();

    QByteArray customIcon1 = QString("custom icon 1").toLocal8Bit();
    QByteArray customIcon2 = QString("custom icon 2").toLocal8Bit();

    dbSource->metadata()->addCustomIcon(customIconId, customIcon1);
    dbDestination->metadata()->addCustomIcon(customIconId, customIcon2);
    // Sanity check.
    QVERIFY(dbSource->metadata()->hasCustomIcon(customIconId));
    QVERIFY(dbDestination->metadata()->hasCustomIcon(customIconId));

    m_clock->advanceSecond(1);

    Merger merger(dbSource.data(), dbDestination.data());
    merger.merge();

    QVERIFY(dbDestination->metadata()->hasCustomIcon(customIconId));
    QCOMPARE(dbDestination->metadata()->customIconsOrder().count(), 1);
    QCOMPARE(dbDestination->metadata()->customIcon(customIconId), customIcon2);
}

void TestMerge::testMetadata()
{
    QSKIP("Sophisticated merging for Metadata not implemented");
    // TODO HNH: I think a merge of recycle bins would be nice since duplicating them
    //           is not really a good solution - the one to use as final recycle bin
    //           is determined by the merge method - if only one has a bin, this one
    //           will be used - exception is the target has no recycle bin activated
}

void TestMerge::testCustomData()
{
    QScopedPointer<Database> dbDestination(new Database());
    QScopedPointer<Database> dbSource(createTestDatabase());
    QScopedPointer<Database> dbDestination2(new Database());
    QScopedPointer<Database> dbSource2(createTestDatabase());

    m_clock->advanceSecond(1);

    dbDestination->metadata()->customData()->set("toBeDeleted", "value");
    dbDestination->metadata()->customData()->set("key3", "oldValue");

    dbSource2->metadata()->customData()->set("key1", "value1");
    dbSource2->metadata()->customData()->set("key2", "value2");
    dbSource2->metadata()->customData()->set("key3", "newValue");
    dbSource2->metadata()->customData()->set("Browser", "n'8=3W@L^6d->d.]St_>]");

    m_clock->advanceSecond(1);

    dbSource->metadata()->customData()->set("key1", "value1");
    dbSource->metadata()->customData()->set("key2", "value2");
    dbSource->metadata()->customData()->set("key3", "newValue");
    dbSource->metadata()->customData()->set("Browser", "n'8=3W@L^6d->d.]St_>]");

    dbDestination2->metadata()->customData()->set("notToBeDeleted", "value");
    dbDestination2->metadata()->customData()->set("key3", "oldValue");

    // Sanity check.
    QVERIFY(!dbSource->metadata()->customData()->isEmpty());
    QVERIFY(!dbSource2->metadata()->customData()->isEmpty());

    m_clock->advanceSecond(1);

    Merger merger(dbSource.data(), dbDestination.data());
    QStringList changes = merger.merge();

    QVERIFY(!changes.isEmpty());

    // Source is newer, data should be merged
    QVERIFY(!dbDestination->metadata()->customData()->isEmpty());
    QVERIFY(dbDestination->metadata()->customData()->contains("key1"));
    QVERIFY(dbDestination->metadata()->customData()->contains("key2"));
    QVERIFY(dbDestination->metadata()->customData()->contains("Browser"));
    QVERIFY(!dbDestination->metadata()->customData()->contains("toBeDeleted"));
    QCOMPARE(dbDestination->metadata()->customData()->value("key1"), QString("value1"));
    QCOMPARE(dbDestination->metadata()->customData()->value("key2"), QString("value2"));
    QCOMPARE(dbDestination->metadata()->customData()->value("Browser"), QString("n'8=3W@L^6d->d.]St_>]"));
    QCOMPARE(dbDestination->metadata()->customData()->value("key3"),
             QString("newValue")); // Old value should be replaced

    // Merging again should not do anything if the values are the same.
    m_clock->advanceSecond(1);
    dbSource->metadata()->customData()->set("key3", "oldValue");
    dbSource->metadata()->customData()->set("key3", "newValue");
    Merger merger2(dbSource.data(), dbDestination.data());
    QStringList changes2 = merger2.merge();
    QVERIFY(changes2.isEmpty());

    Merger merger3(dbSource2.data(), dbDestination2.data());
    merger3.merge();

    // Target is newer, no data is merged
    QVERIFY(!dbDestination2->metadata()->customData()->isEmpty());
    QVERIFY(!dbDestination2->metadata()->customData()->contains("key1"));
    QVERIFY(!dbDestination2->metadata()->customData()->contains("key2"));
    QVERIFY(!dbDestination2->metadata()->customData()->contains("Browser"));
    QVERIFY(dbDestination2->metadata()->customData()->contains("notToBeDeleted"));
    QCOMPARE(dbDestination2->metadata()->customData()->value("key3"),
             QString("oldValue")); // Old value should not be replaced
}

void TestMerge::testDeletedEntry()
{
    QScopedPointer<Database> dbDestination(createTestDatabase());
    QScopedPointer<Database> dbSource(
        createTestDatabaseStructureClone(dbDestination.data(), Entry::CloneNoFlags, Group::CloneIncludeEntries));

    m_clock->advanceSecond(1);

    QPointer<Entry> entry1SourceInitial = dbSource->rootGroup()->findEntryByPath("entry1");
    QVERIFY(entry1SourceInitial != nullptr);
    QUuid entry1Uuid = entry1SourceInitial->uuid();
    delete entry1SourceInitial;
    QVERIFY(dbSource->containsDeletedObject(entry1Uuid));

    m_clock->advanceSecond(1);

    QPointer<Entry> entry2DestinationInitial = dbDestination->rootGroup()->findEntryByPath("entry2");
    QVERIFY(entry2DestinationInitial != nullptr);
    QUuid entry2Uuid = entry2DestinationInitial->uuid();
    delete entry2DestinationInitial;
    QVERIFY(dbDestination->containsDeletedObject(entry2Uuid));

    m_clock->advanceSecond(1);

    Merger merger(dbSource.data(), dbDestination.data());
    merger.merge();

    QPointer<Entry> entry1DestinationMerged = dbDestination->rootGroup()->findEntryByPath("entry1");
    QVERIFY(entry1DestinationMerged);
    QVERIFY(!dbDestination->containsDeletedObject(entry1Uuid));
    QPointer<Entry> entry2DestinationMerged = dbDestination->rootGroup()->findEntryByPath("entry2");
    QVERIFY(entry2DestinationMerged);
    // Uuid in db and deletedObjects is intended according to KeePass #1752
    QVERIFY(dbDestination->containsDeletedObject(entry2Uuid));

    QCOMPARE(dbDestination->rootGroup()->entriesRecursive().size(), 2);
}

void TestMerge::testDeletedGroup()
{
    QScopedPointer<Database> dbDestination(createTestDatabase());
    QScopedPointer<Database> dbSource(
        createTestDatabaseStructureClone(dbDestination.data(), Entry::CloneNoFlags, Group::CloneIncludeEntries));

    m_clock->advanceSecond(1);

    QPointer<Group> group2DestinationInitial = dbDestination->rootGroup()->findChildByName("group2");
    QVERIFY(group2DestinationInitial != nullptr);
    Entry* entry3DestinationCreated = new Entry();
    entry3DestinationCreated->beginUpdate();
    entry3DestinationCreated->setUuid(QUuid::createUuid());
    entry3DestinationCreated->setGroup(group2DestinationInitial);
    entry3DestinationCreated->setTitle("entry3");
    entry3DestinationCreated->endUpdate();

    m_clock->advanceSecond(1);

    QPointer<Group> group1SourceInitial = dbSource->rootGroup()->findChildByName("group1");
    QVERIFY(group1SourceInitial != nullptr);
    QPointer<Entry> entry1SourceInitial = dbSource->rootGroup()->findEntryByPath("entry1");
    QVERIFY(entry1SourceInitial != nullptr);
    QPointer<Entry> entry2SourceInitial = dbSource->rootGroup()->findEntryByPath("entry2");
    QVERIFY(entry2SourceInitial != nullptr);
    QUuid group1Uuid = group1SourceInitial->uuid();
    QUuid entry1Uuid = entry1SourceInitial->uuid();
    QUuid entry2Uuid = entry2SourceInitial->uuid();
    delete group1SourceInitial;
    QVERIFY(dbSource->containsDeletedObject(group1Uuid));
    QVERIFY(dbSource->containsDeletedObject(entry1Uuid));
    QVERIFY(dbSource->containsDeletedObject(entry2Uuid));

    m_clock->advanceSecond(1);

    QPointer<Group> group2SourceInitial = dbSource->rootGroup()->findChildByName("group2");
    QVERIFY(group2SourceInitial != nullptr);
    QUuid group2Uuid = group2SourceInitial->uuid();
    delete group2SourceInitial;
    QVERIFY(dbSource->containsDeletedObject(group2Uuid));

    m_clock->advanceSecond(1);

    Merger merger(dbSource.data(), dbDestination.data());
    merger.merge();

    QVERIFY(!dbDestination->containsDeletedObject(group1Uuid));
    QVERIFY(!dbDestination->containsDeletedObject(entry1Uuid));
    QVERIFY(!dbDestination->containsDeletedObject(entry2Uuid));
    QVERIFY(!dbDestination->containsDeletedObject(group2Uuid));

    QPointer<Entry> entry1DestinationMerged = dbDestination->rootGroup()->findEntryByPath("entry1");
    QVERIFY(entry1DestinationMerged);
    QPointer<Entry> entry2DestinationMerged = dbDestination->rootGroup()->findEntryByPath("entry2");
    QVERIFY(entry2DestinationMerged);
    QPointer<Entry> entry3DestinationMerged = dbDestination->rootGroup()->findEntryByPath("entry3");
    QVERIFY(entry3DestinationMerged);
    QPointer<Group> group1DestinationMerged = dbDestination->rootGroup()->findChildByName("group1");
    QVERIFY(group1DestinationMerged);
    QPointer<Group> group2DestinationMerged = dbDestination->rootGroup()->findChildByName("group2");
    QVERIFY(group2DestinationMerged);

    QCOMPARE(dbDestination->rootGroup()->entriesRecursive().size(), 3);
}

void TestMerge::testDeletedRevertedEntry()
{
    QScopedPointer<Database> dbDestination(createTestDatabase());
    QScopedPointer<Database> dbSource(
        createTestDatabaseStructureClone(dbDestination.data(), Entry::CloneNoFlags, Group::CloneIncludeEntries));

    m_clock->advanceSecond(1);

    QPointer<Entry> entry1DestinationInitial = dbDestination->rootGroup()->findEntryByPath("entry1");
    QVERIFY(entry1DestinationInitial != nullptr);
    QUuid entry1Uuid = entry1DestinationInitial->uuid();
    delete entry1DestinationInitial;
    QVERIFY(dbDestination->containsDeletedObject(entry1Uuid));

    m_clock->advanceSecond(1);

    QPointer<Entry> entry2SourceInitial = dbSource->rootGroup()->findEntryByPath("entry2");
    QVERIFY(entry2SourceInitial != nullptr);
    QUuid entry2Uuid = entry2SourceInitial->uuid();
    delete entry2SourceInitial;
    QVERIFY(dbSource->containsDeletedObject(entry2Uuid));

    m_clock->advanceSecond(1);

    QPointer<Entry> entry1SourceInitial = dbSource->rootGroup()->findEntryByPath("entry1");
    QVERIFY(entry1SourceInitial != nullptr);
    entry1SourceInitial->setNotes("Updated");

    QPointer<Entry> entry2DestinationInitial = dbDestination->rootGroup()->findEntryByPath("entry2");
    QVERIFY(entry2DestinationInitial != nullptr);
    entry2DestinationInitial->setNotes("Updated");

    Merger merger(dbSource.data(), dbDestination.data());
    merger.merge();

    // Uuid in db and deletedObjects is intended according to KeePass #1752
    QVERIFY(dbDestination->containsDeletedObject(entry1Uuid));
    QVERIFY(!dbDestination->containsDeletedObject(entry2Uuid));

    QPointer<Entry> entry1DestinationMerged = dbDestination->rootGroup()->findEntryByPath("entry1");
    QVERIFY(entry1DestinationMerged);
    QVERIFY(entry1DestinationMerged->notes() == "Updated");
    QPointer<Entry> entry2DestinationMerged = dbDestination->rootGroup()->findEntryByPath("entry2");
    QVERIFY(entry2DestinationMerged);
    QVERIFY(entry2DestinationMerged->notes() == "Updated");
}

void TestMerge::testDeletedRevertedGroup()
{
    QScopedPointer<Database> dbDestination(createTestDatabase());
    QScopedPointer<Database> dbSource(
        createTestDatabaseStructureClone(dbDestination.data(), Entry::CloneNoFlags, Group::CloneIncludeEntries));

    m_clock->advanceSecond(1);

    QPointer<Group> group2SourceInitial = dbSource->rootGroup()->findChildByName("group2");
    QVERIFY(group2SourceInitial);
    QUuid group2Uuid = group2SourceInitial->uuid();
    delete group2SourceInitial;
    QVERIFY(dbSource->containsDeletedObject(group2Uuid));

    m_clock->advanceSecond(1);

    QPointer<Group> group1DestinationInitial = dbDestination->rootGroup()->findChildByName("group1");
    QVERIFY(group1DestinationInitial);
    QUuid group1Uuid = group1DestinationInitial->uuid();
    delete group1DestinationInitial;
    QVERIFY(dbDestination->containsDeletedObject(group1Uuid));

    m_clock->advanceSecond(1);

    QPointer<Group> group1SourceInitial = dbSource->rootGroup()->findChildByName("group1");
    QVERIFY(group1SourceInitial);
    group1SourceInitial->setNotes("Updated");

    m_clock->advanceSecond(1);

    QPointer<Group> group2DestinationInitial = dbDestination->rootGroup()->findChildByName("group2");
    QVERIFY(group2DestinationInitial);
    group2DestinationInitial->setNotes("Updated");

    m_clock->advanceSecond(1);

    Merger merger(dbSource.data(), dbDestination.data());
    merger.merge();

    // Uuid in db and deletedObjects is intended according to KeePass #1752
    QVERIFY(dbDestination->containsDeletedObject(group1Uuid));
    QVERIFY(!dbDestination->containsDeletedObject(group2Uuid));

    QPointer<Group> group1DestinationMerged = dbDestination->rootGroup()->findChildByName("group1");
    QVERIFY(group1DestinationMerged);
    QVERIFY(group1DestinationMerged->notes() == "Updated");
    QPointer<Group> group2DestinationMerged = dbDestination->rootGroup()->findChildByName("group2");
    QVERIFY(group2DestinationMerged);
    QVERIFY(group2DestinationMerged->notes() == "Updated");
}

/**
 * If the group is updated in the source database, and the
 * destination database after, the group should remain the
 * same.
 */
void TestMerge::testResolveGroupConflictOlder()
{
    QScopedPointer<Database> dbDestination(createTestDatabase());
    QScopedPointer<Database> dbSource(
        createTestDatabaseStructureClone(dbDestination.data(), Entry::CloneNoFlags, Group::CloneIncludeEntries));

    // sanity check
    QPointer<Group> groupSourceInitial = dbSource->rootGroup()->findChildByName("group1");
    QVERIFY(groupSourceInitial != nullptr);

    // Make sure the two changes have a different timestamp.
    m_clock->advanceSecond(1);

    groupSourceInitial->setName("group1 updated in source");

    // Make sure the two changes have a different timestamp.
    m_clock->advanceSecond(1);

    QPointer<Group> groupDestinationUpdated = dbDestination->rootGroup()->findChildByName("group1");
    groupDestinationUpdated->setName("group1 updated in destination");

    m_clock->advanceSecond(1);

    Merger merger(dbSource.data(), dbDestination.data());
    merger.merge();

    // sanity check
    QPointer<Group> groupDestinationMerged =
        dbDestination->rootGroup()->findChildByName("group1 updated in destination");
    QVERIFY(groupDestinationMerged != nullptr);
}

void TestMerge::testMergeNotModified()
{
    QScopedPointer<Database> dbDestination(createTestDatabase());
    QScopedPointer<Database> dbSource(
        createTestDatabaseStructureClone(dbDestination.data(), Entry::CloneNoFlags, Group::CloneIncludeEntries));

    QSignalSpy modifiedSignalSpy(dbDestination.data(), SIGNAL(databaseModified()));
    Merger merger(dbSource.data(), dbDestination.data());
    merger.merge();
    QTRY_VERIFY(modifiedSignalSpy.empty());
}

void TestMerge::testMergeModified()
{
    QScopedPointer<Database> dbDestination(createTestDatabase());
    QScopedPointer<Database> dbSource(
        createTestDatabaseStructureClone(dbDestination.data(), Entry::CloneNoFlags, Group::CloneIncludeEntries));

    QSignalSpy modifiedSignalSpy(dbDestination.data(), SIGNAL(databaseModified()));
    // Make sure the two changes have a different timestamp.
    QTest::qSleep(1);
    Entry* entry = dbSource->rootGroup()->findEntryByPath("entry1");
    entry->beginUpdate();
    entry->setTitle("new title");
    entry->endUpdate();

    Merger merger(dbSource.data(), dbDestination.data());
    merger.merge();
    QTRY_VERIFY(!modifiedSignalSpy.empty());
}

Database* TestMerge::createTestDatabase()
{
    Database* db = new Database();

    Group* group1 = new Group();
    group1->setName("group1");
    group1->setUuid(QUuid::createUuid());

    Group* group2 = new Group();
    group2->setName("group2");
    group2->setUuid(QUuid::createUuid());

    Entry* entry1 = new Entry();
    entry1->setUuid(QUuid::createUuid());
    Entry* entry2 = new Entry();
    entry2->setUuid(QUuid::createUuid());

    m_clock->advanceYear(1);

    // Give Entry 1 a history
    entry1->beginUpdate();
    entry1->setGroup(group1);
    entry1->setTitle("entry1");
    entry1->endUpdate();

    // Give Entry 2 a history
    entry2->beginUpdate();
    entry2->setGroup(group1);
    entry2->setTitle("entry2");
    entry2->endUpdate();

    group1->setParent(db->rootGroup());
    group2->setParent(db->rootGroup());

    return db;
}

Database* TestMerge::createTestDatabaseStructureClone(Database* source, int entryFlags, int groupFlags)
{
    Database* db = new Database();
    // the old root group is deleted by QObject::parent relationship
    db->setRootGroup(source->rootGroup()->clone(static_cast<Entry::CloneFlag>(entryFlags),
                                                static_cast<Group::CloneFlag>(groupFlags)));
    return db;
}
