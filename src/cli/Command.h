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

#ifndef KEEPASSXC_COMMAND_H
#define KEEPASSXC_COMMAND_H

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include "core/Database.h"

// At the moment, there's no QT class for the positional arguments
// like there is for the options (QCommandLineOption).
struct CommandLineArgument
{
    QString name;
    QString description;
    QString syntax;
};

class Command
{
public:
    Command();
    virtual ~Command();
    virtual int execute(const QStringList& arguments) = 0;
    QString name;
    QString description;
    QSharedPointer<Database> currentDatabase;
    QList<CommandLineArgument> positionalArguments;
    QList<CommandLineArgument> optionalArguments;
    QList<QCommandLineOption> options;

    QString getDescriptionLine();
    QSharedPointer<QCommandLineParser> getCommandLineParser(const QStringList& arguments);
    QString getHelpText();

    static const QCommandLineOption HelpOption;
    static const QCommandLineOption QuietOption;
    static const QCommandLineOption KeyFileOption;
    static const QCommandLineOption NoPasswordOption;
    static const QCommandLineOption YubiKeyOption;
    static const QCommandLineOption LedgerOption;
};

namespace Commands
{
    void setupCommands(bool interactive);
    QList<QSharedPointer<Command>> getCommands();
    QSharedPointer<Command> getCommand(const QString& commandName);
} // namespace Commands

#endif // KEEPASSXC_COMMAND_H
