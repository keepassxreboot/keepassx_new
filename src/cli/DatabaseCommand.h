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

#ifndef KEEPASSXC_DATABASECOMMAND_H
#define KEEPASSXC_DATABASECOMMAND_H

#include <QCommandLineOption>

#include "Command.h"
#include "Utils.h"
#include "core/Database.h"

class DatabaseCommand : public Command
{
    using Ancestor = Command;

public:
    using Ancestor::Ancestor;

    virtual int executeWithDatabase(CommandCtx& ctx, const QCommandLineParser& parser) = 0;

protected:
    std::unique_ptr<Database> openDatabase(const QCommandLineParser& parser);
    CommandArgs getParserArgs(const CommandCtx& ctx) const override;

private:
    int execImpl(CommandCtx& ctx, const QCommandLineParser& parser) override;
};

#endif // KEEPASSXC_DATABASECOMMAND_H
