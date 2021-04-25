/*
 *  Copyright (C) 2021 KeePassXC Team <team@keepassxc.org>
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

#include "DatabaseSettingsWidgetMaintenance.h"
#include "ui_DatabaseSettingsWidgetMaintenance.h"

#include <QProgressDialog>

#include "core/Database.h"
#include "core/Entry.h"
#include "core/Group.h"
#include "core/Metadata.h"
#include "gui/IconModels.h"
#include "gui/Icons.h"
#include "gui/MessageBox.h"

DatabaseSettingsWidgetMaintenance::DatabaseSettingsWidgetMaintenance(QWidget* parent)
    : DatabaseSettingsWidget(parent)
    , m_ui(new Ui::DatabaseSettingsWidgetMaintenance())
    , m_customIconModel(new CustomIconModel(this))
    , m_deletionDecision(MessageBox::NoButton)
{
    m_ui->setupUi(this);

    m_ui->customIconsView->setModel(m_customIconModel);

    connect(m_ui->deleteButton, SIGNAL(clicked()), SLOT(removeCustomIcon()));
    connect(m_ui->purgeButton, SIGNAL(clicked()), SLOT(purgeUnusedCustomIcons()));
    connect(m_ui->customIconsView->selectionModel(),
            SIGNAL(selectionChanged(QItemSelection, QItemSelection)),
            this,
            SLOT(selectionChanged()));
}

DatabaseSettingsWidgetMaintenance::~DatabaseSettingsWidgetMaintenance()
{
}

void DatabaseSettingsWidgetMaintenance::populateIcons(QSharedPointer<Database> db)
{
    m_customIconModel->setIcons(Icons::customIconsPixmaps(db.data(), IconSize::Default),
                                db->metadata()->customIconsOrder());
    m_ui->deleteButton->setEnabled(false);
}

void DatabaseSettingsWidgetMaintenance::initialize()
{
    auto database = DatabaseSettingsWidget::getDatabase();
    if (!database) {
        return;
    }
    populateIcons(database);
}

void DatabaseSettingsWidgetMaintenance::selectionChanged()
{
    QList<QModelIndex> indexes = m_ui->customIconsView->selectionModel()->selectedIndexes();
    if (indexes.isEmpty()) {
        m_ui->deleteButton->setEnabled(false);
    } else {
        m_ui->deleteButton->setEnabled(true);
    }
}

void DatabaseSettingsWidgetMaintenance::removeCustomIcon()
{
    auto database = DatabaseSettingsWidget::getDatabase();
    if (!database) {
        return;
    }

    m_deletionDecision = MessageBox::NoButton;

    QList<QModelIndex> indexes = m_ui->customIconsView->selectionModel()->selectedIndexes();
    for (auto index : indexes) {
        removeSingleCustomIcon(database, index);
    }

    populateIcons(database);
}

void DatabaseSettingsWidgetMaintenance::removeSingleCustomIcon(QSharedPointer<Database> database, QModelIndex index)
{
    QUuid iconUuid = m_customIconModel->uuidFromIndex(index);

    const QList<Entry*> allEntries = database->rootGroup()->entriesRecursive(true);
    QList<Entry*> entriesWithSelectedIcon;
    QList<Entry*> historicEntriesWithSelectedIcon;

    for (Entry* entry : allEntries) {
        if (iconUuid == entry->iconUuid()) {
            // Check if this is a history entry (no assigned group)
            if (!entry->group()) {
                historicEntriesWithSelectedIcon << entry;
            } else {
                entriesWithSelectedIcon << entry;
            }
        }
    }

    const QList<Group*> allGroups = database->rootGroup()->groupsRecursive(true);
    QList<Group*> groupsWithSameIcon;

    for (Group* group : allGroups) {
        if (iconUuid == group->iconUuid()) {
            groupsWithSameIcon << group;
        }
    }

    int iconUseCount = entriesWithSelectedIcon.size() + groupsWithSameIcon.size();
    if (iconUseCount > 0) {
        if (m_deletionDecision == MessageBox::NoButton) {
            m_deletionDecision = MessageBox::question(
                this,
                tr("Confirm Deletion"),
                tr("At least one of the selected icons is currently in use by at least one entry or group. "
                   "The icons of all affected entries and groups will be replaced by the default icon. "
                   "Are you sure you want to delete icons that are currently in use?"),
                MessageBox::Delete | MessageBox::Skip,
                MessageBox::Skip);
        }

        if (m_deletionDecision == MessageBox::Skip) {
            // Early out, nothing is changed
            return;
        } else {
            // Revert matched entries to the default entry icon
            for (Entry* entry : asConst(entriesWithSelectedIcon)) {
                entry->setIcon(Entry::DefaultIconNumber);
            }

            // Revert matched groups to the default group icon
            for (Group* group : asConst(groupsWithSameIcon)) {
                group->setIcon(Group::DefaultIconNumber);
            }
        }
    }

    // Remove the icon from history entries
    for (Entry* entry : asConst(historicEntriesWithSelectedIcon)) {
        entry->setUpdateTimeinfo(false);
        entry->setIcon(0);
        entry->setUpdateTimeinfo(true);
    }

    // Remove the icon from the database
    database->metadata()->removeCustomIcon(iconUuid);
}

void DatabaseSettingsWidgetMaintenance::purgeUnusedCustomIcons()
{
    auto database = DatabaseSettingsWidget::getDatabase();
    if (!database) {
        return;
    }

    QList<Entry*> historyEntries;
    QSet<QUuid> historicIcons;
    QSet<QUuid> iconsInUse;

    const QList<Entry*> allEntries = database->rootGroup()->entriesRecursive(true);
    for (Entry* entry : allEntries) {
        if (!entry->group()) {
            // Icons exclusively in use by historic entries (no
            // group assigned) are also purged from the database
            historyEntries << entry;
            historicIcons << entry->iconUuid();
        } else {
            iconsInUse << entry->iconUuid();
        }
    }

    const QList<Group*> allGroups = database->rootGroup()->groupsRecursive(true);
    for (Group* group : allGroups) {
        iconsInUse.insert(group->iconUuid());
    }

    int purgeCounter = 0;
    QList<QUuid> customIcons = database->metadata()->customIconsOrder();
    for (QUuid iconUuid : customIcons) {
        if (iconsInUse.contains(iconUuid)) {
            continue;
        }

        if (historicIcons.contains(iconUuid)) {
            // Remove the icon from history entries using this icon
            for (Entry* historicEntry : asConst(historyEntries)) {
                if (historicEntry->iconUuid() != iconUuid) {
                    continue;
                }
                historicEntry->setUpdateTimeinfo(false);
                historicEntry->setIcon(0);
                historicEntry->setUpdateTimeinfo(true);
            }
        }

        ++purgeCounter;
        database->metadata()->removeCustomIcon(iconUuid);
    }

    if (0 == purgeCounter) {
        MessageBox::information(this,
                                tr("Custom Icons Are In Use"),
                                tr("All custom icons are in use by at least one entry or group."),
                                MessageBox::Ok);
        return;
    }

    populateIcons(database);

    MessageBox::information(
        this, tr("Purged Unused Icons"), tr("Purged %n icon(s) from the database.", "", purgeCounter), MessageBox::Ok);
}
