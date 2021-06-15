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

#include "TotpLineEdit.h"

#include "gui/Clipboard.h"
#include "gui/MainWindow.h"

#include <QMouseEvent>

TotpLineEdit::TotpLineEdit(QWidget* parent)
    : QLineEdit(parent)
{
    setBackgroundRole(QPalette::Window);
    setFixedWidth(70);
    setReadOnly(true);
    setFrame(false);
    setDragEnabled(true);
    setFocusPolicy(Qt::FocusPolicy::ClickFocus);
    setAttribute(Qt::WA_DeleteOnClose);
}

TotpLineEdit::~TotpLineEdit()
{
}

void TotpLineEdit::setEntry(Entry* entry)
{
    m_entry = entry;
}

void TotpLineEdit::mouseDoubleClickEvent(QMouseEvent* e)
{
    if (text() == "") {
        return;
    }

    if (e->button() == Qt::LeftButton) {
        clipboard()->setText(m_entry->totp());
        if (config()->get(Config::HideWindowOnCopy).toBool()) {
            if (config()->get(Config::MinimizeOnCopy).toBool()) {
                getMainWindow()->minimizeOrHide();
            } else if (config()->get(Config::DropToBackgroundOnCopy).toBool()) {
                getMainWindow()->lower();
                window()->lower();
            }
        }
    }

    QLineEdit::mouseDoubleClickEvent(e);
}
