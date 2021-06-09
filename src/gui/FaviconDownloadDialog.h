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

#ifndef KEEPASSX_FAVICON_DOWNLOAD_DIALOG_H
#define KEEPASSX_FAVICON_DOWNLOAD_DIALOG_H

#include "core/Database.h"
#include "core/Entry.h"
#include "gui/DatabaseWidget.h"
#include <QDialog>
#include <QScopedPointer>

namespace Ui
{
    class FaviconDownloadDialog;
}

class FaviconDownloadDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FaviconDownloadDialog(DatabaseWidget* parent = nullptr, Database* db = nullptr, Entry* entry = nullptr);
    ~FaviconDownloadDialog();

private:
    QScopedPointer<Ui::FaviconDownloadDialog> m_ui;

private slots:

protected:
    Database* m_db;
    Entry* m_entry;
    DatabaseWidget* m_parent;
};

#endif // KEEPASSX_FAVICON_DOWNLOAD_DIALOG_H
