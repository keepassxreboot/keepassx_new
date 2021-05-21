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

#include "BrowserAccessControlDialog.h"
#include "ui_BrowserAccessControlDialog.h"

#include "core/Entry.h"

BrowserAccessControlDialog::BrowserAccessControlDialog(QWidget* parent)
    : QDialog(parent)
    , m_ui(new Ui::BrowserAccessControlDialog())
{
    setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);

    m_ui->setupUi(this);

    connect(m_ui->allowButton, SIGNAL(clicked()), SLOT(accept()));
    connect(m_ui->denyButton, SIGNAL(clicked()), SLOT(reject()));
    connect(m_ui->itemsTable, SIGNAL(cellDoubleClicked(int,int)), this, SLOT(accept()));
    connect(m_ui->itemsTable->selectionModel(), SIGNAL(selectionChanged(QItemSelection, QItemSelection)),
            this, SLOT(selectionChanged()));
}

BrowserAccessControlDialog::~BrowserAccessControlDialog()
{
}

void BrowserAccessControlDialog::setItems(const QList<Entry*>& items, const QString& urlString, bool httpAuth)
{
    QUrl url(urlString);
    m_ui->siteLabel->setText(m_ui->siteLabel->text().arg(
        url.toDisplayString(QUrl::RemoveUserInfo | QUrl::RemovePath | QUrl::RemoveQuery | QUrl::RemoveFragment)));

    m_ui->rememberDecisionCheckBox->setVisible(!httpAuth);
    m_ui->rememberDecisionCheckBox->setChecked(false);

    m_ui->itemsTable->setRowCount(items.count());
    m_ui->itemsTable->setColumnCount(2);

    int row = 0;
    for (const auto& entry : items) {
        auto item = new QTableWidgetItem();
        item->setText(entry->title() + " - " + entry->username());
        item->setData(Qt::UserRole, row);
        item->setFlags(item->flags() | Qt::ItemIsSelectable);
        m_ui->itemsTable->setItem(row, 0, item);

        auto disableButton = new QPushButton(tr("Disable for this site"));
        disableButton->setAutoDefault(false);
        connect(disableButton, &QAbstractButton::pressed, [&, item] {
            emit disableAccess(item);
            m_ui->itemsTable->removeRow(item->row());
            if (m_ui->itemsTable->rowCount() == 0) {
                reject();
            }
        });
        m_ui->itemsTable->setCellWidget(row, 1, disableButton);

        ++row;
    }

    m_ui->itemsTable->resizeColumnsToContents();
    m_ui->itemsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_ui->itemsTable->selectAll();

    m_ui->allowButton->setFocus();
}

bool BrowserAccessControlDialog::remember() const
{
    return m_ui->rememberDecisionCheckBox->isChecked();
}

QList<QTableWidgetItem*> BrowserAccessControlDialog::getSelectedEntries() const
{
    QList<QTableWidgetItem*> selected;
    for (int i = 0; i < m_ui->itemsTable->rowCount(); ++i) {
        auto item = m_ui->itemsTable->item(i, 0);
        if (item->isSelected()) {
            selected.append(item);
        }
    }
    return selected;
}

QList<QTableWidgetItem*> BrowserAccessControlDialog::getNonSelectedEntries() const
{
    QList<QTableWidgetItem*> notSelected;
    for (int i = 0; i < m_ui->itemsTable->rowCount(); ++i) {
        auto item = m_ui->itemsTable->item(i, 0);
        if (!item->isSelected()) {
            notSelected.append(item);
        }
    }
    return notSelected;
}

void BrowserAccessControlDialog::selectionChanged()
{
    auto indexes = m_ui->itemsTable->selectionModel()->selectedIndexes();
    m_ui->allowButton->setEnabled(!indexes.isEmpty());
    
    if (indexes.isEmpty()) {
        m_ui->allowButton->clearFocus();
        m_ui->denyButton->setFocus();
    }
}
