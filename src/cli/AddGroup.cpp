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

#include <cstdlib>
#include <stdio.h>

#include "AddGroup.h"

#include "cli/TextStream.h"
#include "cli/Utils.h"
#include "core/Database.h"
#include "core/Entry.h"
#include "core/Group.h"


CommandArgs AddGroup::getParserArgs(const CommandCtx& ctx) const
{
    static const CommandArgs args {
        { {QString("group"), QObject::tr("Path of the group to add."), QString("")} },
        {},
        {}
    };
    return DatabaseCommand::getParserArgs(ctx).merge(args);
}

int AddGroup::executeWithDatabase(CommandCtx& ctx, const QCommandLineParser& parser)
{
    auto& out = Utils::STDOUT;
    auto& err = Utils::STDERR;

    const QString& groupPath = getArg(0, ctx.getRunmode(), parser.positionalArguments());
    QStringList pathParts = groupPath.split("/");
    QString groupName = pathParts.takeLast();
    QString parentGroupPath = pathParts.join("/");

    Database& database = ctx.getDb();
    Group* group = database.rootGroup()->findGroupByPath(groupPath);
    if (group) {
        err << QObject::tr("Group %1 already exists!").arg(groupPath) << endl;
        return EXIT_FAILURE;
    }

    Group* parentGroup = database.rootGroup()->findGroupByPath(parentGroupPath);
    if (!parentGroup) {
        err << QObject::tr("Group %1 not found.").arg(parentGroupPath) << endl;
        return EXIT_FAILURE;
    }

    Group* newGroup = new Group();
    newGroup->setUuid(QUuid::createUuid());
    newGroup->setName(groupName);
    newGroup->setParent(parentGroup);

    QString errorMessage;
    if (!database.save(&errorMessage, true, false)) {
        err << QObject::tr("Writing the database failed %1.").arg(errorMessage) << endl;
        return EXIT_FAILURE;
    }

    if (!parser.isSet(Command::QuietOption)) {
        out << QObject::tr("Successfully added group %1.").arg(groupName) << endl;
    }
    return EXIT_SUCCESS;
}
