/*
 *  Copyright (C) 2018 KeePassXC Team <team@keepassxc.org>
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

#include "DatabaseSettingsWidgetDatabaseKey.h"

#include "core/Database.h"
#include "gui/MessageBox.h"
#include "gui/databasekey/KeyFileEditWidget.h"
#include "gui/databasekey/PasswordEditWidget.h"
#include "gui/databasekey/YubiKeyEditWidget.h"
#include "keys/FileKey.h"
#include "keys/PasswordKey.h"
#include "keys/YkChallengeResponseKey.h"

#include <QPushButton>
#include <QSpacerItem>
#include <QVBoxLayout>

#include "config-keepassx.h"

#ifdef WITH_XC_WINDOWSHELLO
#include "winhello/WindowsHello.h"
#endif

DatabaseSettingsWidgetDatabaseKey::DatabaseSettingsWidgetDatabaseKey(QWidget* parent)
    : DatabaseSettingsWidget(parent)
    , m_additionalKeyOptionsToggle(new QPushButton(tr("Add additional protection..."), this))
    , m_additionalKeyOptions(new QWidget(this))
    , m_passwordEditWidget(new PasswordEditWidget(this))
    , m_keyFileEditWidget(new KeyFileEditWidget(this))
#ifdef WITH_XC_YUBIKEY
    , m_yubiKeyEditWidget(new YubiKeyEditWidget(this))
#endif
{
    auto* vbox = new QVBoxLayout(this);
    vbox->setSizeConstraint(QLayout::SetMinimumSize);

    // primary password option
    vbox->addWidget(m_passwordEditWidget);

    // additional key options
    m_additionalKeyOptionsToggle->setObjectName("additionalKeyOptionsToggle");
    vbox->addWidget(m_additionalKeyOptionsToggle);
    vbox->addWidget(m_additionalKeyOptions);
    vbox->setSizeConstraint(QLayout::SetMinimumSize);
    m_additionalKeyOptions->setLayout(new QVBoxLayout());
    m_additionalKeyOptions->layout()->setMargin(0);
    m_additionalKeyOptions->layout()->addWidget(m_keyFileEditWidget);
#ifdef WITH_XC_YUBIKEY
    m_additionalKeyOptions->layout()->addWidget(m_yubiKeyEditWidget);
#endif
    m_additionalKeyOptions->setVisible(false);

    connect(m_additionalKeyOptionsToggle, SIGNAL(clicked()), SLOT(showAdditionalKeyOptions()));

    vbox->addStretch();
    setLayout(vbox);
}

DatabaseSettingsWidgetDatabaseKey::~DatabaseSettingsWidgetDatabaseKey()
{
}

void DatabaseSettingsWidgetDatabaseKey::load(QSharedPointer<Database> db)
{
    DatabaseSettingsWidget::load(db);

    if (!m_db->key() || m_db->key()->keys().isEmpty()) {
        // database has no key, we are about to add a new one
        m_passwordEditWidget->changeVisiblePage(KeyComponentWidget::Page::Edit);
        m_passwordEditWidget->setPasswordVisible(true);
    }

    bool hasAdditionalKeys = false;
    for (const auto& key : m_db->key()->keys()) {
        if (key->uuid() == PasswordKey::UUID) {
            m_passwordEditWidget->setComponentAdded(true);
#ifdef WITH_XC_WINDOWSHELLO
            if (WindowsHello::isAvailable()
                && WindowsHello::containsKey(db->filePath())) {
                m_passwordEditWidget->osStoreKey()->setChecked(true);
            }
            else {
                m_passwordEditWidget->osStoreKey()->setChecked(false);
            }
#endif
        } else if (key->uuid() == FileKey::UUID) {
            m_keyFileEditWidget->setComponentAdded(true);
            hasAdditionalKeys = true;
        }
    }

#ifdef WITH_XC_YUBIKEY
    for (const auto& key : m_db->key()->challengeResponseKeys()) {
        if (key->uuid() == YkChallengeResponseKey::UUID) {
            m_yubiKeyEditWidget->setComponentAdded(true);
            hasAdditionalKeys = true;
        }
    }
#endif

    setAdditionalKeyOptionsVisible(hasAdditionalKeys);

    connect(m_passwordEditWidget->findChild<QPushButton*>("removeButton"), SIGNAL(clicked()), SLOT(markDirty()));
    connect(m_keyFileEditWidget->findChild<QPushButton*>("removeButton"), SIGNAL(clicked()), SLOT(markDirty()));
#ifdef WITH_XC_YUBIKEY
    connect(m_yubiKeyEditWidget->findChild<QPushButton*>("removeButton"), SIGNAL(clicked()), SLOT(markDirty()));
#endif
}

void DatabaseSettingsWidgetDatabaseKey::initialize()
{
    bool blocked = blockSignals(true);
    m_passwordEditWidget->setComponentAdded(false);
    m_keyFileEditWidget->setComponentAdded(false);
#ifdef WITH_XC_YUBIKEY
    m_yubiKeyEditWidget->setComponentAdded(false);
#endif
    blockSignals(blocked);
}

void DatabaseSettingsWidgetDatabaseKey::uninitialize()
{
}

bool DatabaseSettingsWidgetDatabaseKey::save()
{
    m_isDirty |= (m_passwordEditWidget->visiblePage() == KeyComponentWidget::Page::Edit);
    m_isDirty |= (m_keyFileEditWidget->visiblePage() == KeyComponentWidget::Page::Edit);
#ifdef WITH_XC_YUBIKEY
    m_isDirty |= (m_yubiKeyEditWidget->visiblePage() == KeyComponentWidget::Page::Edit);
#endif

    if (m_db->key() && !m_db->key()->keys().isEmpty() && !m_isDirty) {
        // key unchanged
        updateOsStore();
        return true;
    }

    auto newKey = QSharedPointer<CompositeKey>::create();

    QSharedPointer<Key> oldPasswordKey;
    QSharedPointer<Key> oldFileKey;
    QSharedPointer<ChallengeResponseKey> oldChallengeResponse;

    for (const auto& key : m_db->key()->keys()) {
        if (key->uuid() == PasswordKey::UUID) {
            oldPasswordKey = key;
        } else if (key->uuid() == FileKey::UUID) {
            oldFileKey = key;
        }
    }

    for (const auto& key : m_db->key()->challengeResponseKeys()) {
        if (key->uuid() == YkChallengeResponseKey::UUID) {
            oldChallengeResponse = key;
        }
    }

    if (m_passwordEditWidget->visiblePage() == KeyComponentWidget::Page::AddNew || m_passwordEditWidget->isEmpty()) {
        QScopedPointer<QMessageBox> msgBox(new QMessageBox(this));
        msgBox->setIcon(QMessageBox::Warning);
        msgBox->setWindowTitle(tr("No password set"));
        msgBox->setText(tr("WARNING! You have not set a password. Using a database without "
                           "a password is strongly discouraged!\n\n"
                           "Are you sure you want to continue without a password?"));
        auto btn = msgBox->addButton(tr("Continue without password"), QMessageBox::ButtonRole::AcceptRole);
        msgBox->addButton(QMessageBox::Cancel);
        msgBox->setDefaultButton(QMessageBox::Cancel);
        msgBox->exec();
        if (msgBox->clickedButton() != btn) {
            return false;
        }
    } else if (!addToCompositeKey(m_passwordEditWidget, newKey, oldPasswordKey)) {
        return false;
    }

    if (!addToCompositeKey(m_keyFileEditWidget, newKey, oldFileKey)) {
        return false;
    }

#ifdef WITH_XC_YUBIKEY
    if (!addToCompositeKey(m_yubiKeyEditWidget, newKey, oldChallengeResponse)) {
        return false;
    }
#endif

    if (newKey->keys().isEmpty() && newKey->challengeResponseKeys().isEmpty()) {
        MessageBox::critical(this,
                             tr("No encryption key added"),
                             tr("You must add at least one encryption key to secure your database!"),
                             MessageBox::Ok,
                             MessageBox::Ok);
        return false;
    }

    m_db->setKey(newKey, true, false, false);
    updateOsStore();

    emit editFinished(true);
    if (m_isDirty) {
        m_db->markAsModified();
    }

    return true;
}

void DatabaseSettingsWidgetDatabaseKey::discard()
{
    emit editFinished(false);
}

void DatabaseSettingsWidgetDatabaseKey::updateOsStore()
{
#ifdef WITH_XC_WINDOWSHELLO
    if (WindowsHello::isAvailable()) {
        const bool pwdStored = [this]() {
            if (m_passwordEditWidget->osStoreKey()->isChecked()) {
                for (const auto& key : m_db->key()->keys()) {
                    if (key->uuid() == PasswordKey::UUID) {
                        return WindowsHello(this)
                            .storeKey(m_db->filePath(), key->rawKey());
                    }
                }
            }
            return false;
        }();
        if (!pwdStored) { // Remove, any stored password key for DB
            WindowsHello(this).removeKey(m_db->filePath());
        }
    }
#endif
}

void DatabaseSettingsWidgetDatabaseKey::showAdditionalKeyOptions()
{
    setAdditionalKeyOptionsVisible(true);
}

void DatabaseSettingsWidgetDatabaseKey::setAdditionalKeyOptionsVisible(bool show)
{
    m_additionalKeyOptionsToggle->setVisible(!show);
    m_additionalKeyOptions->setVisible(show);
}

bool DatabaseSettingsWidgetDatabaseKey::addToCompositeKey(KeyComponentWidget* widget,
                                                          QSharedPointer<CompositeKey>& newKey,
                                                          QSharedPointer<Key>& oldKey)
{
    if (widget->visiblePage() == KeyComponentWidget::Edit) {
        QString error = tr("Unknown error");
        if (!widget->validate(error) || !widget->addToCompositeKey(newKey)) {
            MessageBox::critical(this, tr("Failed to change database credentials"), error, MessageBox::Ok);
            return false;
        }
    } else if (widget->visiblePage() == KeyComponentWidget::LeaveOrRemove) {
        Q_ASSERT(oldKey);
        newKey->addKey(oldKey);
    }
    return true;
}

bool DatabaseSettingsWidgetDatabaseKey::addToCompositeKey(KeyComponentWidget* widget,
                                                          QSharedPointer<CompositeKey>& newKey,
                                                          QSharedPointer<ChallengeResponseKey>& oldKey)
{
    if (widget->visiblePage() == KeyComponentWidget::Edit) {
        QString error = tr("Unknown error");
        if (!widget->validate(error) || !widget->addToCompositeKey(newKey)) {
            MessageBox::critical(this, tr("Failed to change database credentials"), error, MessageBox::Ok);
            return false;
        }
    } else if (widget->visiblePage() == KeyComponentWidget::LeaveOrRemove) {
        Q_ASSERT(oldKey);
        newKey->addChallengeResponseKey(oldKey);
    }
    return true;
}

void DatabaseSettingsWidgetDatabaseKey::markDirty()
{
    m_isDirty = true;
}
