/*
 *  Copyright (C) 2019 KeePassXC Team <team@keepassxc.org>
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

#include "OpVaultOpenWidget.h"

#include <QDir>

#include "core/Database.h"
#include "core/Metadata.h"
#include "format/OpVaultReader.h"
#include "gui/MessageBox.h"
#include "ui_DatabaseOpenWidget.h"

OpVaultOpenWidget::OpVaultOpenWidget(QWidget* parent)
    : DatabaseOpenWidget(parent)
{
    m_ui->labelHeadline->setText("Import 1Password Database");
}

void OpVaultOpenWidget::openDatabase(bool useOsStoredKey)
{
    // TODO: implement storing and retrieving os stored key
    Q_UNUSED(useOsStoredKey);
    OpVaultReader reader;

    QString password;
    password = m_ui->editPassword->text();

    QDir opVaultDir(m_filename);

    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    m_db.reset(reader.readDatabase(opVaultDir, password));
    QApplication::restoreOverrideCursor();

    if (m_db) {
        emit dialogFinished(true);
    } else {
        m_ui->messageWidget->showMessage(tr("Read Database did not produce an instance\n%1").arg(reader.errorString()),
                                         MessageWidget::Error);
        m_ui->editPassword->clear();
    }
}
