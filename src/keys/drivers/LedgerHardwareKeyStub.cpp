#include "LedgerHardwareKey.h"
#include "core/AsyncTask.h"

#include <memory>

static std::unique_ptr<LedgerHardwareKey> Instance_;

LedgerHardwareKey::LedgerHardwareKey() = default;
LedgerHardwareKey::~LedgerHardwareKey() = default;

LedgerHardwareKey& LedgerHardwareKey::instance()
{
    if (!Instance_) {
        Instance_.reset(new LedgerHardwareKey{});
    }
    return *Instance_;
}

QFuture<bool> LedgerHardwareKey::findDevices()
{
    return QtConcurrent::run([] { return false; });
}

QList<LedgerKeyIdentifier> LedgerHardwareKey::foundDevices()
{
    return {};
}

bool LedgerHardwareKey::fromDeviceSlot(kpl::LedgerDevice& Dev,
                                       unsigned Slot,
                                       uint8_t* Key,
                                       const size_t KeyLen,
                                       QString& Err)
{
    Q_UNUSED(Dev);
    Q_UNUSED(Slot);
    Q_UNUSED(Key);
    Q_UNUSED(KeyLen);
    Q_UNUSED(Err);
    return false;
}

bool LedgerHardwareKey::fromDeviceDeriveName(kpl::LedgerDevice& Dev,
                                             QString const& Name,
                                             uint8_t* Key,
                                             const size_t KeyLen,
                                             QString& Err)
{
    Q_UNUSED(Dev);
    Q_UNUSED(Name);
    Q_UNUSED(Key);
    Q_UNUSED(KeyLen);
    Q_UNUSED(Err);
    return false;
}

bool LedgerHardwareKey::getValidKeySlots(kpl::LedgerDevice& Dev, QVector<uint8_t>& Ret, QString& Err)
{
    Q_UNUSED(Dev);
    Q_UNUSED(Err);
    Ret.clear();
    return false;
}

size_t LedgerHardwareKey::maxNameSize()
{
    return 0;
}
