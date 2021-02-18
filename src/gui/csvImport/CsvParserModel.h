/*
 *  Copyright (C) 2016 Enrico Mariotti <enricomariotti@yahoo.it>
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

#ifndef KEEPASSX_CSVPARSERMODEL_H
#define KEEPASSX_CSVPARSERMODEL_H

#include <QAbstractTableModel>
#include <QMap>

#include "core/Group.h"
#include "format/CsvParser.h"

class CsvParserModel : public QAbstractTableModel, public CsvParser
{
    Q_OBJECT

public:
    explicit CsvParserModel(QObject* parent = nullptr);
    ~CsvParserModel();
    void setFilename(const QString& filename);
    QString getFileInfo();
    bool parse();

    void setHeaderLabels(const QStringList& labels);
    void mapColumns(int csvColumn, int dbColumn);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

public slots:
    void setSkippedRows(int skipped);

private:
    int m_skipped;
    QString m_filename;
    QStringList m_columnHeader;
    // first column of model must be empty (aka combobox row "Not present in CSV file")
    void addEmptyColumn();
    // mapping CSV columns to keepassx columns
    QMap<int, int> m_columnMap;
};

#endif // KEEPASSX_CSVPARSERMODEL_H
