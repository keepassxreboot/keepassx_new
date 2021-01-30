/*
 *  Copyright (C) 2018 Aetf <aetf@unlimitedcodeworks.xyz>
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

#include "Collection.h"

#include "fdosecrets/FdoSecretsPlugin.h"
#include "fdosecrets/FdoSecretsSettings.h"
#include "fdosecrets/objects/Item.h"
#include "fdosecrets/objects/Prompt.h"
#include "fdosecrets/objects/Service.h"
#include "fdosecrets/objects/Session.h"

#include "core/Config.h"
#include "core/Tools.h"
#include "gui/DatabaseTabWidget.h"
#include "gui/DatabaseWidget.h"

#include <QFileInfo>
#include <QRegularExpression>

namespace FdoSecrets
{
    Collection* Collection::Create(Service* parent, DatabaseWidget* backend)
    {
        return new Collection(parent, backend);
    }

    Collection::Collection(Service* parent, DatabaseWidget* backend)
        : DBusObject(parent)
        , m_backend(backend)
        , m_exposedGroup(nullptr)
    {
        // whenever the file path or the database object itself change, we do a full reload.
        connect(backend, &DatabaseWidget::databaseFilePathChanged, this, &Collection::reloadBackendOrDelete);
        connect(backend, &DatabaseWidget::databaseReplaced, this, &Collection::reloadBackendOrDelete);

        // also remember to clear/populate the database when lock state changes.
        connect(backend, &DatabaseWidget::databaseUnlocked, this, &Collection::onDatabaseLockChanged);
        connect(backend, &DatabaseWidget::databaseLocked, this, &Collection::onDatabaseLockChanged);

        // get notified whenever unlock db dialog finishes
        connect(parent, &Service::doneUnlockDatabaseInDialog, this, [this](bool accepted, DatabaseWidget* dbWidget) {
            if (!dbWidget || dbWidget != m_backend) {
                return;
            }
            emit doneUnlockCollection(accepted);
        });
    }

    bool Collection::reloadBackend()
    {
        Q_ASSERT(m_backend);

        // delete all items
        // this has to be done because the backend is actually still there, just we don't expose them
        // NOTE: Do NOT use a for loop, because Item::doDelete will remove itself from m_items.
        while (!m_items.isEmpty()) {
            m_items.first()->doDelete();
        }
        cleanupConnections();
        dbus().unregisterObject(this);

        // make sure we have updated copy of the filepath, which is used to identify the database.
        m_backendPath = m_backend->database()->canonicalFilePath();

        // register the object, handling potentially duplicated name
        if (!dbus().registerObject(this)) {
            return false;
        }

        // populate contents after expose on dbus, because items rely on parent's dbus object path
        if (!backendLocked()) {
            populateContents();
        } else {
            cleanupConnections();
        }

        emit collectionChanged();
        return true;
    }

    void Collection::reloadBackendOrDelete()
    {
        if (!reloadBackend()) {
            doDelete();
        }
    }

    DBusResult Collection::ensureBackend() const
    {
        if (!m_backend) {
            return DBusResult(QStringLiteral(DBUS_ERROR_SECRET_NO_SUCH_OBJECT));
        }
        return {};
    }

    DBusResult Collection::ensureUnlocked() const
    {
        if (backendLocked()) {
            return DBusResult(QStringLiteral(DBUS_ERROR_SECRET_IS_LOCKED));
        }
        return {};
    }

    DBusResult Collection::items(QList<Item*>& items) const
    {
        auto ret = ensureBackend();
        if (ret.err()) {
            return ret;
        }
        items = m_items;
        return {};
    }

    DBusResult Collection::label(QString& label) const
    {
        auto ret = ensureBackend();
        if (ret.err()) {
            return ret;
        }

        if (backendLocked()) {
            label = name();
        } else {
            label = m_backend->database()->metadata()->name();
        }
        return {};
    }

    DBusResult Collection::setLabel(const QString& label)
    {
        auto ret = ensureBackend();
        if (ret.err()) {
            return ret;
        }
        ret = ensureUnlocked();
        if (ret.err()) {
            return ret;
        }

        m_backend->database()->metadata()->setName(label);
        return {};
    }

    DBusResult Collection::locked(bool& locked) const
    {
        auto ret = ensureBackend();
        if (ret.err()) {
            return ret;
        }
        locked = backendLocked();
        return {};
    }

    DBusResult Collection::created(qulonglong& created) const
    {
        auto ret = ensureBackend();
        if (ret.err()) {
            return ret;
        }
        ret = ensureUnlocked();
        if (ret.err()) {
            return ret;
        }
        created = static_cast<qulonglong>(
            m_backend->database()->rootGroup()->timeInfo().creationTime().toMSecsSinceEpoch() / 1000);

        return {};
    }

    DBusResult Collection::modified(qulonglong& modified) const
    {
        auto ret = ensureBackend();
        if (ret.err()) {
            return ret;
        }
        ret = ensureUnlocked();
        if (ret.err()) {
            return ret;
        }
        // FIXME: there seems not to have a global modified time.
        // Use a more accurate time, considering all metadata, group, entry.
        modified = static_cast<qulonglong>(
            m_backend->database()->rootGroup()->timeInfo().lastModificationTime().toMSecsSinceEpoch() / 1000);
        return {};
    }

    DBusResult Collection::remove(PromptBase*& prompt)
    {
        auto ret = ensureBackend();
        if (ret.err()) {
            return ret;
        }

        // Delete means close database
        prompt = PromptBase::Create<DeleteCollectionPrompt>(service(), this);
        if (!prompt) {
            return QDBusError::InternalError;
        }
        if (backendLocked()) {
            // this won't raise a dialog, immediate execute
            ret = prompt->prompt({});
            if (ret.err()) {
                return ret;
            }
            prompt = nullptr;
        }
        // defer the close to the prompt
        return {};
    }

    DBusResult Collection::searchItems(const StringStringMap& attributes, QList<Item*>& items)
    {
        items.clear();

        auto ret = ensureBackend();
        if (ret.err()) {
            return ret;
        }
        ret = ensureUnlocked();
        if (ret.err()) {
            // searchItems should work, whether `this` is locked or not.
            // however, we can't search items the same way as in gnome-keying,
            // because there's no database at all when locked.
            return {};
        }

        // shortcut logic for Uuid/Path attributes, as they can uniquely identify an item.
        if (attributes.contains(ItemAttributes::UuidKey)) {
            auto uuid = QUuid::fromRfc4122(QByteArray::fromHex(attributes.value(ItemAttributes::UuidKey).toLatin1()));
            auto entry = m_exposedGroup->findEntryByUuid(uuid);
            if (entry) {
                items += m_entryToItem.value(entry);
            }
            return {};
        }

        if (attributes.contains(ItemAttributes::PathKey)) {
            auto path = attributes.value(ItemAttributes::PathKey);
            auto entry = m_exposedGroup->findEntryByPath(path);
            if (entry) {
                items += m_entryToItem.value(entry);
            }
            return {};
        }

        QList<EntrySearcher::SearchTerm> terms;
        for (auto it = attributes.constBegin(); it != attributes.constEnd(); ++it) {
            terms << attributeToTerm(it.key(), it.value());
        }

        const auto foundEntries = EntrySearcher(false, true).search(terms, m_exposedGroup);
        items.reserve(foundEntries.size());
        for (const auto& entry : foundEntries) {
            items << m_entryToItem.value(entry);
        }
        return {};
    }

    EntrySearcher::SearchTerm Collection::attributeToTerm(const QString& key, const QString& value)
    {
        static QMap<QString, EntrySearcher::Field> attrKeyToField{
            {EntryAttributes::TitleKey, EntrySearcher::Field::Title},
            {EntryAttributes::UserNameKey, EntrySearcher::Field::Username},
            {EntryAttributes::URLKey, EntrySearcher::Field::Url},
            {EntryAttributes::NotesKey, EntrySearcher::Field::Notes},
        };

        EntrySearcher::SearchTerm term{};
        term.field = attrKeyToField.value(key, EntrySearcher::Field::AttributeValue);
        term.word = key;
        term.exclude = false;

        const auto useWildcards = false;
        const auto exactMatch = true;
        const auto caseSensitive = true;
        term.regex = Tools::convertToRegex(QRegularExpression::escape(value), useWildcards, exactMatch, caseSensitive);

        return term;
    }

    DBusResult Collection::createItem(const QVariantMap& properties,
                                      const Secret& secret,
                                      bool replace,
                                      Item*& item,
                                      PromptBase*& prompt)
    {
        auto ret = ensureBackend();
        if (ret.err()) {
            return ret;
        }
        ret = ensureUnlocked();
        if (ret.err()) {
            return ret;
        }

        item = nullptr;
        QString itemPath;

        auto iterAttr = properties.find(QStringLiteral(DBUS_INTERFACE_SECRET_ITEM ".Attributes"));
        if (iterAttr != properties.end()) {
            // the actual value in iterAttr.value() is QDBusArgument, which represents a structure
            // and qt has no idea what this corresponds to.
            // we thus force a conversion to StringStringMap here. The conversion is registered in
            // DBusTypes.cpp
            auto attributes = iterAttr.value().value<StringStringMap>();

            itemPath = attributes.value(ItemAttributes::PathKey);

            // check existing item using attributes
            QList<Item*> existing;
            ret = searchItems(attributes, existing);
            if (ret.err()) {
                return ret;
            }
            if (!existing.isEmpty() && replace) {
                item = existing.front();
            }
        }

        prompt = PromptBase::Create<CreateItemPrompt>(service(), this, properties, secret, itemPath, item);
        if (!prompt) {
            return QDBusError::InternalError;
        }
        return {};
    }

    DBusResult Collection::setProperties(const QVariantMap& properties)
    {
        auto label = properties.value(QStringLiteral(DBUS_INTERFACE_SECRET_COLLECTION ".Label")).toString();

        auto ret = setLabel(label);
        if (ret.err()) {
            return ret;
        }

        return {};
    }

    const QSet<QString> Collection::aliases() const
    {
        return m_aliases;
    }

    DBusResult Collection::addAlias(QString alias)
    {
        auto ret = ensureBackend();
        if (ret.err()) {
            return ret;
        }

        alias = encodePath(alias);

        if (m_aliases.contains(alias)) {
            return {};
        }

        emit aliasAboutToAdd(alias);

        if (dbus().registerAlias(this, alias)) {
            m_aliases.insert(alias);
            emit aliasAdded(alias);
        } else {
            return QDBusError::InvalidObjectPath;
        }

        return {};
    }

    DBusResult Collection::removeAlias(QString alias)
    {
        auto ret = ensureBackend();
        if (ret.err()) {
            return ret;
        }

        alias = encodePath(alias);

        if (!m_aliases.contains(alias)) {
            return {};
        }

        dbus().unregisterAlias(alias);
        m_aliases.remove(alias);
        emit aliasRemoved(alias);

        return {};
    }

    QString Collection::name() const
    {
        if (m_backendPath.isEmpty()) {
            // This is a newly created db without saving to file,
            // but we have to give a name, which is used to register dbus path.
            // We use database name for this purpose. For simplicity, we don't monitor the name change.
            // So the dbus object path is not updated if the db name changes.
            // This should not be a problem because once the database gets saved,
            // the dbus path will be updated to use filename and everything back to normal.
            return m_backend->database()->metadata()->name();
        }
        return QFileInfo(m_backendPath).completeBaseName();
    }

    DatabaseWidget* Collection::backend() const
    {
        return m_backend;
    }

    void Collection::onDatabaseLockChanged()
    {
        if (!reloadBackend()) {
            doDelete();
            return;
        }
        emit collectionLockChanged(backendLocked());
    }

    void Collection::populateContents()
    {
        if (!m_backend) {
            return;
        }

        // we have an unlocked db

        auto newUuid = FdoSecrets::settings()->exposedGroup(m_backend->database());
        auto newGroup = m_backend->database()->rootGroup()->findGroupByUuid(newUuid);
        if (!newGroup || inRecycleBin(newGroup)) {
            // no exposed group, delete self
            doDelete();
            return;
        }

        // clean up old group connections
        cleanupConnections();

        m_exposedGroup = newGroup;

        // Attach signal to update exposed group settings if the group was removed.
        //
        // When the group object is normally deleted due to ~Database, the databaseReplaced
        // signal should be first emitted, and we will clean up connection in reloadDatabase,
        // so this handler won't be triggered.
        connect(m_exposedGroup.data(), &Group::groupAboutToRemove, this, [this](Group* toBeRemoved) {
            if (backendLocked()) {
                return;
            }
            auto db = m_backend->database();
            if (toBeRemoved->database() != db) {
                // should not happen, but anyway.
                // somehow our current database has been changed, and the old group is being deleted
                // possibly logic changes in replaceDatabase.
                return;
            }
            auto uuid = FdoSecrets::settings()->exposedGroup(db);
            auto exposedGroup = db->rootGroup()->findGroupByUuid(uuid);
            if (toBeRemoved == exposedGroup) {
                // reset the exposed group to none
                FdoSecrets::settings()->setExposedGroup(db, {});
            }
        });
        // Another possibility is the group being moved to recycle bin.
        connect(m_exposedGroup.data(), &Group::groupModified, this, [this]() {
            if (inRecycleBin(m_exposedGroup->parentGroup())) {
                // reset the exposed group to none
                FdoSecrets::settings()->setExposedGroup(m_backend->database().data(), {});
            }
        });

        // Monitor exposed group settings
        connect(m_backend->database()->metadata()->customData(), &CustomData::customDataModified, this, [this]() {
            if (!m_exposedGroup || backendLocked()) {
                return;
            }
            if (m_exposedGroup->uuid() == FdoSecrets::settings()->exposedGroup(m_backend->database())) {
                // no change
                return;
            }
            onDatabaseExposedGroupChanged();
        });

        // Add items for existing entry
        const auto entries = m_exposedGroup->entriesRecursive(false);
        for (const auto& entry : entries) {
            onEntryAdded(entry, false);
        }

        // Do not connect to databaseModified signal because we only want signals for the subset under m_exposedGroup
        connect(m_backend->database()->metadata(), &Metadata::metadataModified, this, &Collection::collectionChanged);
        connectGroupSignalRecursive(m_exposedGroup);
    }

    void Collection::onDatabaseExposedGroupChanged()
    {
        // delete all items
        // this has to be done because the backend is actually still there
        // just we don't expose them
        for (const auto& item : asConst(m_items)) {
            item->doDelete();
        }

        // repopulate
        if (!backendLocked()) {
            populateContents();
        }
    }

    void Collection::onEntryAdded(Entry* entry, bool emitSignal)
    {
        if (inRecycleBin(entry)) {
            return;
        }

        auto item = Item::Create(this, entry);
        m_items << item;
        m_entryToItem[entry] = item;

        // forward delete signals
        connect(entry->group(), &Group::entryAboutToRemove, item, [item](Entry* toBeRemoved) {
            if (item->backend() == toBeRemoved) {
                item->doDelete();
            }
        });

        // relay signals
        connect(item, &Item::itemChanged, this, [this, item]() { emit itemChanged(item); });
        connect(item, &Item::itemAboutToDelete, this, [this, item]() {
            m_items.removeAll(item);
            m_entryToItem.remove(item->backend());
            emit itemDeleted(item);
        });

        if (emitSignal) {
            emit itemCreated(item);
        }
    }

    void Collection::connectGroupSignalRecursive(Group* group)
    {
        if (inRecycleBin(group)) {
            return;
        }

        connect(group, &Group::groupModified, this, &Collection::collectionChanged);
        connect(group, &Group::entryAdded, this, [this](Entry* entry) { onEntryAdded(entry, true); });

        const auto children = group->children();
        for (const auto& cg : children) {
            connectGroupSignalRecursive(cg);
        }
    }

    Service* Collection::service() const
    {
        return qobject_cast<Service*>(parent());
    }

    bool Collection::doLock()
    {
        Q_ASSERT(m_backend);

        return m_backend->lock();
    }

    void Collection::doUnlock()
    {
        Q_ASSERT(m_backend);

        service()->doUnlockDatabaseInDialog(m_backend);
    }

    void Collection::doDelete()
    {
        if (!m_backend) {
            // I'm already deleted
            return;
        }

        emit collectionAboutToDelete();

        // remove from dbus early
        dbus().unregisterObject(this);

        // remove alias manually to trigger signal
        for (const auto& a : aliases()) {
            removeAlias(a).okOrDie();
        }

        // cleanup connection on Database
        cleanupConnections();
        // cleanup connection on Backend itself
        m_backend->disconnect(this);
        parent()->disconnect(this);

        m_exposedGroup = nullptr;

        // reset backend and delete self
        m_backend = nullptr;
        deleteLater();

        // items will be removed automatically as they are children objects
    }

    void Collection::cleanupConnections()
    {
        m_backend->database()->metadata()->customData()->disconnect(this);
        if (m_exposedGroup) {
            for (const auto group : m_exposedGroup->groupsRecursive(true)) {
                group->disconnect(this);
            }
        }

        m_items.clear();
    }

    QString Collection::backendFilePath() const
    {
        return m_backendPath;
    }

    Group* Collection::exposedRootGroup() const
    {
        return m_exposedGroup;
    }

    bool Collection::backendLocked() const
    {
        return !m_backend || !m_backend->database()->isInitialized() || m_backend->isLocked();
    }

    void Collection::doDeleteEntries(QList<Entry*> entries)
    {
        m_backend->deleteEntries(std::move(entries), FdoSecrets::settings()->confirmDeleteItem());
    }

    Group* Collection::findCreateGroupByPath(const QString& groupPath)
    {
        auto group = m_exposedGroup->findGroupByPath(groupPath);
        if (group) {
            return group;
        }

        // groupPath can't be empty here, because otherwise it will match m_exposedGroup and was returned above
        Q_ASSERT(!groupPath.isEmpty());

        auto groups = groupPath.split('/', QString::SkipEmptyParts);
        auto groupName = groups.takeLast();

        // create parent group
        auto parentGroup = findCreateGroupByPath(groups.join('/'));

        // create this group
        group = new Group();
        group->setUuid(QUuid::createUuid());
        group->setName(groupName);
        group->setIcon(Group::DefaultIconNumber);
        group->setParent(parentGroup);

        return group;
    }

    bool Collection::inRecycleBin(Group* group) const
    {
        Q_ASSERT(m_backend);

        if (!group) {
            // the root group's parent is nullptr, we treat it as not in recycle bin.
            return false;
        }

        if (!m_backend->database()->metadata()) {
            return false;
        }

        auto recycleBin = m_backend->database()->metadata()->recycleBin();
        if (!recycleBin) {
            return false;
        }

        return group->uuid() == recycleBin->uuid() || group->isRecycled();
    }

    bool Collection::inRecycleBin(Entry* entry) const
    {
        Q_ASSERT(entry);
        return inRecycleBin(entry->group());
    }

    DBusResult Collection::doNewItem(QString itemPath, Item*& created)
    {
        Q_ASSERT(m_backend);
        created = nullptr;

        // normalize itemPath
        itemPath = (itemPath.startsWith('/') ? QString{} : QStringLiteral("/")) + itemPath;

        // split itemPath to groupPath and itemName
        auto components = itemPath.split('/');
        Q_ASSERT(components.size() >= 2);

        auto itemName = components.takeLast();
        Group* group = findCreateGroupByPath(components.join('/'));

        // create new Entry in backend
        auto* entry = new Entry();
        entry->setUuid(QUuid::createUuid());
        entry->setTitle(itemName);
        entry->setUsername(m_backend->database()->metadata()->defaultUserName());
        group->applyGroupIconOnCreateTo(entry);

        entry->setGroup(group);

        // when creation finishes in backend, we will already have item
        created = m_entryToItem.value(entry, nullptr);

        if (!created) {
            // may happen if entry somehow ends up in recycle bin
            return DBusResult(QStringLiteral(DBUS_ERROR_SECRET_NO_SUCH_OBJECT));
        }

        // the item was just created so there is no point in having it not authorized
        dbus().callingClient()->setItemAuthorized(entry->uuid(), AuthDecision::Allowed);

        return {};
    }

} // namespace FdoSecrets
