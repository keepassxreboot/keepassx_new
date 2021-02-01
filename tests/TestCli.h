/*
 *  Copyright (C) 2019 KeePassXC Team <team@keepassxc.org>
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

#ifndef KEEPASSXC_TESTCLI_H
#define KEEPASSXC_TESTCLI_H

#include <QBuffer>
#include <QScopedPointer>
#include <QSharedPointer>
#include <QStringList>
#include <QTest>

#include "util/TemporaryFile.h"

class Command;
class Database;

class TestCli : public QObject
{
    Q_OBJECT

private:
    QSharedPointer<Database>
    readDatabase(const QString& filename = {}, const QString& pw = {}, const QString& keyfile = {});
    int execCmd(Command& cmd, const QStringList& args) const;
    bool isTotp(const QString& value);
    void setInput(const QString& input);
    void setInput(const QStringList& input);

private slots:
    void initTestCase();
    void init();
    void cleanup();
    void cleanupTestCase();

    void testBatchCommands();
    void testAdd();
    void testAddGroup();
    void testAnalyze();
    void testAttachmentExport();
    void testAttachmentImport();
    void testAttachmentRemove();
    void testClip();
    void testCommandParsing_data();
    void testCommandParsing();
    void testCreate();
    void testDiceware();
    void testEdit();
    void testEstimate_data();
    void testEstimate();
    void testExport();
    void testGenerate_data();
    void testGenerate();
    void testImport();
    void testInfo();
    void testKeyFileOption();
    void testNoPasswordOption();
    void testHelp();
    void testInteractiveCommands();
    void testList();
    void testLocate();
    void testMerge();
    void testMergeWithKeys();
    void testMove();
    void testOpen();
    void testRemove();
    void testRemoveGroup();
    void testRemoveQuiet();
    void testShow();
    void testInvalidDbFiles();
    void testYubiKeyOption();

private:
    QScopedPointer<QFile> m_devNull;
    QScopedPointer<TemporaryFile> m_dbFile;
    QScopedPointer<TemporaryFile> m_dbFile2;
    QScopedPointer<TemporaryFile> m_dbFileMulti;
    QScopedPointer<TemporaryFile> m_xmlFile;
    QScopedPointer<TemporaryFile> m_keyFileProtectedDbFile;
    QScopedPointer<TemporaryFile> m_keyFileProtectedNoPasswordDbFile;
    QScopedPointer<TemporaryFile> m_yubiKeyProtectedDbFile;

    QScopedPointer<QBuffer> m_stdout;
    QScopedPointer<QBuffer> m_stderr;
    QScopedPointer<QBuffer> m_stdin;
};

#endif // KEEPASSXC_TESTCLI_H
