/*
 *  Copyright (C) 2017 Weslly Honorato <﻿weslly@protonmail.com>
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

#ifndef QTOTP_H
#define QTOTP_H

#include <QMap>
#include <QString>
#include <QtCore/QSharedPointer>
#include <QtCore/qglobal.h>

class QUrl;

namespace Totp
{
    struct Encoder
    {
        QString name;
        QString shortName;
        QString alphabet;
        uint digits;
        uint step;
        bool reverse;
    };

    enum Algorithm
    {
        Sha1,
        Sha256,
        Sha512,
    };

    enum StorageFormat
    {
        OTPURL,
        KEEOTP,
        LEGACY,
    };

    struct Settings
    {
        Totp::StorageFormat format;
        Totp::Encoder encoder;
        Totp::Algorithm algorithm;
        QString key;
        bool custom;
        uint digits;
        uint step;
    };

    constexpr uint DEFAULT_STEP = 30u;
    constexpr uint DEFAULT_DIGITS = 6u;
    constexpr uint STEAM_DIGITS = 5u;
    constexpr Totp::Algorithm DEFAULT_ALGORITHM = Sha1;
    constexpr Totp::StorageFormat DEFAULT_FORMAT = OTPURL;
    static const QString STEAM_SHORTNAME = "S";

    static const QString ATTRIBUTE_OTP = "otp";
    static const QString ATTRIBUTE_SEED = "TOTP Seed";
    static const QString ATTRIBUTE_SETTINGS = "TOTP Settings";

    QSharedPointer<Totp::Settings> parseSettings(const QString& rawSettings, const QString& key = {});
    QSharedPointer<Totp::Settings> createSettings(const QString& key,
                                                  const uint digits,
                                                  const uint step,
                                                  const Totp::StorageFormat format = DEFAULT_FORMAT,
                                                  const QString& encoderShortName = {},
                                                  const Totp::Algorithm algorithm = DEFAULT_ALGORITHM);
    QString writeSettings(const QSharedPointer<Totp::Settings>& settings,
                          const QString& title = {},
                          const QString& username = {},
                          bool forceOtp = false);

    quint64 secondsLeft(const QSharedPointer<Totp::Settings>& settings);
    QString generateTotp(const QSharedPointer<Totp::Settings>& settings, const quint64 time = 0ull);

    QList<QPair<QString, QString>> supportedEncoders();
    QList<QPair<QString, Algorithm>> supportedAlgorithms();

    Encoder& defaultEncoder();
    Encoder& steamEncoder();
    Encoder& getEncoderByShortName(const QString& shortName);
    Encoder& getEncoderByName(const QString& name);
} // namespace Totp

#endif // QTOTP_H
