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

#include "TestTools.h"

#include <QLocale>
#include <QTest>

QTEST_GUILESS_MAIN(TestTools)

namespace
{
    QString createDecimal(QString wholes, QString fractions, QString unit)
    {
        return wholes + QLocale().decimalPoint() + fractions + " " + unit;
    }
} // namespace

void TestTools::testHumanReadableFileSize()
{
    constexpr auto kibibyte = 1024u;
    using namespace Tools;

    QCOMPARE(createDecimal("1", "00", "B"), humanReadableFileSize(1));
    QCOMPARE(createDecimal("1", "00", "KiB"), humanReadableFileSize(kibibyte));
    QCOMPARE(createDecimal("1", "00", "MiB"), humanReadableFileSize(kibibyte * kibibyte));
    QCOMPARE(createDecimal("1", "00", "GiB"), humanReadableFileSize(kibibyte * kibibyte * kibibyte));

    QCOMPARE(QString("100 B"), humanReadableFileSize(100, 0));
    QCOMPARE(createDecimal("1", "10", "KiB"), humanReadableFileSize(kibibyte + 100));
    QCOMPARE(createDecimal("1", "001", "KiB"), humanReadableFileSize(kibibyte + 1, 3));
    QCOMPARE(createDecimal("15", "00", "KiB"), humanReadableFileSize(kibibyte * 15));
}

void TestTools::testIsHex()
{
    QVERIFY(Tools::isHex("0123456789abcdefABCDEF"));
    QVERIFY(not Tools::isHex(QByteArray("0xnothex")));
}

void TestTools::testIsBase64()
{
    QVERIFY(Tools::isBase64(QByteArray("1234")));
    QVERIFY(Tools::isBase64(QByteArray("123=")));
    QVERIFY(Tools::isBase64(QByteArray("12==")));
    QVERIFY(Tools::isBase64(QByteArray("abcd9876MN==")));
    QVERIFY(Tools::isBase64(QByteArray("abcd9876DEFGhijkMNO=")));
    QVERIFY(Tools::isBase64(QByteArray("abcd987/DEFGh+jk/NO=")));
    QVERIFY(not Tools::isBase64(QByteArray("abcd123==")));
    QVERIFY(not Tools::isBase64(QByteArray("abc_")));
    QVERIFY(not Tools::isBase64(QByteArray("123")));
}

void TestTools::testEnvSubstitute()
{
    QProcessEnvironment environment;

#if defined(Q_OS_WIN)
    environment.insert("HOMEDRIVE", "C:");
    environment.insert("HOMEPATH", "\\Users\\User");
    environment.insert("USERPROFILE", "C:\\Users\\User");

    QCOMPARE(Tools::envSubstitute("%HOMEDRIVE%%HOMEPATH%\\.ssh\\id_rsa", environment),
             QString("C:\\Users\\User\\.ssh\\id_rsa"));
    QCOMPARE(Tools::envSubstitute("start%EMPTY%%EMPTY%%%HOMEDRIVE%%end", environment), QString("start%C:%end"));
    QCOMPARE(Tools::envSubstitute("%USERPROFILE%\\.ssh\\id_rsa", environment),
             QString("C:\\Users\\User\\.ssh\\id_rsa"));
    QCOMPARE(Tools::envSubstitute("~\\.ssh\\id_rsa", environment), QString("C:\\Users\\User\\.ssh\\id_rsa"));
#else
    environment.insert("HOME", QString("/home/user"));
    environment.insert("USER", QString("user"));

    QCOMPARE(Tools::envSubstitute("~/.ssh/id_rsa", environment), QString("/home/user/.ssh/id_rsa"));
    QCOMPARE(Tools::envSubstitute("$HOME/.ssh/id_rsa", environment), QString("/home/user/.ssh/id_rsa"));
    QCOMPARE(Tools::envSubstitute("start/$EMPTY$$EMPTY$HOME/end", environment), QString("start/$/home/user/end"));
#endif
}

void TestTools::testBaseDomain()
{
    QString url1 = "https://another.example.co.uk";
    QString url2 = "https://www.example.com";
    QString url3 = "http://test.net";
    QString url4 = "http://so.many.subdomains.co.jp";
    QString url5 = "https://192.168.0.1";
    QString url6 = "https://192.168.0.1:8000";

    QString res1 = Tools::getBaseDomainFromUrl(url1);
    QString res2 = Tools::getBaseDomainFromUrl(url2);
    QString res3 = Tools::getBaseDomainFromUrl(url3);
    QString res4 = Tools::getBaseDomainFromUrl(url4);
    QString res5 = Tools::getBaseDomainFromUrl(url5);
    QString res6 = Tools::getBaseDomainFromUrl(url6);

    QCOMPARE(res1, QString("example.co.uk"));
    QCOMPARE(res2, QString("example.com"));
    QCOMPARE(res3, QString("test.net"));
    QCOMPARE(res4, QString("subdomains.co.jp"));
    QCOMPARE(res5, QString("192.168.0.1"));
    QCOMPARE(res6, QString("192.168.0.1"));
}
