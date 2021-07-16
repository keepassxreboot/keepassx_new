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

#include "AddGroup.h"

#include "Utils.h"
#include "core/Group.h"

#include <QCommandLineParser>

AddGroup::AddGroup()
{
    name = QString("mkdir");
    description = QObject::tr("Adds a new group to a database.");
    positionalArguments.append({QString("group"), QObject::tr("Path of the group to add."), QString("")});
}

AddGroup::~AddGroup()
{
}

int AddGroup::executeWithDatabase(QSharedPointer<Database> database, QSharedPointer<QCommandLineParser> parser)
{
    auto& out = Utils::STDOUT;
    auto& err = Utils::STDERR;

    const QStringList args = parser->positionalArguments();
    const QString& groupPath = args.at(1);

    QStringList pathParts = groupPath.split("/");
    QString groupName = pathParts.takeLast();
    QString parentGroupPath = pathParts.join("/");

    Group* group = database->rootGroup()->findGroupByPath(groupPath);
    if (group) {
        err << QObject::tr("Group %1 already exists!").arg(groupPath) << endl;
        return EXIT_FAILURE;
    }

    Group* parentGroup = database->rootGroup()->findGroupByPath(parentGroupPath);
    if (!parentGroup) {
        err << QObject::tr("Group %1 not found.").arg(parentGroupPath) << endl;
        return EXIT_FAILURE;
    }

    Group* newGroup = new Group();
    newGroup->setUuid(QUuid::createUuid());
    newGroup->setName(groupName);
    newGroup->setParent(parentGroup);

    QString errorMessage;
    if (!database->save(Database::Atomic, &errorMessage)) {
        err << QObject::tr("Writing the database failed %1.").arg(errorMessage) << endl;
        return EXIT_FAILURE;
    }

    if (!parser->isSet(Command::QuietOption)) {
        out << QObject::tr("Successfully added group %1.").arg(groupName) << endl;
    }
    return EXIT_SUCCESS;
}
