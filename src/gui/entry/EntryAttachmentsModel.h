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

#ifndef KEEPASSX_ENTRYATTACHMENTSMODEL_H
#define KEEPASSX_ENTRYATTACHMENTSMODEL_H

#include <QAbstractListModel>
#include <QPointer>

class EntryAttachments;

class EntryAttachmentsModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Columns
    {
        NameColumn,
        SizeColumn,
        ColumnsCount
    };

    explicit EntryAttachmentsModel(QObject* parent = nullptr);
    void setEntryAttachments(EntryAttachments* entry);
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QString keyByIndex(const QModelIndex& index) const;

private slots:
    void attachmentChange(const QString& key);
    void attachmentAboutToAdd(const QString& key);
    void attachmentAdd();
    void attachmentAboutToRemove(const QString& key);
    void attachmentRemove();
    void aboutToReset();
    void reset();
    void setReadOnly(bool readOnly);

private:
    QPointer<EntryAttachments> m_entryAttachments;
    QStringList m_headers;
    bool m_readOnly = false;
};

#endif // KEEPASSX_ENTRYATTACHMENTSMODEL_H
