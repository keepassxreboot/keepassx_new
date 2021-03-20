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

#ifndef KEEPASSX_TESTTOOLS_H
#define KEEPASSX_TESTTOOLS_H

#include "core/Tools.h"

class TestTools : public QObject
{
    Q_OBJECT
private slots:
    void testHumanReadableFileSize();
    void testIsHex();
    void testIsBase64();
    void testEnvSubstitute();
};

#endif // KEEPASSX_TESTTOOLS_H
