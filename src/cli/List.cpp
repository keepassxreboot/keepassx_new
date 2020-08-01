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

#include "List.h"
#include "cli/Utils.h"

#include "cli/TextStream.h"
#include "core/Database.h"
#include "core/Entry.h"
#include "core/Group.h"

const QCommandLineOption RecursiveOption =
    QCommandLineOption(QStringList() << "R"
                                     << "recursive",
                       QObject::tr("Recursively list the elements of the group."));

const QCommandLineOption FlattenOption = QCommandLineOption(QStringList() << "f"
                                                                                << "flatten",
                                                                  QObject::tr("Flattens the output to single lines."));

CommandArgs List::getParserArgs(const CommandCtx& ctx) const
{
    static const CommandArgs args {
        {},
        { {"group", QObject::tr("Path of the group to list. Default is /"), "[group]"} },
        {
            RecursiveOption,
            FlattenOption
        }
    };
    return DatabaseCommand::getParserArgs(ctx).merge(args);
}

int List::executeWithDatabase(CommandCtx& ctx, const QCommandLineParser& parser)
{
    auto& out = Utils::STDOUT;
    auto& err = Utils::STDERR;

    const bool recursive = parser.isSet(RecursiveOption);
    const bool flatten = parser.isSet(FlattenOption);

    Database& database = ctx.getDb();
    const QStringList args = parser.positionalArguments();
    // No group provided, defaulting to root group.
    if (args.empty() || (ctx.getRunmode() == Runmode::SingleCmd && args.size() == 1)) {
        out << database.rootGroup()->print(recursive, flatten) << flush;
        return EXIT_SUCCESS;
    }
    const QString& groupPath = getArg(0, ctx.getRunmode(), args);
    const Group* group = database.rootGroup()->findGroupByPath(groupPath);
    if (!group) {
        err << QObject::tr("Cannot find group %1.").arg(groupPath) << endl;
        return EXIT_FAILURE;
    }

    out << group->print(recursive, flatten) << flush;
    return EXIT_SUCCESS;
}
