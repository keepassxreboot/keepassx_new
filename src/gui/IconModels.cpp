/*
 *  Copyright (C) 2012 Felix Geyer <debfx@fobos.de>
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

#include "IconModels.h"

#include <QUuid>

#include "gui/DatabaseIcons.h"

DefaultIconModel::DefaultIconModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int DefaultIconModel::rowCount(const QModelIndex& parent) const
{
    if (!parent.isValid()) {
        return databaseIcons()->count();
    } else {
        return 0;
    }
}

QVariant DefaultIconModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    Q_ASSERT(index.row() < databaseIcons()->count());

    if (role == Qt::DecorationRole) {
        return databaseIcons()->icon(index.row(), IconSize::Medium);
    }

    return QVariant();
}

CustomIconModel::CustomIconModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

void CustomIconModel::setIcons(const QHash<QUuid, QPixmap>& icons, const QList<QUuid>& iconsOrder)
{
    beginResetModel();

    m_icons = icons;
    m_iconsOrder = iconsOrder;
    Q_ASSERT(m_icons.count() == m_iconsOrder.count());

    endResetModel();
}

int CustomIconModel::rowCount(const QModelIndex& parent) const
{
    if (!parent.isValid()) {
        return m_icons.size();
    } else {
        return 0;
    }
}

QVariant CustomIconModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    if (role == Qt::DecorationRole) {
        QUuid uuid = uuidFromIndex(index);
        return m_icons.value(uuid);
    }

    return QVariant();
}

QUuid CustomIconModel::uuidFromIndex(const QModelIndex& index) const
{
    Q_ASSERT(index.isValid());

    return m_iconsOrder.value(index.row());
}

QModelIndex CustomIconModel::indexFromUuid(const QUuid& uuid) const
{
    int idx = m_iconsOrder.indexOf(uuid);
    if (idx > -1) {
        return index(idx, 0);
    }
    return {};
}
