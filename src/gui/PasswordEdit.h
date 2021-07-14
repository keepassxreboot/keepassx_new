/*
 *  Copyright (C) 2014 Felix Geyer <debfx@fobos.de>
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

#ifndef KEEPASSX_PASSWORDEDIT_H
#define KEEPASSX_PASSWORDEDIT_H

#include <QAction>
#include <QLineEdit>
#include <QPointer>

class QDialog;

class PasswordEdit : public QLineEdit
{
    Q_OBJECT

public:
    explicit PasswordEdit(QWidget* parent = nullptr);
    void enablePasswordGenerator();
    void setRepeatPartner(PasswordEdit* repeatEdit);
    bool isPasswordVisible() const;

public slots:
    void setShowPassword(bool show);
    void updateRepeatStatus();
    void doAutoTypePasswordEdit(const QString& password);

protected:
    bool event(QEvent* event) override;

signals:
    void capslockToggled(bool capslockOn);
    void doAutoTypePasswordEntry(const QString& password);

private slots:
    void autocompletePassword(const QString& password);
    void popupPasswordGenerator();
    void setParentPasswordEdit(PasswordEdit* parent);
    void checkCapslockState();

private:
    QPointer<QAction> m_errorAction;
    QPointer<QAction> m_correctAction;
    QPointer<QAction> m_toggleVisibleAction;
    QPointer<QAction> m_passwordGeneratorAction;
    QPointer<QAction> m_capslockAction;
    QPointer<PasswordEdit> m_repeatPasswordEdit;
    QPointer<PasswordEdit> m_parentPasswordEdit;
    bool m_capslockState = false;
};

#endif // KEEPASSX_PASSWORDEDIT_H
