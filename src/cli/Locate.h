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

#ifndef KEEPASSXC_LOCATE_H
#define KEEPASSXC_LOCATE_H

#include "DatabaseCommand.h"

class Locate : public DatabaseCommand
{
    using Ancestor = DatabaseCommand;
public:
    using Ancestor::Ancestor;

    int executeWithDatabase(CommandCtx& ctx, const QCommandLineParser& parser) override;

private:
    CommandArgs getParserArgs(const CommandCtx& ctx) const override;
};
DECL_TRAITS(Locate, "locate", "Find entries quickly.");

#endif // KEEPASSXC_LOCATE_H
