/*
 *  Copyright (C) 2013 Francois Ferrand
 *  Copyright (C) 2021 KeePassXC Team <team@keepassxc.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef BROWSERACCESSCONTROLDIALOG_H
#define BROWSERACCESSCONTROLDIALOG_H

#include <QDialog>
#include <QScopedPointer>
#include <QTableWidgetItem>

class Entry;

namespace Ui
{
    class BrowserAccessControlDialog;
}

class BrowserAccessControlDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BrowserAccessControlDialog(QWidget* parent = nullptr);
    ~BrowserAccessControlDialog() override;

    void setItems(const QList<Entry*>& items, const QString& urlString, bool httpAuth);
    bool remember() const;

    QList<QTableWidgetItem*> getSelectedEntries() const;
    QList<QTableWidgetItem*> getNonSelectedEntries() const;

signals:
    void disableAccess(QTableWidgetItem* item);

private slots:
    void selectionChanged();

private:
    QScopedPointer<Ui::BrowserAccessControlDialog> m_ui;
};

#endif // BROWSERACCESSCONTROLDIALOG_H
