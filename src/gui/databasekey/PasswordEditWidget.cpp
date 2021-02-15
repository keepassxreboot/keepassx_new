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

#include "PasswordEditWidget.h"
#include "ui_PasswordEditWidget.h"

#include "core/Resources.h"
#include "gui/PasswordGeneratorWidget.h"
#include "keys/CompositeKey.h"
#include "keys/PasswordKey.h"

#include <QDialog>

#include "config-keepassx.h"

#ifdef WITH_XC_WINDOWSHELLO
#include "winhello/WindowsHello.h"
#endif

PasswordEditWidget::PasswordEditWidget(QWidget* parent)
    : KeyComponentWidget(parent)
    , m_compUi(new Ui::PasswordEditWidget())
{
    setComponentName(tr("Password"));
    setComponentDescription(tr("<p>A password is the primary method for securing your database.</p>"
                               "<p>Good passwords are long and unique. KeePassXC can generate one for you.</p>"));

    m_checkOsStoreKey = new QCheckBox(this);
    m_checkOsStoreKey->setVisible(false);
    this->findChild<QWidget*>("changeOrRemovePage")->layout()->addWidget(m_checkOsStoreKey);

#ifdef WITH_XC_WINDOWSHELLO
    if (WindowsHello::isAvailable()) {
       m_checkOsStoreKey->setVisible(true);
       m_checkOsStoreKey->setText(tr("Use Windows Hello to store and retrieve database password"));
    }
#endif
}

PasswordEditWidget::~PasswordEditWidget()
{
}

bool PasswordEditWidget::addToCompositeKey(QSharedPointer<CompositeKey> key)
{
    QString pw = m_compUi->enterPasswordEdit->text();
    if (!pw.isEmpty()) {
        key->addKey(QSharedPointer<PasswordKey>::create(pw));
        return true;
    }
    return false;
}

/**
 * @param visible changed password visibility state
 */
void PasswordEditWidget::setPasswordVisible(bool visible)
{
    m_compUi->enterPasswordEdit->setShowPassword(visible);
}

/**
 * @return password visibility state
 */
bool PasswordEditWidget::isPasswordVisible() const
{
    return m_compUi->enterPasswordEdit->isPasswordVisible();
}

bool PasswordEditWidget::isEmpty() const
{
    return (visiblePage() == Page::Edit) && m_compUi->enterPasswordEdit->text().isEmpty();
}

QWidget* PasswordEditWidget::componentEditWidget()
{
    m_compEditWidget = new QWidget(this);
    m_compUi->setupUi(m_compEditWidget);
    m_compUi->enterPasswordEdit->enablePasswordGenerator();
    m_compUi->enterPasswordEdit->setRepeatPartner(m_compUi->repeatPasswordEdit);

#ifdef WITH_XC_WINDOWSHELLO
    if (WindowsHello::isAvailable()) {
        auto checkOsStoreKey = new QCheckBox(m_checkOsStoreKey->text(), m_compEditWidget);
        connect(checkOsStoreKey, &QCheckBox::toggled, [this](bool checked){
            m_checkOsStoreKey->setChecked(checked);
        });

        checkOsStoreKey->setChecked(true); // Set checked by default
        m_compUi->formLayout->addWidget(checkOsStoreKey);
    }
#endif

    return m_compEditWidget;
}

void PasswordEditWidget::initComponentEditWidget(QWidget* widget)
{
    Q_UNUSED(widget);
    Q_ASSERT(m_compEditWidget);
    m_compUi->enterPasswordEdit->setFocus();
}

void PasswordEditWidget::hideEvent(QHideEvent* event)
{
    if (!isVisible() && m_compUi->enterPasswordEdit) {
        m_compUi->enterPasswordEdit->setText("");
        m_compUi->repeatPasswordEdit->setText("");
    }

    QWidget::hideEvent(event);
}

bool PasswordEditWidget::validate(QString& errorMessage) const
{
    if (m_compUi->enterPasswordEdit->text() != m_compUi->repeatPasswordEdit->text()) {
        errorMessage = tr("Passwords do not match.");
        return false;
    }

    return true;
}

void PasswordEditWidget::setPassword(const QString& password)
{
    Q_ASSERT(m_compEditWidget);

    m_compUi->enterPasswordEdit->setText(password);
    m_compUi->repeatPasswordEdit->setText(password);
}
