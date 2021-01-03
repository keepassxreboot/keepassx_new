#include "LedgerHardwareKey.h"
#include "core/AsyncTask.h"

#include <QDebug>
#include <memory>

#include <kpl/errors.h>
#include <kpl/kpl.h>
#include <kpl/ledger_device.h>

static std::unique_ptr<LedgerHardwareKey> Instance_;
static constexpr unsigned OpTimeoutMS = 4000;

LedgerHardwareKey::LedgerHardwareKey()
    : Mutex_(QMutex::Recursive)
{
}

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
    return QtConcurrent::run([&] {
        if (!Mutex_.tryLock(1000)) {
            emit detectComplete(false);
            return false;
        }
        auto Devices = kpl::LedgerDevice::listDevices();
        Devices_.clear();
        Devices_.reserve(Devices.size());
        for (auto& D : Devices) {
            Devices_.push_back(QSharedPointer<kpl::LedgerDevice>{D.release()});
        }
        const bool Ret = !Devices_.empty();
        Mutex_.unlock();
        emit detectComplete(Ret);
        return Ret;
    });
}

QList<LedgerKeyIdentifier> LedgerHardwareKey::foundDevices()
{
    QList<LedgerKeyIdentifier> Ret;
    if (!Mutex_.tryLock(1000)) {
        return Ret;
    }
    Ret.reserve(Devices_.size());
    std::transform(Devices_.begin(), Devices_.end(), std::back_inserter(Ret), [](QSharedPointer<kpl::LedgerDevice>& D) {
        std::string const& name = D->name();
        return LedgerKeyIdentifier{D, QString::fromUtf8(name.c_str(), name.length())};
    });
    Mutex_.unlock();
    return Ret;
}

static QString protocolErrorMsg(uint8_t appProto, uint8_t libProto)
{
    QString Msg = QObject::tr("Ledger hardware key found, but protocol version mismatch: "
                              "device application supports version %1, current software supports version %2. ")
                      .arg(appProto)
                      .arg(libProto);
    if (appProto < libProto) {
        Msg += QObject::tr("You should update the application running on your device with Ledger Live.");
    } else {
        Msg += QObject::tr("You should update KeePassXC.");
    }
    return Msg;
}

static kpl::ErrorOr<kpl::KPL> getKPL(kpl::LedgerDevice& Dev, QString& Err)
{
    kpl::Version AppVer;
    auto EKPL = kpl::KPL::fromDevice(Dev, AppVer, OpTimeoutMS);
    if (!EKPL) {
        const auto errVal = EKPL.errorValue();
        if (errVal == kpl::Result::PROTOCOL_BAD_VERSION) {
            Err = protocolErrorMsg(AppVer.protocol(), kpl::Version::lib().protocol());
        } else {
            Err = QObject::tr(kpl::errorStr(errVal));
        }
    }
    return EKPL;
}

bool LedgerHardwareKey::fromDeviceSlot(kpl::LedgerDevice& Dev,
                                       unsigned Slot,
                                       uint8_t* Key,
                                       const size_t KeyLen,
                                       QString& Err)
{
    if (!Mutex_.tryLock(1000)) {
        return false;
    }
    auto EKPL = getKPL(Dev, Err);
    if (!EKPL) {
        Mutex_.unlock();
        return false;
    }
    emit userInteractionRequest();
    auto Res = EKPL->getKey(Slot, Key, KeyLen);
    const bool Ret = (Res == kpl::Result::SUCCESS);
    if (!Ret) {
        Err = kpl::errorStr(Res);
    }
    emit userInteractionDone(Ret, QString{Err});
    Mutex_.unlock();
    return Ret;
}

bool LedgerHardwareKey::fromDeviceDeriveName(kpl::LedgerDevice& Dev,
                                             QString const& Name,
                                             uint8_t* Key,
                                             const size_t KeyLen,
                                             QString& Err)
{
    if (!Mutex_.tryLock(1000)) {
        return false;
    }
    auto EKPL = getKPL(Dev, Err);
    if (!EKPL) {
        Mutex_.unlock();
        return false;
    }
    emit userInteractionRequest();
    QByteArray NameUtf8 = Name.toUtf8();
    auto Res = EKPL->getKeyFromName(NameUtf8.constData(), Key, KeyLen);
    const bool Ret = (Res == kpl::Result::SUCCESS);
    if (!Ret) {
        Err = tr(kpl::errorStr(Res));
    }
    emit userInteractionDone(Ret, QString{Err});
    Mutex_.unlock();
    return Ret;
}

bool LedgerHardwareKey::getValidKeySlots(kpl::LedgerDevice& Dev, QVector<uint8_t>& Ret, QString& Err)
{
    Ret.clear();
    if (!Mutex_.tryLock(1000)) {
        return false;
    }
    auto EKPL = getKPL(Dev, Err);
    if (!EKPL) {
        Mutex_.unlock();
        return false;
    }
    std::vector<uint8_t> Slots;
    auto Res = EKPL->getValidKeySlots(Slots, OpTimeoutMS);
    if (Res != kpl::Result::SUCCESS) {
        Err = tr(kpl::errorStr(Res));
        Mutex_.unlock();
        return false;
    }
    Mutex_.unlock();
    Ret.resize(Slots.size());
    std::copy(Slots.begin(), Slots.end(), Ret.begin());
    return true;
}

size_t LedgerHardwareKey::maxNameSize()
{
    return kpl::KPL::maxNameSize();
}
