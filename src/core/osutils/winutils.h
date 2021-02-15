#ifndef KEEPASSXC_CORE_WINUTILS_H
#define KEEPASSXC_CORE_WINUTILS_H
#include <QSharedPointer>
#include <QString>
#include <QStringView>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include <windows.h>
#include <ncrypt.h>
#include <sddl.h>
#undef MessageBox

#include <memory>
#include <functional>
#include <system_error>

// The Win32 error code category.
class Win32ErrorCategory : public std::error_category
{
public:
    char const* name() const noexcept override final {
        return "Win32Error";
    }

    std::string message(int c) const override final
    {
        char error[256];
        auto len = FormatMessageA(
            FORMAT_MESSAGE_FROM_SYSTEM,
            NULL,
            static_cast<DWORD>(c),
            0,
            error,
            sizeof(error),
            NULL
            );

        if (len == 0) return "N/A";

        // trim trailing newline
        while (len && (error[len - 1] == '\r' || error[len - 1] == '\n')) {
            --len;
        }
        return std::string(error, len);
    }
};

// Returns global static wi32 error category instance
static const Win32ErrorCategory& win32ErrorCategory()
{
    static Win32ErrorCategory c;
    return c;
}

inline std::error_code makeWin32ErrorCode(DWORD dwError)
{
    return std::error_code(static_cast<int>(dwError), win32ErrorCategory());
}

static void checkStatus(DWORD dwStatus, const char* pszErrorMsg, DWORD dwIgnoreStatus = ERROR_SUCCESS)
{
    if (IS_ERROR(HRESULT_FROM_WIN32(dwStatus)) && dwStatus != dwIgnoreStatus)
    {
        const auto ec = makeWin32ErrorCode(HRESULT_FROM_WIN32(dwStatus));
        throw std::system_error(ec, pszErrorMsg);
    }
}

template <typename T>
using raii_ptr = std::unique_ptr<T, std::function<void(T*)>>;

template <typename T>
struct local_ptr : raii_ptr<T>
{
    local_ptr(SIZE_T uBytes, UINT uFlags = LPTR) :
        raii_ptr<T>(
            (T*)LocalAlloc(uFlags, uBytes),
            [](T* p) {
                if (p) {
                    LocalFree((HLOCAL)p);
                }
            })
    {
        if (this->get() == nullptr) {
            checkStatus(GetLastError(), "Failed to alloc memory");
        }
    }
};

template<typename HandleT, typename DeriveT>
struct WinHandleBase
{
    WinHandleBase(const WinHandleBase&) = delete;
    WinHandleBase& operator=(const WinHandleBase&) = delete;
    WinHandleBase(WinHandleBase&& rhs) noexcept :
        handle(rhs.handle)
    {
        rhs.handle = 0;
    }

    WinHandleBase& operator=(WinHandleBase&& rhs) noexcept
    {
        if (std::addressof(rhs) != this)
        {
            static_cast<DeriveT*>(this)->closeHandle();
            handle = rhs.handle;
            rhs.handle = 0;
        }
        return *this;
    }

    HandleT handle = 0;

    HandleT* operator&()
    {
        return &handle;
    }

    const HandleT* operator&() const
    {
        return &handle;
    }

    operator HandleT() const
    {
        return handle;
    }

    bool isValid() const {
        return handle != 0;
    }

    explicit operator bool() const {
        return isValid();
    }

    ~WinHandleBase() noexcept {
        static_cast<DeriveT*>(this)->closeHandle();
    }

private:
    WinHandleBase() = default;
    friend DeriveT;
};

struct WinHandle final : WinHandleBase<HANDLE, WinHandle>
{
    void closeHandle() noexcept
    {
        if (handle) {
            CloseHandle(handle);
            handle = nullptr;
        }
    }
};

struct NCryptHandle : WinHandleBase<NCRYPT_HANDLE, NCryptHandle>
{
    void closeHandle() noexcept
    {
        if (handle) {
            NCryptFreeObject(handle);
            handle = NULL;
        }
    }
};

struct NCryptKeyHandle final : NCryptHandle{};

static QString getCurrentUserSID()
{
    // from https://stackoverflow.com/questions/11316350/how-to-get-psid-from-token-information-class/11319825#11319825

    // Open a handle to the access token for the calling process.
    WinHandle hToken;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)){
        checkStatus(GetLastError(), "OpenProcessToken failed");
    }

    DWORD dwSize = 0;
    if (!GetTokenInformation(hToken, TokenUser, NULL, 0, &dwSize)
        && ERROR_INSUFFICIENT_BUFFER != GetLastError()) {
        checkStatus(GetLastError(), "GetTokenInformation failed");
    }

    auto ptu = local_ptr<TOKEN_USER>(dwSize);
    if (!GetTokenInformation(hToken, TokenUser, ptu.get(), dwSize, &dwSize)) {
        checkStatus(GetLastError(), "GetTokenInformation failed");
    }

    auto ptrSid = raii_ptr<WCHAR>(
        nullptr,
        [](WCHAR* p) {
            if (p) {
                LocalFree((HLOCAL)p);
            }
        });

    LPWSTR pszSid = ptrSid.get();
    if (!ConvertSidToStringSidW(ptu->User.Sid, &pszSid)) {
        checkStatus(GetLastError(), "ConvertSidToStringSidW failed");
    }

    ptrSid.reset(pszSid);
    return QString::fromWCharArray(pszSid);
}

inline std::wstring qstrToWStr(QStringView str)
{
    //https://wiki.qt.io/ToStdWStringAndBuiltInWchar
    return std::wstring((const wchar_t*)str.utf16());
}

#endif // KEEPASSXC_CORE_WINUTILS_H
