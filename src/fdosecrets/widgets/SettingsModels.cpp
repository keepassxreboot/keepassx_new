/*
 *  Copyright (C) 2019 Aetf <aetf@unlimitedcodeworks.xyz>
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

#include "SettingsModels.h"

#include "fdosecrets/FdoSecretsPlugin.h"
#include "fdosecrets/FdoSecretsSettings.h"
#include "fdosecrets/dbus/DBusMgr.h"
#include "fdosecrets/objects/Service.h"
#include "fdosecrets/objects/Session.h"

#include "gui/DatabaseTabWidget.h"
#include "gui/DatabaseWidget.h"
#include "gui/Icons.h"

#include <QFileInfo>

namespace FdoSecrets
{

    SettingsDatabaseModel::SettingsDatabaseModel(DatabaseTabWidget* dbTabs, QObject* parent)
        : QAbstractTableModel(parent)
        , m_dbTabs(nullptr)
    {
        setTabWidget(dbTabs);
    }

    void SettingsDatabaseModel::setTabWidget(DatabaseTabWidget* dbTabs)
    {
        auto old = m_dbTabs;
        m_dbTabs = dbTabs;
        if (old != m_dbTabs) {
            populateModel();
        }
    }

    int SettingsDatabaseModel::rowCount(const QModelIndex& parent) const
    {
        if (parent.isValid()) {
            return 0;
        }
        return m_dbs.size();
    }

    int SettingsDatabaseModel::columnCount(const QModelIndex& parent) const
    {
        if (parent.isValid()) {
            return 0;
        }
        return 3;
    }

    QVariant SettingsDatabaseModel::headerData(int section, Qt::Orientation orientation, int role) const
    {
        if (orientation != Qt::Horizontal) {
            return {};
        }

        if (role != Qt::DisplayRole) {
            return {};
        }

        switch (section) {
        case 0:
            return tr("File Name");
        case 1:
            return tr("Group");
        case 2:
            return tr("Manage");
        default:
            return {};
        }
    }

    QVariant SettingsDatabaseModel::data(const QModelIndex& index, int role) const
    {
        if (!index.isValid()) {
            return {};
        }
        const auto& dbWidget = m_dbs[index.row()];
        if (!dbWidget) {
            return {};
        }

        switch (index.column()) {
        case 0:
            return dataForName(dbWidget, role);
        case 1:
            return dataForExposedGroup(dbWidget, role);
        case 2:
            return dataForManage(dbWidget, role);
        default:
            return {};
        }
    }

    QVariant SettingsDatabaseModel::dataForName(DatabaseWidget* db, int role) const
    {
        switch (role) {
        case Qt::DisplayRole: {
            QFileInfo fi(db->database()->filePath());
            return fi.fileName();
        }
        case Qt::ToolTipRole:
            return db->database()->filePath();
        default:
            return {};
        }
    }

    QVariant SettingsDatabaseModel::dataForExposedGroup(DatabaseWidget* dbWidget, int role)
    {
        if (dbWidget->isLocked()) {
            switch (role) {
            case Qt::DisplayRole:
                return tr("Unlock to show");
            case Qt::DecorationRole:
                return icons()->icon(QStringLiteral("object-locked"));
            case Qt::FontRole: {
                QFont font;
                font.setItalic(true);
                return font;
            }
            default:
                return {};
            }
        }
        auto db = dbWidget->database();
        auto group = db->rootGroup()->findGroupByUuid(FdoSecrets::settings()->exposedGroup(db));
        if (group) {
            switch (role) {
            case Qt::DisplayRole:
                return group->name();
            case Qt::DecorationRole:
                return Icons::groupIconPixmap(group);
            case Qt::FontRole:
                if (group->isExpired()) {
                    QFont font;
                    font.setStrikeOut(true);
                    return font;
                } else {
                    return {};
                }
            default:
                return {};
            }
        } else {
            switch (role) {
            case Qt::DisplayRole:
                return tr("None");
            case Qt::DecorationRole:
                return icons()->icon(QStringLiteral("paint-none"));
            default:
                return {};
            }
        }
    }

    QVariant SettingsDatabaseModel::dataForManage(DatabaseWidget* db, int role) const
    {
        switch (role) {
        case Qt::EditRole:
            return QVariant::fromValue(db);
        default:
            return {};
        }
    }

    void SettingsDatabaseModel::populateModel()
    {
        beginResetModel();

        m_dbs.clear();

        if (m_dbTabs) {
            // Add existing database tabs
            for (int idx = 0; idx != m_dbTabs->count(); ++idx) {
                auto dbWidget = m_dbTabs->databaseWidgetFromIndex(idx);
                databaseAdded(dbWidget, false);
            }
            // connect signals
            connect(m_dbTabs, &DatabaseTabWidget::databaseOpened, this, [this](DatabaseWidget* db) {
                databaseAdded(db, true);
            });
            connect(m_dbTabs, &DatabaseTabWidget::databaseClosed, this, &SettingsDatabaseModel::databaseRemoved);
        }

        endResetModel();
    }

    void SettingsDatabaseModel::databaseAdded(DatabaseWidget* db, bool emitSignals)
    {
        int row = m_dbs.size();
        if (emitSignals) {
            beginInsertRows({}, row, row);
        }

        m_dbs.append(db);
        connect(db, &DatabaseWidget::databaseLocked, this, [row, this]() {
            emit dataChanged(index(row, 1), index(row, 2));
        });
        connect(db, &DatabaseWidget::databaseUnlocked, this, [row, this]() {
            emit dataChanged(index(row, 1), index(row, 2));
        });
        connect(db, &DatabaseWidget::databaseModified, this, [row, this]() {
            emit dataChanged(index(row, 0), index(row, 2));
        });
        connect(db, &DatabaseWidget::databaseFilePathChanged, this, [row, this]() {
            emit dataChanged(index(row, 0), index(row, 2));
        });

        if (emitSignals) {
            endInsertRows();
        }
    }

    void SettingsDatabaseModel::databaseRemoved(const QString& filePath)
    {
        for (int i = 0; i != m_dbs.size(); i++) {
            if (m_dbs[i] && m_dbs[i]->database()->filePath() == filePath) {
                beginRemoveRows({}, i, i);

                m_dbs[i]->disconnect(this);
                m_dbs.removeAt(i);

                endRemoveRows();
                break;
            }
        }
    }

    SettingsClientModel::SettingsClientModel(DBusMgr& dbus, QObject* parent)
        : QAbstractTableModel(parent)
        , m_dbus(dbus)
    {
        populateModel();
    }

    int SettingsClientModel::rowCount(const QModelIndex& parent) const
    {
        if (parent.isValid()) {
            return 0;
        }
        return m_clients.size();
    }

    int SettingsClientModel::columnCount(const QModelIndex& parent) const
    {
        if (parent.isValid()) {
            return 0;
        }
        return 2;
    }

    QVariant SettingsClientModel::headerData(int section, Qt::Orientation orientation, int role) const
    {
        if (orientation != Qt::Horizontal) {
            return {};
        }

        if (role != Qt::DisplayRole) {
            return {};
        }

        switch (section) {
        case 0:
            return tr("Application");
        case 1:
            return tr("Manage");
        default:
            return {};
        }
    }

    QVariant SettingsClientModel::data(const QModelIndex& index, int role) const
    {
        if (!index.isValid()) {
            return {};
        }
        const auto& client = m_clients[index.row()];
        if (!client) {
            return {};
        }

        switch (index.column()) {
        case 0:
            return dataForApplication(client, role);
        case 1:
            return dataForManage(client, role);
        default:
            return {};
        }
    }

    QVariant SettingsClientModel::dataForApplication(const DBusClientPtr& client, int role) const
    {
        switch (role) {
        case Qt::DisplayRole:
            return client->name();
        default:
            return {};
        }
    }

    QVariant SettingsClientModel::dataForManage(const DBusClientPtr& client, int role) const
    {
        switch (role) {
        case Qt::EditRole: {
            return QVariant::fromValue(client);
        }
        default:
            return {};
        }
    }

    void SettingsClientModel::populateModel()
    {
        beginResetModel();

        m_clients.clear();

        // Add existing database tabs
        for (const auto& client : m_dbus.clients()) {
            clientConnected(client, false);
        }

        // connect signals
        connect(&m_dbus, &DBusMgr::clientConnected, this, [this](const DBusClientPtr& client) {
            clientConnected(client, true);
        });
        connect(&m_dbus, &DBusMgr::clientDisconnected, this, &SettingsClientModel::clientDisconnected);

        endResetModel();
    }

    void SettingsClientModel::clientConnected(const DBusClientPtr& client, bool emitSignals)
    {
        int row = m_clients.size();
        if (emitSignals) {
            beginInsertRows({}, row, row);
        }

        m_clients.append(client);

        if (emitSignals) {
            endInsertRows();
        }
    }

    void SettingsClientModel::clientDisconnected(const DBusClientPtr& client)
    {
        for (int i = 0; i != m_clients.size(); i++) {
            if (m_clients[i] == client) {
                beginRemoveRows({}, i, i);

                m_clients.removeAt(i);

                endRemoveRows();
                break;
            }
        }
    }

} // namespace FdoSecrets
