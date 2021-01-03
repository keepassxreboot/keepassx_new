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

#ifndef KEEPASSXC_YUBIKEYEDITWIDGET_H
#define KEEPASSXC_YUBIKEYEDITWIDGET_H

#include "KeyComponentWidget.h"
#include <QMessageBox>
#include <QMutex>
#include <QPointer>
#include <QSharedPointer>

namespace Ui
{
    class YubiKeyEditWidget;
}

class LedgerKey;
class YkChallengeResponseKey;

class YubiKeyEditWidget : public KeyComponentWidget
{
    Q_OBJECT

public:
    explicit YubiKeyEditWidget(QWidget* parent = nullptr);
    Q_DISABLE_COPY(YubiKeyEditWidget);
    ~YubiKeyEditWidget() override;

    bool addToCompositeKey(QSharedPointer<CompositeKey> key) override;
    bool validate(QString& errorMessage) const override;

protected:
    QWidget* componentEditWidget() override;
    void initComponentEditWidget(QWidget* widget) override;

private slots:
    void hardwareKeyResponse(bool found);
    void pollHardwareKey();
    void hardwareKeySelected(int index);

private:
    void updateLedgerWidgets();
    void showLedgerWidgets(bool show);

    const QScopedPointer<Ui::YubiKeyEditWidget> m_compUi;
    QPointer<QWidget> m_compEditWidget;
    QMutex m_mutexList;
    QMessageBox* m_msgBox;
    mutable QSharedPointer<LedgerKey> m_ledgerKey;
    bool m_isDetected = false;
};

#endif // KEEPASSXC_YUBIKEYEDITWIDGET_H
