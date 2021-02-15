#ifndef KEEPASSX_WINDOWSHELLO_H
#define KEEPASSX_WINDOWSHELLO_H
#include <QStringView>
#include <QByteArray>
#include <QSharedPointer>
#include <QStringView>
#include <QWidget>

#include "WinHelloKeyManager.h"

class WindowsHello : public QWidget
{
public:
    static bool isAvailable();
    static bool containsKey(QStringView dbPath);

    WindowsHello(QWidget* parent = nullptr); //!< @throw WinHelloKeyManagerError if Windows Hello is not available
    bool storeKey(QStringView dbPath, const QByteArray& key);
    QSharedPointer<QByteArray> getKey(QStringView dbPath) const;
    void removeKey(QStringView dbPath);
    bool reset() const;

private:
    void handleKeyStorageError(const WinHelloKeyManagerError& e) const;
    void showResetDialog(const QString& errorMsg) const;

private:
    mutable WinHelloKeyManager m_keymgr;
};

#endif // KEEPASSX_WINDOWSHELLO_H
