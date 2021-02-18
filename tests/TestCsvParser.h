/*
 *  Copyright (C) 2015 Enrico Mariotti <enricomariotti@yahoo.it>
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

#ifndef KEEPASSX_TESTCSVPARSER_H
#define KEEPASSX_TESTCSVPARSER_H

#include <QFile>
#include <QObject>
#include <QScopedPointer>
#include <QTemporaryFile>

#include "format/CsvParser.h"

class CsvParser;

class TestCsvParser : public QObject
{
    Q_OBJECT

public:
private slots:
    void init();
    void cleanup();
    void initTestCase();

    void testUnicode();
    void testLF();
    void testEmptyReparsing();
    void testSimple();
    void testEmptyQuoted();
    void testEmptyNewline();
    void testSeparator();
    void testCR();
    void testCRLF();
    void testMalformed();
    void testQualifier();
    void testNewline();
    void testEmptySimple();
    void testMissingQuote();
    void testComments();
    void testBackslashSyntax();
    void testReparsing();
    void testEmptyFile();
    void testQuoted();
    void testMultiline();
    void testColumns();

private:
    QScopedPointer<QTemporaryFile> file;
    QScopedPointer<CsvParser> parser;
    CsvTable t;
    void dumpRow(CsvTable table, int row);
};

#endif // KEEPASSX_TESTCSVPARSER_H
