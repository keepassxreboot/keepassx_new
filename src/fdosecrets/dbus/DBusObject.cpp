/*
 *  Copyright (C) 2018 Aetf <aetf@unlimitedcodeworks.xyz>
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

#include "DBusObject.h"

#include <QFile>
#include <QRegularExpression>
#include <QTextStream>
#include <QUrl>
#include <QUuid>

namespace FdoSecrets
{

    DBusObject::DBusObject(DBusObject* parent)
        : DBusObject(parent, parent->dbus())
    {
    }

    DBusObject::DBusObject(DBusObject* parent, DBusMgr& dbus)
        : QObject(parent)
        , m_objectPath("/")
        , m_dbus(dbus)
    {}

    DBusObject::~DBusObject()
    {
        emit destroyed(this);
    }

    void DBusObject::setObjectPath(const QString& path)
    {
        m_objectPath.setPath(path);
    }

    QString encodePath(const QString& value)
    {
        // toPercentEncoding encodes everything except those in the unreserved group:
        // ALPHA / DIGIT / "-" / "." / "_" / "~"
        // we want only ALPHA / DIGIT / "_", with "_" as the escape character
        // so force "-.~_" to be encoded
        return QUrl::toPercentEncoding(value, "", "-.~_").replace('%', '_');
    }

} // namespace FdoSecrets
