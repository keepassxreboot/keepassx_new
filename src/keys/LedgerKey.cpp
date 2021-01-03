#include <QtConcurrent>

#include "core/AsyncTask.h"
#include "core/Tools.h"

#include <algorithm>
#include <cstring>

#include <sodium.h>

#include "LedgerKey.h"
#include "drivers/LedgerHardwareKey.h"

QUuid LedgerKey::UUID("4f998a1e-10a3-476c-81f1-77d1faafc59c");

LedgerKey::LedgerKey()
    : Key(UUID)
{
}

LedgerKey::~LedgerKey()
{
    sodium_memzero(RawKey_, sizeof(RawKey_));
}

QByteArray LedgerKey::rawKey() const
{
    return QByteArray::fromRawData(reinterpret_cast<const char*>(RawKey_), sizeof(RawKey_));
}

QSharedPointer<LedgerKey> LedgerKey::fromDeviceSlot(kpl::LedgerDevice& Dev, unsigned Slot, QString& Err)
{
    auto Ret = QSharedPointer<LedgerKey>::create();
    if (!Ret) {
        Err = QObject::tr("out of memory");
        return Ret;
    }
    bool Res = AsyncTask::runAndWaitForFuture([&] {
        return LedgerHardwareKey::instance().fromDeviceSlot(Dev, Slot, Ret->RawKey_, sizeof(Ret->RawKey_), Err);
    });
    if (!Res) {
        return {};
    }
    return Ret;
}

QSharedPointer<LedgerKey> LedgerKey::fromDeviceDeriveName(kpl::LedgerDevice& Dev, QString const& Name, QString& Err)
{
    auto Key = QSharedPointer<LedgerKey>::create();
    if (!Key) {
        Err = QObject::tr("out of memory");
        return Key;
    }
    bool Res = AsyncTask::runAndWaitForFuture([&] {
        return LedgerHardwareKey::instance().fromDeviceDeriveName(Dev, Name, Key->RawKey_, sizeof(Key->RawKey_), Err);
    });
    if (!Res) {
        return {};
    }
    return Key;
}
