/*
 *  Copyright (C) 2021 Team KeePassXC <team@keepassxc.org>
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

#ifndef KEEPASSX_AUTOTYPESELECTDIALOG_H
#define KEEPASSX_AUTOTYPESELECTDIALOG_H

#include "autotype/AutoTypeMatch.h"
#include <QDialog>
#include <QTimer>

class Database;
class QMenu;

namespace Ui
{
    class AutoTypeSelectDialog;
}

class AutoTypeSelectDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AutoTypeSelectDialog(QWidget* parent = nullptr);
    ~AutoTypeSelectDialog() override;

    void setMatches(const QList<AutoTypeMatch>& matchList, const QList<QSharedPointer<Database>>& dbs);
    void setUrl(const QString& url);

signals:
    void matchActivated(AutoTypeMatch match);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private slots:
    void submitAutoTypeMatch(AutoTypeMatch match);
    void performSearch();
    void moveSelectionUp();
    void moveSelectionDown();
    void activateCurrentMatch();
    void updateActionMenu(const AutoTypeMatch& match);

private:
    void buildActionMenu();

    QScopedPointer<Ui::AutoTypeSelectDialog> m_ui;

    QList<QSharedPointer<Database>> m_dbs;
    QList<AutoTypeMatch> m_matches;
    QTimer m_searchTimer;
    QPointer<QMenu> m_actionMenu;

    bool m_accepted = false;
};

#endif // KEEPASSX_AUTOTYPESELECTDIALOG_H
