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

#include "YubiKeyEditWidget.h"
#include "ui_YubiKeyEditWidget.h"

#include "config-keepassx.h"
#include "core/AsyncTask.h"
#include "gui/MainWindow.h"
#include "gui/MessageBox.h"
#include "keys/CompositeKey.h"
#include "keys/YkChallengeResponseKey.h"

#include "keys/LedgerKey.h"
#include "keys/drivers/LedgerHardwareKey.h"
#define LEDGER_COMBO_TYPE_NAME_IDX 0
#define LEDGER_COMBO_TYPE_SLOT_IDX 1

#include <pthread.h>

YubiKeyEditWidget::YubiKeyEditWidget(QWidget* parent)
    : KeyComponentWidget(parent)
    , m_compUi(new Ui::YubiKeyEditWidget())
    , m_msgBox(new QMessageBox(QMessageBox::Information, "Ledger Key", "", QMessageBox::NoButton, this))

{
    m_msgBox->setStandardButtons(QMessageBox::StandardButton());
    setComponentName(tr("YubiKey Challenge-Response / Ledger hardware key"));
    setComponentDescription(
        tr("<p>If you own a <a href=\"https://www.yubico.com/\">YubiKey</a>, you can use it "
           "for additional security.</p><p>The YubiKey requires one of its slots to be programmed as "
           "<a href=\"https://www.yubico.com/products/services-software/personalization-tools/challenge-response/\">"
           "HMAC-SHA1 Challenge-Response</a>.</p><p>TODO ledger</p>"));

    connect(YubiKey::instance(),
            SIGNAL(detectComplete(bool)),
            this,
            SLOT(hardwareKeyResponse(bool)),
            static_cast<Qt::ConnectionType>(Qt::QueuedConnection | Qt::UniqueConnection));

#ifdef WITH_XC_LEDGER
    connect(&LedgerHardwareKey::instance(),
            SIGNAL(detectComplete(bool)),
            this,
            SLOT(hardwareKeyResponse(bool)),
            static_cast<Qt::ConnectionType>(Qt::QueuedConnection | Qt::UniqueConnection));

    connect(
        &LedgerHardwareKey::instance(),
        &LedgerHardwareKey::userInteractionRequest,
        this,
        [this] {
            if (m_compEditWidget) {
                m_msgBox->setText(tr("Please accept accessing the database key on your Ledger device."));
                m_msgBox->show();
            }
        },
        Qt::UniqueConnection);
    connect(
        &LedgerHardwareKey::instance(),
        &LedgerHardwareKey::userInteractionDone,
        this,
        [this](bool, QString) { m_msgBox->hide(); },
        Qt::UniqueConnection);

#endif
}

YubiKeyEditWidget::~YubiKeyEditWidget()
{
}

bool YubiKeyEditWidget::addToCompositeKey(QSharedPointer<CompositeKey> key)
{
    if (!m_isDetected || !m_compEditWidget) {
        return false;
    }

    int selectionIndex = m_compUi->comboChallengeResponse->currentIndex();
    QVariant varSlot = m_compUi->comboChallengeResponse->itemData(selectionIndex);
#ifdef WITH_XC_YUBIKEY
    if (varSlot.canConvert<YubiKeySlot>()) {
        auto slot = varSlot.value<YubiKeySlot>();
        key->addChallengeResponseKey(QSharedPointer<YkChallengeResponseKey>::create(slot));
        return true;
    }
#endif
#ifdef WITH_XC_LEDGER
    if (varSlot.canConvert<QSharedPointer<kpl::LedgerDevice>>()) {
        if (!m_ledgerKey) {
            return false;
        }
        key->addKey(m_ledgerKey);
        m_ledgerKey.clear();
        return true;
    }
#endif
    return false;
}

bool YubiKeyEditWidget::validate(QString& errorMessage) const
{
    if (!m_isDetected) {
        errorMessage = tr("Could not find any hardware keys!");
        return false;
    }

    // Perform a test challenge response
    int selectionIndex = m_compUi->comboChallengeResponse->currentIndex();
    QVariant varSlot = m_compUi->comboChallengeResponse->itemData(selectionIndex);
#ifdef WITH_XC_YUBIKEY
    if (varSlot.canConvert<YubiKeySlot>()) {
        auto slot = varSlot.value<YubiKeySlot>();
        bool valid = AsyncTask::runAndWaitForFuture([&slot] { return YubiKey::instance()->testChallenge(slot); });
        if (!valid) {
            errorMessage = tr("Selected hardware key slot does not support challenge-response!");
        }
        return valid;
    }
#endif
#ifdef WITH_XC_LEDGER
    m_ledgerKey.clear();
    if (varSlot.canConvert<QSharedPointer<kpl::LedgerDevice>>()) {
        const auto Type = m_compUi->ledgerKeyTypeCombo->currentIndex();
        QString Err;
        auto Dev = varSlot.value<QSharedPointer<kpl::LedgerDevice>>();
        if (Type == LEDGER_COMBO_TYPE_NAME_IDX) {
            QString SKey = m_compUi->ledgerKeyLineEdit->text();
            if (SKey.isEmpty()) {
                errorMessage = tr("Please enter a key name.");
                return false;
            }
            m_ledgerKey = LedgerKey::fromDeviceDeriveName(*Dev, SKey, Err);
        } else {
            const int idx = m_compUi->ledgerKeySlotCombo->currentIndex();
            if (idx == 0) {
                errorMessage = tr("Please select a valid slot.");
                return false;
            }
            m_ledgerKey = LedgerKey::fromDeviceSlot(*Dev, idx, Err);
        }
        if (!m_ledgerKey) {
            errorMessage = tr("Ledger key error: ") + Err;
            return false;
        }
        return true;
    }
#endif
    return false;
}

QWidget* YubiKeyEditWidget::componentEditWidget()
{
    m_compEditWidget = new QWidget();
    m_compUi->setupUi(m_compEditWidget);

    QSizePolicy sp = m_compUi->yubikeyProgress->sizePolicy();
    sp.setRetainSizeWhenHidden(true);
    m_compUi->yubikeyProgress->setSizePolicy(sp);
    m_compUi->yubikeyProgress->setVisible(false);

    showLedgerWidgets(false);

    connect(m_compUi->buttonRedetectYubikey, SIGNAL(clicked()), SLOT(pollHardwareKey()));

#ifdef WITH_XC_LEDGER
    connect(m_compUi->ledgerKeyTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        updateLedgerWidgets();
    });
    connect(m_compUi->comboChallengeResponse, SIGNAL(currentIndexChanged(int)), SLOT(hardwareKeySelected(int)));

    m_compUi->ledgerKeyLineEdit->setMaxLength(LedgerHardwareKey::maxNameSize());
#endif

    pollHardwareKey();

    return m_compEditWidget;
}

void YubiKeyEditWidget::initComponentEditWidget(QWidget* widget)
{
    Q_UNUSED(widget);
    Q_ASSERT(m_compEditWidget);
    m_compUi->comboChallengeResponse->setFocus();
}

void YubiKeyEditWidget::pollHardwareKey()
{
    m_isDetected = false;
    if (!m_compEditWidget) {
        return;
    }

    m_compUi->comboChallengeResponse->clear();
    m_compUi->comboChallengeResponse->addItem(tr("Detecting hardware keys…"));
    m_compUi->buttonRedetectYubikey->setEnabled(false);
    m_compUi->comboChallengeResponse->setEnabled(false);
    m_compUi->yubikeyProgress->setVisible(true);

    YubiKey::instance()->findValidKeys();
    LedgerHardwareKey::instance().findDevices();
}

void YubiKeyEditWidget::hardwareKeyResponse(bool found)
{
    Q_UNUSED(found);
    if (!m_compEditWidget) {
        return;
    }

    // Remove this connection in order not to have one call to hardwareKeySelected per item added.
    disconnect(
        m_compUi->comboChallengeResponse, SIGNAL(currentIndexChanged(int)), this, SLOT(hardwareKeySelected(int)));

    m_compUi->comboChallengeResponse->clear();

    m_compUi->buttonRedetectYubikey->setEnabled(true);
    m_compUi->yubikeyProgress->setVisible(false);

    const auto YubiKeys = YubiKey::instance()->foundKeys();
    const auto LedgerKeys = LedgerHardwareKey::instance().foundDevices();
    if (YubiKeys.empty() && LedgerKeys.empty()) {
        m_compUi->comboChallengeResponse->addItem(tr("No hardware keys detected"));
        m_isDetected = false;
        return;
    }

    for (auto& slot : YubiKeys) {
        // add detected YubiKey to combo box and encode blocking mode in LSB, slot number in second LSB
        m_compUi->comboChallengeResponse->addItem(YubiKey::instance()->getDisplayName(slot), QVariant::fromValue(slot));
    }

    if (!LedgerKeys.empty() && !YubiKeys.empty()) {
        m_compUi->comboChallengeResponse->insertSeparator(m_compUi->comboChallengeResponse->count());
    }
    for (auto const& key : LedgerKeys) {
        m_compUi->comboChallengeResponse->addItem(key.Name, QVariant::fromValue(key.Dev));
    }

    m_isDetected = true;
    m_compUi->comboChallengeResponse->setEnabled(true);

    connect(m_compUi->comboChallengeResponse, SIGNAL(currentIndexChanged(int)), this, SLOT(hardwareKeySelected(int)));
    m_compUi->comboChallengeResponse->setCurrentIndex(0);
    hardwareKeySelected(0);
}

void YubiKeyEditWidget::showLedgerWidgets(bool show)
{
    if (!show) {
        m_compUi->ledgerKeyTypeCombo->setVisible(false);
        m_compUi->ledgerKeyLineEdit->setVisible(false);
        m_compUi->ledgerKeySlotCombo->setVisible(false);
    } else {
        m_compUi->ledgerKeyTypeCombo->setVisible(true);
        updateLedgerWidgets();
    }
}

void YubiKeyEditWidget::updateLedgerWidgets()
{
#ifdef WITH_XC_LEDGER
    const int idx = m_compUi->ledgerKeyTypeCombo->currentIndex();
    switch (idx) {
    case LEDGER_COMBO_TYPE_SLOT_IDX:
        m_compUi->ledgerKeyLineEdit->setVisible(false);
        m_compUi->ledgerKeySlotCombo->setVisible(true);
        break;
    case LEDGER_COMBO_TYPE_NAME_IDX:
        m_compUi->ledgerKeyLineEdit->setVisible(true);
        m_compUi->ledgerKeySlotCombo->setVisible(false);
        break;
    default:
        break;
    }
#endif
}

void YubiKeyEditWidget::hardwareKeySelected(int index)
{
    if (!m_compEditWidget) {
        return;
    }
#ifdef WITH_XC_LEDGER
    QVariant Data = m_compUi->comboChallengeResponse->itemData(index);
    if (!Data.canConvert<QSharedPointer<kpl::LedgerDevice>>()) {
        showLedgerWidgets(false);
        return;
    }
    // Get slots
    auto Dev = Data.value<QSharedPointer<kpl::LedgerDevice>>();
    QVector<uint8_t> Slots;
    QString Err;
    m_compUi->ledgerKeySlotCombo->setEnabled(false);
    const bool Success = AsyncTask::runAndWaitForFuture(
        [&] { return LedgerHardwareKey::instance().getValidKeySlots(*Dev, Slots, Err); });
    if (!Success) {
        QMessageBox::warning(this, tr("Ledger key error"), tr("Unable to get valid slots: ") + Err);
        return;
    }
    m_compUi->ledgerKeySlotCombo->setEnabled(true);
    QComboBox* slotCombo = m_compUi->ledgerKeySlotCombo;
    slotCombo->clear();
    slotCombo->addItem(tr("Select a slot..."));
    for (int Slot : Slots) {
        slotCombo->addItem(QString("# %1").arg(Slot), QVariant{Slot});
    }
    showLedgerWidgets(true);
#else
    Q_UNUSED(index);
#endif
}
