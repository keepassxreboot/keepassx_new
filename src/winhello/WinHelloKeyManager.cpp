#include "WinHelloKeyManager.h"
#include <assert.h>
#include <ncrypt.h>
#include <string_view>
#include <wincred.h>

#include<QString>

static constexpr auto kDomain            = "KeePassXC";
static constexpr auto kSubDomain         = "";
static constexpr auto kPersistentName    = "WinHelloKey";
static constexpr auto kCredPrefix        = "KeePassXCWinHello_";

constexpr auto kDefaultStorageProvider   = MS_NGC_KEY_STORAGE_PROVIDER; // MS Passport storage provider
constexpr auto kPersistentKeyAlgId       = BCRYPT_RSA_ALGORITHM;
constexpr auto kPersistentKeySize        = 2048;
constexpr auto kPersistentKeyUsage       = NCRYPT_ALLOW_DECRYPT_FLAG | NCRYPT_ALLOW_SIGNING_FLAG;

constexpr DWORD NCRYPT_NGC_CACHE_TYPE_PROPERTY_AUTH_MANDATORY_FLAG = 1;
constexpr auto NCRYPT_NGC_CACHE_TYPE_PROPERTY            = L"NgcCacheType";
constexpr auto NCRYPT_NGC_CACHE_TYPE_PROPERTY_DEPRECATED = L"NgcCacheTypeProperty";

inline std::wstring getEncryptionKeyName()
{
    return qstrToWStr(getCurrentUserSID() + "//" + kDomain + "/" + kSubDomain + "/" + kPersistentName);
}

inline std::wstring getCredTargetName(QStringView name)
{
    return qstrToWStr(kCredPrefix + name.toString());
}

static void whCheckStatus(DWORD dwStatus, const char* pszErrorMsg, DWORD dwIgnoreStatus = ERROR_SUCCESS)
{
    try{
        checkStatus(dwStatus, pszErrorMsg, dwIgnoreStatus);
    }
    catch (const std::system_error& err)
    {
        switch (err.code().value())
        {
        case NTE_USER_CANCELLED:
            throw WinHelloKeyManagerUserCanceled{ err };
        default:
            throw WinHelloKeyManagerError{ err };
        };
    }
}

void setNgcKeyPropertyMandatoryAuthn(const NCryptKeyHandle& hKey)
{
    if (NCryptSetProperty(
            hKey,
            NCRYPT_NGC_CACHE_TYPE_PROPERTY,
            (PBYTE)&NCRYPT_NGC_CACHE_TYPE_PROPERTY_AUTH_MANDATORY_FLAG,
            sizeof(NCRYPT_NGC_CACHE_TYPE_PROPERTY_AUTH_MANDATORY_FLAG),
            0
        ) != ERROR_SUCCESS) {
        whCheckStatus(
            NCryptSetProperty(
                hKey,
                NCRYPT_NGC_CACHE_TYPE_PROPERTY_DEPRECATED,
                (PBYTE)&NCRYPT_NGC_CACHE_TYPE_PROPERTY_AUTH_MANDATORY_FLAG,
                sizeof(NCRYPT_NGC_CACHE_TYPE_PROPERTY_AUTH_MANDATORY_FLAG),
                0
            ),
            "NCRYPT_NGC_CACHE_TYPE_PROPERTY_DEPRECATED"
        );
    }
}

void setKeyWindowHandleProperty(const NCryptKeyHandle& hKey, HWND hwnd)
{
    whCheckStatus(
        NCryptSetProperty(hKey, NCRYPT_WINDOW_HANDLE_PROPERTY, (PBYTE)&hwnd, sizeof(HWND), 0),
        "NCRYPT_WINDOW_HANDLE_PROPERTY"
    );
}

static NCryptKeyHandle createPersistentKey(std::wstring_view keyName, bool overwriteExisting, std::wstring_view providerName, HWND hwnd, const std::wstring& message)
{
    if (keyName.size() >= NCRYPT_MAX_KEY_NAME_LENGTH) {
        whCheckStatus(ERROR_INVALID_PARAMETER, "Persistent key name too big");
    }

    NCryptHandle hProvider;
    whCheckStatus(
        NCryptOpenStorageProvider(&hProvider, providerName.data(), 0),
        "NCryptOpenStorageProvider failed"
    );

    NCryptKeyHandle hKey;
    whCheckStatus(
        NCryptCreatePersistedKey(hProvider,
            &hKey,
            kPersistentKeyAlgId,
            keyName.data(),
            0,
            overwriteExisting ? NCRYPT_OVERWRITE_KEY_FLAG : 0
        ),
        "NCryptCreatePersistedKey failed"
    );

    whCheckStatus(
        NCryptSetProperty(
            hKey,
            NCRYPT_LENGTH_PROPERTY,
            (PBYTE)&kPersistentKeySize,
            sizeof(kPersistentKeySize),
            0
        ),
        "NCRYPT_LENGTH_PROPERTY"
    );

    whCheckStatus(
        NCryptSetProperty(
            hKey,
            NCRYPT_KEY_USAGE_PROPERTY,
            (PBYTE)&kPersistentKeyUsage,
            sizeof(kPersistentKeyUsage),
            0
        ),
        "NCRYPT_KEY_USAGE_PROPERTY"
    );

    if (providerName == MS_NGC_KEY_STORAGE_PROVIDER) {
        setNgcKeyPropertyMandatoryAuthn(hKey);
    }

    if (IsWindow(hwnd)) {
        setKeyWindowHandleProperty(hKey, hwnd);
    }

    NCRYPT_UI_POLICY uiPolicy{};
    uiPolicy.dwVersion      = 1;
    uiPolicy.dwFlags        = NCRYPT_UI_FORCE_HIGH_PROTECTION_FLAG;
    uiPolicy.pszDescription = message.data();

    whCheckStatus(
        NCryptSetProperty(hKey, NCRYPT_UI_POLICY_PROPERTY, (PBYTE)&uiPolicy, sizeof(uiPolicy), 0),
        "NCRYPT_UI_POLICY_PROPERTY"
    );

    whCheckStatus(NCryptFinalizeKey(hKey, 0), "NCryptFinalizeKey");
    return hKey;
}

static bool tryOpenPersistentKey(std::wstring_view keyName, NCryptKeyHandle& kh, std::wstring_view providerName)
{
    NCryptHandle hProvider;
    whCheckStatus(
        NCryptOpenStorageProvider(&hProvider, providerName.data(), 0),
        "NCryptOpenStorageProvider failed"
    );

    whCheckStatus(
        NCryptOpenKey(
            hProvider,
            &kh,
            keyName.data(),
            0,
            0
        ),
        "NCryptOpenKey failed",
        NTE_NO_KEY
    );
    return kh.isValid();
}

void deletePersistentKey(NCryptKeyHandle& kh) {
    whCheckStatus(
        NCryptDeleteKey(kh, 0),
        "NCryptDeleteKey failed"
    );
    kh.closeHandle();
}

bool verifyPersistentKeyIntegrity(const NCryptKeyHandle& hKey)
{
    DWORD pcbResult;
    int keyUsage = 0;
    whCheckStatus(
        NCryptGetProperty(
            hKey,
            NCRYPT_KEY_USAGE_PROPERTY,
            (PBYTE)&keyUsage,
            sizeof(keyUsage),
            &pcbResult,
            0
        ),
        "Getting key property NCRYPT_KEY_USAGE_PROPERTY failed"
    );

    if ((keyUsage & NCRYPT_ALLOW_KEY_IMPORT_FLAG) == NCRYPT_ALLOW_KEY_IMPORT_FLAG) {
        return false;
    }

    DWORD cacheType = 0;
    DWORD size = 0;
    if (NCryptGetProperty(
            hKey,
            NCRYPT_NGC_CACHE_TYPE_PROPERTY,
            (PBYTE)&cacheType,
            sizeof(cacheType),
            &size,
            0
        ) != ERROR_SUCCESS) {
        whCheckStatus(
            NCryptGetProperty(hKey, NCRYPT_NGC_CACHE_TYPE_PROPERTY_DEPRECATED, (PBYTE)&cacheType, sizeof(cacheType), &size, 0),
            "Getting key property NCRYPT_NGC_CACHE_TYPE_PROPERTY_DEPRECATED failed"
        );
    }
    return cacheType == NCRYPT_NGC_CACHE_TYPE_PROPERTY_AUTH_MANDATORY_FLAG;
}

WinHelloKeyManager::WinHelloKeyManager(HWND parentWndHandle) :
    m_hwnd(parentWndHandle)
{
    if (!isAvailable()) {
        checkStatus(NTE_NOT_SUPPORTED, "Microsoft Passport Key Storage Provider is not available");
    }
}

void WinHelloKeyManager::setParentWindowHandle(HWND hwnd)
{
    m_hwnd = hwnd;
}

QByteArray WinHelloKeyManager::encrypt(const QByteArray& data)
{
    setupEncryptionKey(/*createIfNotExists=*/true);
    if (!verifyPersistentKeyIntegrity(m_hkey)) {
        throw WinHelloKeyManagerError(makeWin32ErrorCode(NTE_BAD_KEY_STATE), "Encryption key integrity check failed");
    }

    DWORD pcbResult;
    whCheckStatus(
        NCryptEncrypt(m_hkey, (PBYTE)data.data(), data.size(), nullptr, nullptr, 0, &pcbResult, NCRYPT_PAD_PKCS1_FLAG),
        "NCryptEncrypt failed"
    );

    QByteArray edata(pcbResult, 0);
    whCheckStatus(
        NCryptEncrypt(m_hkey, (PBYTE)data.data(), data.size(), nullptr, (PBYTE)edata.data(), edata.size(), &pcbResult, NCRYPT_PAD_PKCS1_FLAG),
        "NCryptEncrypt failed"
    );

    assert(edata.size() == pcbResult);
    return edata;
}

QByteArray WinHelloKeyManager::promptToDecrypt(const QByteArray& edata) const
{
    setupEncryptionKey(/*createIfNotExists=*/false);
    if (!verifyPersistentKeyIntegrity(m_hkey)) {
        throw WinHelloKeyManagerError(makeWin32ErrorCode(NTE_BAD_KEY_STATE), "Decription key integrity check failed");
    }

    if (IsWindow(m_hwnd)) {
        setKeyWindowHandleProperty(m_hkey, m_hwnd);
    }

    std::wstring message = L"Authentication to access KeePassXC database";
    whCheckStatus(
        NCryptSetProperty(m_hkey, NCRYPT_USE_CONTEXT_PROPERTY, (PBYTE)message.data(), (message.size() + 1) * 2, 0),
        "NCRYPT_USE_CONTEXT_PROPERTY"
    );

    const DWORD pinRequired = NCRYPT_PIN_CACHE_REQUIRE_GESTURE_FLAG;
    whCheckStatus(
        NCryptSetProperty(m_hkey, NCRYPT_PIN_CACHE_IS_GESTURE_REQUIRED_PROPERTY, (PBYTE)&pinRequired, sizeof(pinRequired), 0),
        "NCRYPT_PIN_CACHE_IS_GESTURE_REQUIRED_PROPERTY"
    );

    QByteArray data(edata.size(), 0);
    DWORD pcbResult;
    whCheckStatus(
        NCryptDecrypt(m_hkey, (PBYTE)edata.data(), edata.size(), nullptr, (PBYTE)data.data(), data.size(), &pcbResult, NCRYPT_PAD_PKCS1_FLAG),
        "NCryptDecrypt failed"
    );

    data.resize(pcbResult);
    return data;
}

void WinHelloKeyManager::setupEncryptionKey(bool createIfNotExists) const
{
    if (!m_hkey.isValid())
    {
        const auto keyName = getEncryptionKeyName();
        if (!tryOpenPersistentKey(keyName, m_hkey, kDefaultStorageProvider) && createIfNotExists) {
            m_hkey = createPersistentKey(keyName, true, kDefaultStorageProvider, m_hwnd, L"");
        }
    }
}

void WinHelloKeyManager::storeKey(QStringView name, const QByteArray& key, bool bOverride)
{
    // TODO: verify max key size
    if (!bOverride && contains(name)) {
        whCheckStatus(ERROR_ALREADY_EXISTS, "Key already exists");
    }

    auto ekey = encrypt(key);

    const auto targetName = getCredTargetName(name);
    CREDENTIALW cred{};
    cred.Type               = CRED_TYPE_GENERIC;
    cred.Persist            = CRED_PERSIST_LOCAL_MACHINE;
    cred.UserName           = nullptr;
    cred.TargetName         = (LPWSTR)targetName.data();
    cred.CredentialBlob     = (LPBYTE)ekey.data();
    cred.CredentialBlobSize = ekey.size();

    if (!CredWriteW(&cred, 0)) {
        whCheckStatus(GetLastError(), "CredWriteW failed");
    }
}

QByteArray WinHelloKeyManager::getKey(QStringView name) const
{
    PCREDENTIALW pCred;
    if (!CredReadW(getCredTargetName(name).data(), CRED_TYPE_GENERIC, 0, &pCred)) {
        whCheckStatus(GetLastError(), "Key doesn't exist");
    }

    auto ptrCred = raii_ptr<CREDENTIALW>(pCred, [](const PCREDENTIALW p){
        if(p)CredFree(p);
    });

    auto ekey = QByteArray::fromRawData((const char*)ptrCred->CredentialBlob, ptrCred->CredentialBlobSize);
    return promptToDecrypt(ekey);
}

bool WinHelloKeyManager::contains(QStringView name)
{
    PCREDENTIALW pCred;
    bool hasKey = CredReadW(getCredTargetName(name).data(), CRED_TYPE_GENERIC, 0, &pCred);
    CredFree(pCred);
    return hasKey;
}

bool WinHelloKeyManager::removeKey(QStringView name) const
{
    return CredDeleteW(getCredTargetName(name).data(), CRED_TYPE_GENERIC, 0);
}

void WinHelloKeyManager::clear()
{
    PCREDENTIALW* pCreds;
    DWORD count;
    if (CredEnumerateW(getCredTargetName(u"*").data(), 0, &count, &pCreds)) {
        for (DWORD i = 0; i != count; ++i) {
            if (!CredDeleteW(pCreds[i]->TargetName, CRED_TYPE_GENERIC, 0)) {
                if (GetLastError() != ERROR_NOT_FOUND) {
                    // do something
                }
            }
        }
        CredFree(pCreds);

        setupEncryptionKey(/*createIfNotExists=*/false);
        if (m_hkey.isValid()) {
            deletePersistentKey(m_hkey);
        }
    }
}

bool WinHelloKeyManager::isAvailable()
{
    NCryptHandle hProvider;
    if (NCryptOpenStorageProvider(&hProvider, kDefaultStorageProvider, 0) != ERROR_SUCCESS) {
        return false;
    }

    // Try to open ephemeral key
    NCryptKeyHandle kh;
    const auto result = NCryptOpenKey(hProvider, &kh, getEncryptionKeyName().data(), 0, 0);
    return result == ERROR_SUCCESS || result == NTE_NO_KEY;
}
