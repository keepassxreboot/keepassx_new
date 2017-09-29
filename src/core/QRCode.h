/*
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

#ifndef KEEPASSX_QRCODE_H
#define KEEPASSX_QRCODE_H

#include <QScopedPointer>
#include <QtCore/qglobal.h>

class QImage;
class QString;
class QByteArray;

struct QRCodePrivate;

class QRCode
{

public:
    enum class ErrorCorrectionLevel : int
    {
        LOW = 0,
        MEDIUM,
        QUARTILE,
        HIGH
    };

    // See: http://www.qrcode.com/en/about/version.html
    enum class Version : int
    {
        AUTO = 0,
        V1,
        V2,
        V3,
        V4,
        V5,
        V6,
        V7,
        V8,
        V9,
        V10,
        V11,
        V12,
        V13,
        V14,
        V15,
        V16,
        V17,
        V18,
        V19,
        V20,
        V21,
        V22,
        V23,
        V24,
        V25,
        V26,
        V27,
        V28,
        V29,
        V30,
        V31,
        V32,
        V33,
        V34,
        V35,
        V36,
        V37,
        V38,
        V39,
        V40
    };

    // Uses QRcode_encodeString (can't contain NUL characters)
    explicit QRCode(const QString& data,
                    const Version version = Version::AUTO,
                    const ErrorCorrectionLevel ecl = ErrorCorrectionLevel::HIGH,
                    const bool caseSensitive = true);

    // Uses QRcode_encodeData (can contain NUL characters)
    explicit QRCode(const QByteArray& data,
                    const Version version = Version::AUTO,
                    const ErrorCorrectionLevel ecl = ErrorCorrectionLevel::HIGH);

    QRCode();
    ~QRCode();

    QImage toQImage(const int size, const int margin) const;

private:
    void init(const QString& data, const Version version, const ErrorCorrectionLevel ecl, const bool caseSensitive);

    void init(const QByteArray& data, const Version version, const ErrorCorrectionLevel ecl);

    QScopedPointer<QRCodePrivate> d_ptr;
};

#endif // KEEPASSX_QRCODE_H
