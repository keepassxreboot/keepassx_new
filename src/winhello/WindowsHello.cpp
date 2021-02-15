#include <QLoggingCategory>
#include <QMessageBox>
#include <QtWidgets/QApplication>

#include "gui/MessageBox.h"
#include "WindowsHello.h"

QLoggingCategory winhello("WindowsHello");

QString formatErrorCode(const std::error_code& errc)
{
    return "error_code=0x" + QString::number((DWORD)errc.value(), 16).toUpper();
}

bool WindowsHello::isAvailable()
{
    return WinHelloKeyManager::isAvailable();
}

WindowsHello::WindowsHello(QWidget* parent) : QWidget(parent)
{
    if(parent) {
        m_keymgr.setParentWindowHandle((HWND)parent->effectiveWinId());
    }
}

bool WindowsHello::storeKey(QStringView dbPath, const QByteArray& key)
{
    try {
        m_keymgr.storeKey(dbPath, key);
        return true;
    }
    catch(const WinHelloKeyManagerUserCanceled&) {
        qCDebug(winhello) << "User canceled authentication while trying to store the key";
        return false;
    }
    catch(const WinHelloKeyManagerError& e) {
        qCCritical(winhello) << "An error has occored while storing key: " << e.what() << " " << formatErrorCode(e.code());
        handleKeyStorageError(e);
        return false;
    }
    catch(const std::exception& e) {
        qCCritical(winhello) << "An exception was encountered while storing key: " << e.what();
        showResetDialog(tr(
            "An error has occored while trying to store key!\n"
            "Do you want to reset storage?"
        ));
    }
    return false;
}

QSharedPointer<QByteArray> WindowsHello::getKey(QStringView dbPath) const
{
    try {
        return QSharedPointer<QByteArray>::create(
            m_keymgr.getKey(dbPath)
        );
    }
    catch(const WinHelloKeyManagerUserCanceled&) {
        qCDebug(winhello) << "User canceled authentication while trying to retrieve key";
    }
    catch(const WinHelloKeyManagerError& e) {
        qCCritical(winhello) << "An error has occored while retrieving key: " << e.what() << " " << formatErrorCode(e.code());
        handleKeyStorageError(e);
    }
    catch(const std::exception& e) {
        qCCritical(winhello) << "An exception was encountered while trying to retrieve key: " << e.what();
        showResetDialog(tr(
            "An error has occored while trying to retrieve key!\n"
            "Do you want to reset storage?"
        ));
    }
    return nullptr;
}

void WindowsHello::removeKey(QStringView dbPath)
{
    try {
        m_keymgr.removeKey(dbPath);
    }
    catch(const std::exception& e) {
        qCCritical(winhello) << "An exception was encountered while trying to remove key: " << e.what();
        MessageBox::warning(
            const_cast<WindowsHello*>(this),
            tr("Windows Hello error"),
            tr("Failed to remove stored key!"),
            MessageBox::Close
        );
    }
}

bool WindowsHello::containsKey(QStringView dbPath)
{
    return WinHelloKeyManager::contains(dbPath);
}

void WindowsHello::handleKeyStorageError(const WinHelloKeyManagerError& e) const
{
    QString errorMsg;
    switch(e.code().value()){
    case NTE_PERM:
    case NTE_INVALID_HANDLE:    // can be thrown when persistent encryption key doesn't exist
    case NTE_INVALID_PARAMETER: // can be thrown when persistent encryption key was changed
        errorMsg = tr(
            "Hmm encryption key is corrupted or missing?!\n"
            "It is adviced to reset storage and re-import all database keys."
        );
        break;
    case NTE_BAD_KEY_STATE:
        errorMsg = tr(
            "Integrity of encryption key has failed.\n"
            "It might be caused by a spoofing attack.\n"
            "It is adviced to reset storage and re-import all database keys."
        );
        break;
    }

    if (!errorMsg.isEmpty()) {
        showResetDialog(errorMsg);
    }
}

void WindowsHello::showResetDialog(const QString& errorMsg) const
{
    auto selectedButton = MessageBox::critical(
        const_cast<WindowsHello*>(this),
        tr("Windows Hello Error"),
        errorMsg,
        MessageBox::Reset | MessageBox::Close
    );
    if (selectedButton == MessageBox::Reset) {
        qCInfo(winhello) << "Resetting key manager";
        reset();
    }
}

bool WindowsHello::reset() const
{
    try {
        m_keymgr.clear();
        return true;
    }
    catch(const WinHelloKeyManagerError& e) {
        qCCritical(winhello) << "An error has occored while resetting key storage: " << e.what() << " " << formatErrorCode(e.code());
    }
    catch(const std::exception& e) {
        qCCritical(winhello) << "An exception was encountered while resetting key storage: " << e.what();
    }

    MessageBox::critical(
        const_cast<WindowsHello*>(this),
        tr("Windows Hello error"),
        tr("Failed to reset Windows Hello key storage!"),
        MessageBox::Close
    );
    return false;
}
