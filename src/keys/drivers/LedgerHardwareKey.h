#ifndef KEEPASSX_LEDGER_HARDWARE_H
#define KEEPASSX_LEDGER_HARDWARE_H

#include <QFuture>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QSharedPointer>
#include <QVector>

#include "config-keepassx.h"

namespace kpl
{
    class KPL;
    class LedgerDevice;
}

struct LedgerKeyIdentifier
{
    QSharedPointer<kpl::LedgerDevice> Dev;
    QString Name;
};

Q_DECLARE_OPAQUE_POINTER(kpl::LedgerDevice*);
Q_DECLARE_METATYPE(QSharedPointer<kpl::LedgerDevice>);

/**
 * Singleton class to manage the interface to hardware Ledger key(s)
 */
class LedgerHardwareKey : public QObject
{
    Q_OBJECT

public:
    enum DetectResult
    {
        Found = 0,
        NotFound = -1,
        ProtocolMismatch = -2
    };

    ~LedgerHardwareKey() override;
    static LedgerHardwareKey& instance();

    QFuture<bool> findDevices();
    QList<LedgerKeyIdentifier> foundDevices();

    bool fromDeviceSlot(kpl::LedgerDevice& Dev, unsigned Slot, uint8_t* Key, const size_t KeyLen, QString& Err);
    bool
    fromDeviceDeriveName(kpl::LedgerDevice& Dev, QString const& Name, uint8_t* Key, const size_t KeyLen, QString& Err);
    bool getValidKeySlots(kpl::LedgerDevice& Dev, QVector<uint8_t>& Ret, QString& Err);

    static size_t maxNameSize();

signals:
    void detectComplete(bool found);
    void userInteractionRequest();
    void userInteractionDone(bool Accepted, QString Err);

private:
    explicit LedgerHardwareKey();
    void fillValidKeySlots();

#ifdef WITH_XC_LEDGER
    QMutex Mutex_;
    QList<QSharedPointer<kpl::LedgerDevice>> Devices_;
#endif
};

#endif // KEEPASSX_YUBIKEY_H
