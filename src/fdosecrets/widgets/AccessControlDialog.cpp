/*
 *  Copyright (C) 2013 Francois Ferrand
 *  Copyright (C) 2017 KeePassXC Team <team@keepassxc.org>
 *  Copyright (C) 2020 Aetf <aetf@unlimitedcodeworks.xyz>
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

#include "AccessControlDialog.h"
#include "ui_AccessControlDialog.h"

#include "fdosecrets/widgets/RowButtonHelper.h"

#include "core/Entry.h"
#include "gui/Icons.h"

#include <QWindow>

#include <utility>

AccessControlDialog::AccessControlDialog(QWindow* parent,
                                         const QList<Entry*>& entries,
                                         const QString& app,
                                         AuthOptions authOptions)
    : m_ui(new Ui::AccessControlDialog())
    , m_model(new EntryModel(entries))
{
    if (parent) {
        // Force the creation of the QWindow, without this windowHandle() will return nullptr
        winId();
        auto window = windowHandle();
        Q_ASSERT(window);
        window->setTransientParent(parent);
    }
    setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);

    m_ui->setupUi(this);

    connect(m_ui->cancelButton, &QPushButton::clicked, this, [this]() { done(DenyAll); });
    connect(m_ui->allowButton, &QPushButton::clicked, this, [this]() { done(AllowSelected); });
    connect(m_ui->itemsTable, &QTableView::clicked, m_model.data(), &EntryModel::toggleCheckState);
    connect(m_ui->rememberCheck, &QCheckBox::clicked, this, &AccessControlDialog::rememberChecked);
    connect(this, &QDialog::finished, this, &AccessControlDialog::dialogFinished);

    m_ui->rememberMsg->setCloseButtonVisible(false);
    m_ui->rememberMsg->setMessageType(MessageWidget::Information);

    m_ui->appLabel->setText(m_ui->appLabel->text().arg(app));

    m_ui->itemsTable->setModel(m_model.data());
    installWidgetItemDelegate<DenyButton>(m_ui->itemsTable, 2, [this](QWidget* p, const QModelIndex& idx) {
        auto btn = new DenyButton(p, idx);
        connect(btn, &DenyButton::clicked, this, &AccessControlDialog::denyEntryClicked);
        return btn;
    });
    m_ui->itemsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_ui->itemsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_ui->itemsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_ui->itemsTable->resizeColumnsToContents();

    if (!authOptions.testFlag(AuthOption::Remember)) {
        m_ui->rememberCheck->setHidden(true);
        m_ui->rememberCheck->setChecked(false);
    }
    if (!authOptions.testFlag(AuthOption::PerEntryDeny)) {
        m_ui->itemsTable->horizontalHeader()->setSectionHidden(2, true);
    }

    m_ui->allowButton->setFocus();
}

AccessControlDialog::~AccessControlDialog() = default;

void AccessControlDialog::rememberChecked(bool checked)
{
    if (checked) {
        m_ui->rememberMsg->animatedShow();
    } else {
        m_ui->rememberMsg->animatedHide();
    }
}

void AccessControlDialog::denyEntryClicked(Entry* entry, const QModelIndex& index)
{
    m_decisions.insert(entry, AuthDecision::Denied);
    m_model->removeRow(index.row());
    if (m_model->rowCount({}) == 0) {
        reject();
    }
}

void AccessControlDialog::dialogFinished(int result)
{
    auto allow = m_ui->rememberCheck->isChecked() ? AuthDecision::Allowed : AuthDecision::AllowedOnce;
    auto deny = m_ui->rememberCheck->isChecked() ? AuthDecision::Denied : AuthDecision::DeniedOnce;

    for (int row = 0; row != m_model->rowCount({}); ++row) {
        auto entry = m_model->data(m_model->index(row, 2), Qt::EditRole).value<Entry*>();
        auto selected = m_model->data(m_model->index(row, 0), Qt::CheckStateRole).value<Qt::CheckState>();
        Q_ASSERT(entry);
        switch (result) {
        case AllowSelected:
            if (selected) {
                m_decisions.insert(entry, allow);
            } else {
                m_decisions.insert(entry, AuthDecision::Undecided);
            }
            break;
        case DenyAll:
            m_decisions.insert(entry, deny);
            break;
        case Rejected:
        default:
            m_decisions.insert(entry, AuthDecision::Undecided);
            break;
        }
    }

    emit finished(m_decisions);
}

QHash<Entry*, AuthDecision> AccessControlDialog::decisions() const
{
    return m_decisions;
}

AccessControlDialog::EntryModel::EntryModel(QList<Entry*> entries, QObject* parent)
    : QAbstractTableModel(parent)
    , m_entries(std::move(entries))
    , m_selected(QSet<Entry*>::fromList(m_entries))
{
}

int AccessControlDialog::EntryModel::rowCount(const QModelIndex& parent) const
{
    return isValid(parent) ? 0 : m_entries.count();
}

int AccessControlDialog::EntryModel::columnCount(const QModelIndex& parent) const
{
    return isValid(parent) ? 0 : 3;
}

bool AccessControlDialog::EntryModel::isValid(const QModelIndex& index) const
{
    return index.isValid() && index.row() < rowCount({}) && index.column() < columnCount({});
}

void AccessControlDialog::EntryModel::toggleCheckState(const QModelIndex& index)
{
    if (!isValid(index)) {
        return;
    }
    auto entry = m_entries.at(index.row());
    // click anywhere in the row to check/uncheck the item
    auto it = m_selected.find(entry);
    if (it == m_selected.end()) {
        m_selected.insert(entry);
    } else {
        m_selected.erase(it);
    }
    auto rowIdx = index.sibling(index.row(), 0);
    emit dataChanged(rowIdx, rowIdx, {Qt::CheckStateRole});
}

QVariant AccessControlDialog::EntryModel::data(const QModelIndex& index, int role) const
{
    if (!isValid(index)) {
        return {};
    }
    auto entry = m_entries.at(index.row());

    switch (index.column()) {
    case 0:
        switch (role) {
        case Qt::DisplayRole:
            return entry->title();
        case Qt::DecorationRole:
            return Icons::entryIcon(entry);
        case Qt::CheckStateRole:
            return QVariant::fromValue(m_selected.contains(entry) ? Qt::Checked : Qt::Unchecked);
        default:
            return {};
        }
    case 1:
        switch (role) {
        case Qt::DisplayRole:
            return entry->username();
        default:
            return {};
        }
    case 2:
        switch (role) {
        case Qt::EditRole:
            return QVariant::fromValue(entry);
        default:
            return {};
        }
    default:
        return {};
    }
}

bool AccessControlDialog::EntryModel::removeRows(int row, int count, const QModelIndex& parent)
{
    beginRemoveRows(parent, row, row + count - 1);
    while (count--) {
        m_entries.removeAt(row);
    }
    endRemoveRows();
    return true;
}

AccessControlDialog::DenyButton::DenyButton(QWidget* p, const QModelIndex& idx)
    : QPushButton(p)
    , m_index(idx)
    , m_entry()
{
    setText(tr("Deny for this program"));
    connect(this, &QPushButton::clicked, [this]() { emit clicked(entry(), m_index); });
}

void AccessControlDialog::DenyButton::setEntry(Entry* e)
{
    m_entry = e;
}

Entry* AccessControlDialog::DenyButton::entry() const
{
    return m_entry;
}
