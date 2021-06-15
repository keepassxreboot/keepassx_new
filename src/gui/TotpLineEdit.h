/*
 *  Copyright (C) 2021 Claudio Maradonna <claudio@unitoo.pw>
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

#ifndef KEEPASSX_TOTPLINEEDIT_H
#define KEEPASSX_TOTPLINEEDIT_H

#include <QLineEdit>

class Entry;
class QWidget;
class QMouseEvent;
class TotpLineEdit : public QLineEdit
{
    Q_OBJECT

public:
    explicit TotpLineEdit(QWidget* parent = nullptr);
    ~TotpLineEdit() override;

    void setEntry(Entry* entry = nullptr);

protected:
    void mouseDoubleClickEvent(QMouseEvent*) override;

private:
    Entry* m_entry;
};

#endif
