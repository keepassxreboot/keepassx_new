/*
 *  Copyright (C) 2014 Kyle Manna <kyle@kylemanna.com>
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

#include "YubiKey.h"

#include "core/Tools.h"
#include "crypto/Random.h"

#include "thirdparty/ykcore/ykcore.h"
#include "thirdparty/ykcore/ykdef.h"
#include "thirdparty/ykcore/ykstatus.h"

#include <QtConcurrent>

namespace
{
    constexpr int MAX_KEYS = 4;

    YK_KEY* openKey(int index)
    {
        static const int vids[] = {YUBICO_VID, ONLYKEY_VID};
        static const int pids[] = {YUBIKEY_PID,
                                   NEO_OTP_PID,
                                   NEO_OTP_CCID_PID,
                                   NEO_OTP_U2F_PID,
                                   NEO_OTP_U2F_CCID_PID,
                                   YK4_OTP_PID,
                                   YK4_OTP_U2F_PID,
                                   YK4_OTP_CCID_PID,
                                   YK4_OTP_U2F_CCID_PID,
                                   PLUS_U2F_OTP_PID,
                                   ONLYKEY_PID};

        return yk_open_key_vid_pid(vids, sizeof(vids) / sizeof(vids[0]), pids, sizeof(pids) / sizeof(pids[0]), index);
    }

    void closeKey(YK_KEY* key)
    {
        yk_close_key(key);
    }

    unsigned int getSerial(YK_KEY* key)
    {
        unsigned int serial;
        yk_get_serial(key, 1, 0, &serial);
        return serial;
    }

    YK_KEY* openKeySerial(unsigned int serial)
    {
        for (int i = 0; i < MAX_KEYS; ++i) {
            auto* yk_key = openKey(i);
            if (yk_key) {
                // If the provided serial number is 0, or the key matches the serial, return it
                if (serial == 0 || getSerial(yk_key) == serial) {
                    return yk_key;
                }
                closeKey(yk_key);
            } else if (yk_errno == YK_ENOKEY) {
                // No more connected keys
                break;
            } else if (yk_errno == YK_EUSBERR) {
                qWarning("Hardware key USB error: %s", yk_usb_strerror());
            } else {
                qWarning("Hardware key error: %s", yk_strerror(yk_errno));
            }
        }
        return nullptr;
    }
} // namespace

YubiKey::YubiKey()
    : m_mutex(QMutex::Recursive)
{
    m_interactionTimer.setSingleShot(true);
    m_interactionTimer.setInterval(300);

    if (!yk_init()) {
        qDebug("YubiKey: Failed to initialize USB interface.");
    } else {
        m_initialized = true;
        // clang-format off
        connect(&m_interactionTimer, SIGNAL(timeout()), this, SIGNAL(userInteractionRequest()));
        connect(this, &YubiKey::challengeStarted, this, [this] { m_interactionTimer.start(); }, Qt::QueuedConnection);
        connect(this, &YubiKey::challengeCompleted, this, [this] { m_interactionTimer.stop(); }, Qt::QueuedConnection);
        // clang-format on
    }
}

YubiKey::~YubiKey()
{
    yk_release();
}

YubiKey* YubiKey::m_instance(Q_NULLPTR);

YubiKey* YubiKey::instance()
{
    if (!m_instance) {
        m_instance = new YubiKey();
    }

    return m_instance;
}

bool YubiKey::isInitialized()
{
    return m_initialized;
}

void YubiKey::findValidKeys()
{
    m_error.clear();
    if (!isInitialized()) {
        return;
    }

    QtConcurrent::run([this] {
        if (!m_mutex.tryLock(1000)) {
            emit detectComplete(false);
            return;
        }

        // Remove all known keys
        m_foundKeys.clear();

        // Try to detect up to 4 connected hardware keys
        for (int i = 0; i < MAX_KEYS; ++i) {
            auto yk_key = openKey(i);
            if (yk_key) {
                auto serial = getSerial(yk_key);
                if (serial == 0) {
                    closeKey(yk_key);
                    continue;
                }

                auto st = ykds_alloc();
                yk_get_status(yk_key, st);
                int vid, pid;
                yk_get_key_vid_pid(yk_key, &vid, &pid);

                auto vendor = vid == 0x1d50 ? QStringLiteral("OnlyKey") : QStringLiteral("YubiKey");

                bool wouldBlock;
                QList<QPair<int, QString>> ykSlots;
                for (int slot = 1; slot <= 2; ++slot) {
                    auto config = (slot == 1 ? CONFIG1_VALID : CONFIG2_VALID);
                    if (!(ykds_touch_level(st) & config)) {
                        // Slot is not configured
                        continue;
                    }
                    // Don't actually challenge a YubiKey Neo or below, they always require button press
                    // if it is enabled for the slot resulting in failed detection
                    if (pid <= NEO_OTP_U2F_CCID_PID) {
                        auto display = tr("%1 [%2] Configured Slot - %3")
                                           .arg(vendor, QString::number(serial), QString::number(slot));
                        ykSlots.append({slot, display});
                    } else if (performTestChallenge(yk_key, slot, &wouldBlock)) {
                        auto display =
                            tr("%1 [%2] Challenge-Response - Slot %3 - %4")
                                .arg(vendor,
                                     QString::number(serial),
                                     QString::number(slot),
                                     wouldBlock ? tr("Press", "Challenge-Response Key interaction request")
                                                : tr("Passive", "Challenge-Response Key no interaction required"));
                        ykSlots.append({slot, display});
                    }
                }

                if (!ykSlots.isEmpty()) {
                    m_foundKeys.insert(serial, ykSlots);
                }

                ykds_free(st);
                closeKey(yk_key);

                Tools::wait(100);
            } else if (yk_errno == YK_ENOKEY) {
                // No more keys are connected
                break;
            } else if (yk_errno == YK_EUSBERR) {
                qWarning("Hardware key USB error: %s", yk_usb_strerror());
            } else {
                qWarning("Hardware key error: %s", yk_strerror(yk_errno));
            }
        }

        m_mutex.unlock();
        emit detectComplete(!m_foundKeys.isEmpty());
    });
}

QList<YubiKeySlot> YubiKey::foundKeys()
{
    QList<YubiKeySlot> keys;
    for (auto serial : m_foundKeys.uniqueKeys()) {
        for (auto key : m_foundKeys.value(serial)) {
            keys.append({serial, key.first});
        }
    }
    return keys;
}

QString YubiKey::getDisplayName(YubiKeySlot slot)
{
    for (auto key : m_foundKeys.value(slot.first)) {
        if (slot.second == key.first) {
            return key.second;
        }
    }
    return tr("%1 Invalid slot specified - %2").arg(QString::number(slot.first), QString::number(slot.second));
}

QString YubiKey::errorMessage()
{
    return m_error;
}

/**
 * Issue a test challenge to the specified slot to determine if challenge
 * response is properly configured.
 *
 * @param slot YubiKey configuration slot
 * @param wouldBlock return if the operation requires user input
 * @return whether the challenge succeeded
 */
bool YubiKey::testChallenge(YubiKeySlot slot, bool* wouldBlock)
{
    bool ret = false;
    auto* yk_key = openKeySerial(slot.first);
    if (yk_key) {
        ret = performTestChallenge(yk_key, slot.second, wouldBlock);
    }
    return ret;
}

bool YubiKey::performTestChallenge(void* key, int slot, bool* wouldBlock)
{
    auto chall = randomGen()->randomArray(1);
    Botan::secure_vector<char> resp;
    auto ret = performChallenge(static_cast<YK_KEY*>(key), slot, false, chall, resp);
    if (ret == SUCCESS || ret == WOULDBLOCK) {
        if (wouldBlock) {
            *wouldBlock = ret == WOULDBLOCK;
        }
        return true;
    }
    return false;
}

/**
 * Issue a challenge to the specified slot
 * This operation could block if the YubiKey requires a touch to trigger.
 *
 * @param slot YubiKey configuration slot
 * @param challenge challenge input to YubiKey
 * @param response response output from YubiKey
 * @return challenge result
 */
YubiKey::ChallengeResult
YubiKey::challenge(YubiKeySlot slot, const QByteArray& challenge, Botan::secure_vector<char>& response)
{
    m_error.clear();
    if (!m_initialized) {
        m_error = tr("The YubiKey interface has not been initialized.");
        return ERROR;
    }

    // Try to grab a lock for 1 second, fail out if not possible
    if (!m_mutex.tryLock(1000)) {
        m_error = tr("Hardware key is currently in use.");
        return ERROR;
    }

    auto* yk_key = openKeySerial(slot.first);
    if (!yk_key) {
        // Key with specified serial number is not connected
        m_error =
            tr("Could not find hardware key with serial number %1. Please plug it in to continue.").arg(slot.first);
        m_mutex.unlock();
        return ERROR;
    }

    emit challengeStarted();
    auto ret = performChallenge(yk_key, slot.second, true, challenge, response);

    closeKey(yk_key);
    emit challengeCompleted();
    m_mutex.unlock();

    return ret;
}

YubiKey::ChallengeResult YubiKey::performChallenge(void* key,
                                                   int slot,
                                                   bool mayBlock,
                                                   const QByteArray& challenge,
                                                   Botan::secure_vector<char>& response)
{
    m_error.clear();
    int yk_cmd = (slot == 1) ? SLOT_CHAL_HMAC1 : SLOT_CHAL_HMAC2;
    QByteArray paddedChallenge = challenge;

    // yk_challenge_response() insists on 64 bytes response buffer */
    response.clear();
    response.resize(64);

    /* The challenge sent to the yubikey should always be 64 bytes for
     * compatibility with all configurations.  Follow PKCS7 padding.
     *
     * There is some question whether or not 64 bytes fixed length
     * configurations even work, some docs say avoid it.
     */
    const int padLen = 64 - paddedChallenge.size();
    if (padLen > 0) {
        paddedChallenge.append(QByteArray(padLen, padLen));
    }

    const unsigned char* c;
    unsigned char* r;
    c = reinterpret_cast<const unsigned char*>(paddedChallenge.constData());
    r = reinterpret_cast<unsigned char*>(response.data());

    int ret = yk_challenge_response(
        static_cast<YK_KEY*>(key), yk_cmd, mayBlock, paddedChallenge.size(), c, response.size(), r);

    // actual HMAC-SHA1 response is only 20 bytes
    response.resize(20);

    if (!ret) {
        if (yk_errno == YK_EWOULDBLOCK) {
            return WOULDBLOCK;
        } else if (yk_errno) {
            if (yk_errno == YK_ETIMEOUT) {
                m_error = tr("Hardware key timed out waiting for user interaction.");
            } else if (yk_errno == YK_EUSBERR) {
                m_error = tr("A USB error occurred when accessing the hardware key: %1").arg(yk_usb_strerror());
            } else {
                m_error = tr("Failed to complete a challenge-response, the specific error was: %1")
                              .arg(yk_strerror(yk_errno));
            }

            return ERROR;
        }
    }

    return SUCCESS;
}
