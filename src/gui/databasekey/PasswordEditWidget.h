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

#ifndef KEEPASSXC_PASSWORDEDITWIDGET_H
#define KEEPASSXC_PASSWORDEDITWIDGET_H

#include "KeyComponentWidget.h"
#include <QCheckBox>
#include <QPointer>

namespace Ui
{
    class PasswordEditWidget;
}

class PasswordEditWidget : public KeyComponentWidget
{
    Q_OBJECT

public:
    explicit PasswordEditWidget(QWidget* parent = nullptr);
    Q_DISABLE_COPY(PasswordEditWidget);
    ~PasswordEditWidget() override;

    bool addToCompositeKey(QSharedPointer<CompositeKey> key) override;
    void setPasswordVisible(bool visible);
    bool isPasswordVisible() const;
    bool isEmpty() const;
    bool validate(QString& errorMessage) const override;

    QCheckBox* osStoreKey() {
        return m_checkOsStoreKey;
    }

protected:
    QWidget* componentEditWidget() override;
    void initComponentEditWidget(QWidget* widget) override;
    void hideEvent(QHideEvent* event) override;

private slots:
    void setPassword(const QString& password);

private:
    QCheckBox* m_checkOsStoreKey;
    const QScopedPointer<Ui::PasswordEditWidget> m_compUi;
    QPointer<QWidget> m_compEditWidget;
};

#endif // KEEPASSXC_PASSWORDEDITWIDGET_H
