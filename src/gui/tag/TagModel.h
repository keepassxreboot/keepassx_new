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

#ifndef KEEPASSX_TAGMODEL_H
#define KEEPASSX_TAGMODEL_H

#include <QAbstractListModel>
#include <QSet>

class Group;
class Entry;

// TODO: rename as TagGroupModel or make it general
class TagModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit TagModel(Group* g, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QStringList entryTags(const Entry* entry); // TODO: move to entry
    QStringList& tags();
    void findTags();

public slots:
    void setGroup(Group* g);

private:
    Group* m_group;
    QStringList tagList;
};

#endif // KEEPASSX_TAGMODEL_H
