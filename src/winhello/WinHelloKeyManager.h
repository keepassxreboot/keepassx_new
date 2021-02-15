#ifndef KEEPASSX_WINHELLOMANAGER_H
#define KEEPASSX_WINHELLOMANAGER_H
#include "core/osutils/winutils.h"

#include <QByteArray>
#include <QStringView>

#include <stdexcept>

struct WinHelloKeyManagerError : std::system_error {
    using std::system_error::system_error;
    WinHelloKeyManagerError(const std::system_error& err) : std::system_error(err){};
};

struct WinHelloKeyManagerUserCanceled : WinHelloKeyManagerError {
    using WinHelloKeyManagerError::WinHelloKeyManagerError;
};

/**
 * Class uses Microsoft Passport API to encrypt secret key and WinCred API to store secret keys.
 * At the creation of encryption key and decryption of stored secret key the user authentication
 * is required via Windows Hello biometrics or Windows Hello PIN.
 */
class WinHelloKeyManager
{
public:
    /**
    * Constructs new WinHelloKeyManager.
    * @param pointer to parent Window HWND. If null the function can block when user consent is required.
    *        see setParentWindowHandle.
    * @throw WinHelloKeyManagerError if Microsoft Passport is not available or any other error.
    */
    WinHelloKeyManager(HWND parentWndHandle = NULL);

    /**
    * Stores or updates key.
    * @note if validation  of encryption key fails, key manager should be reset via clear() function.
    *
    * @param key name
    * @param key bytes
    * @param override existing key. Default is true.
    * @throw WinHelloKeyManagerError if:
    *            - key exists and override parameter is false
    *            - encryption key creation or integrity validation fails
    *            - key encryption fails
    *            - storing encrypted key in credential manager fails
    *        WinHelloKeyManagerUserCanceled if user authentication was required during encryption key creation and user canceled the operation.
    */
    void storeKey(QStringView name, const QByteArray& key, bool bOverride = true);

    /**
    * Retrieves stored key.
    * @note if validation of decryption key fails, key manager should be reset via clear() function.
    *
    * @param name of the key to retrieve
    * @return key bytes
    * @throw WinHelloKeyManagerError if:
    *            - key doesn't exist
    *            - decryption key doesn't exist or it's integrity validation fails
    *            - retrieving encrypted key from credential manager fails
    *            - key decryption fails
    *        WinHelloKeyManagerUserCanceled if user canceled authentication during key decryption.
    */
    QByteArray getKey(QStringView name) const;

    /**
    * Checks if manager has stored key.
    * @param key name
    * @return true if manager contains key, otherwise false.
    */
    static bool contains(QStringView name);

    /**
    * Deletes stored key.
    * @param key name to delete
    * @return true if key existed and was successfully deleted, otherwise false.
    */
    bool removeKey(QStringView name) const; // Deletes stored key

    /**
    * Sets parent window handle.
    * @param parent windows HWND
    */
    void setParentWindowHandle(HWND hWND);

    /**
    * Deletes all stored keys and MS Passport encryption key.
    * @note if any of stored keys is not successfully deleted no exception is thrown.
    * @throw WinHelloKeyManagerError if encryption key was not successfully deleted.
    */
    void clear();

    /** Checks if Microsoft Passport is available. */
    static bool isAvailable();

private:
    QByteArray encrypt(const QByteArray& data);
    QByteArray promptToDecrypt(const QByteArray& edata) const;
    void setupEncryptionKey(bool createIfNotExists) const;

private:
    HWND m_hwnd;
    mutable NCryptKeyHandle m_hkey;
};

#endif //KEEPASSX_WINHELLOMANAGER_H
