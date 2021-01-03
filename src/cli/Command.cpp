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

#include <cstdio>
#include <cstdlib>
#include <utility>

#include <QFileInfo>
#include <QMap>

#include "Command.h"

#include "Add.h"
#include "AddGroup.h"
#include "Analyze.h"
#include "Clip.h"
#include "Close.h"
#include "Create.h"
#include "Diceware.h"
#include "Edit.h"
#include "Estimate.h"
#include "Exit.h"
#include "Export.h"
#include "Generate.h"
#include "Help.h"
#include "Import.h"
#include "Info.h"
#include "List.h"
#include "Locate.h"
#include "Merge.h"
#include "Move.h"
#include "Open.h"
#include "Remove.h"
#include "RemoveGroup.h"
#include "Show.h"
#include "TextStream.h"
#include "Utils.h"

const QCommandLineOption Command::HelpOption = QCommandLineOption(QStringList()
#ifdef Q_OS_WIN
                                                                      << QStringLiteral("?")
#endif
                                                                      << QStringLiteral("h") << QStringLiteral("help"),
                                                                  QObject::tr("Display this help."));

const QCommandLineOption Command::QuietOption =
    QCommandLineOption(QStringList() << "q"
                                     << "quiet",
                       QObject::tr("Silence password prompt and other secondary outputs."));

const QCommandLineOption Command::KeyFileOption = QCommandLineOption(QStringList() << "k"
                                                                                   << "key-file",
                                                                     QObject::tr("Key file of the database."),
                                                                     QObject::tr("path"));

const QCommandLineOption Command::NoPasswordOption =
    QCommandLineOption(QStringList() << "no-password", QObject::tr("Deactivate password key for the database."));

const QCommandLineOption Command::YubiKeyOption =
    QCommandLineOption(QStringList() << "y"
                                     << "yubikey",
                       QObject::tr("Yubikey slot and optional serial used to access the database (e.g., 1:7370001)."),
                       QObject::tr("slot[:serial]"));

const QCommandLineOption Command::LedgerOption =
    QCommandLineOption(QStringList() << "ledger",
                       QObject::tr("Ledger slot or name used to access the database (e.g., slot:0, name:personal)."),
                       QObject::tr("(slot:id|name:string)"));

namespace
{

    QSharedPointer<QCommandLineParser> buildParser(Command* command)
    {
        auto parser = QSharedPointer<QCommandLineParser>(new QCommandLineParser());
        parser->setApplicationDescription(command->description);
        for (const CommandLineArgument& positionalArgument : command->positionalArguments) {
            parser->addPositionalArgument(
                positionalArgument.name, positionalArgument.description, positionalArgument.syntax);
        }
        for (const CommandLineArgument& optionalArgument : command->optionalArguments) {
            parser->addPositionalArgument(optionalArgument.name, optionalArgument.description, optionalArgument.syntax);
        }
        for (const QCommandLineOption& option : command->options) {
            parser->addOption(option);
        }
        parser->addOption(Command::HelpOption);
        return parser;
    }

} // namespace

Command::Command()
    : currentDatabase(nullptr)
{
    options.append(Command::QuietOption);
}

Command::~Command()
{
}

QString Command::getDescriptionLine()
{
    QString response = name;
    QString space(" ");
    QString spaces = space.repeated(15 - name.length());
    response = response.append(spaces);
    response = response.append(description);
    response = response.append("\n");
    return response;
}

QString Command::getHelpText()
{
    auto help = buildParser(this)->helpText();
    // Fix spacing of options parameter
    help.replace(QStringLiteral("[options]"), name + QStringLiteral(" [options]"));
    // Remove application directory from command line example
    auto appname = QFileInfo(QCoreApplication::applicationFilePath()).fileName();
    auto regex = QRegularExpression(QStringLiteral(" .*%1").arg(QRegularExpression::escape(appname)));
    help.replace(regex, appname.prepend(" "));

    return help;
}

QSharedPointer<QCommandLineParser> Command::getCommandLineParser(const QStringList& arguments)
{
    auto& err = Utils::STDERR;
    QSharedPointer<QCommandLineParser> parser = buildParser(this);

    if (!parser->parse(arguments)) {
        err << parser->errorText() << "\n\n";
        err << getHelpText();
        return {};
    }
    if (parser->positionalArguments().size() < positionalArguments.size()) {
        err << getHelpText();
        return {};
    }
    if (parser->positionalArguments().size() > (positionalArguments.size() + optionalArguments.size())) {
        err << getHelpText();
        return {};
    }
    if (parser->isSet(HelpOption)) {
        err << getHelpText();
        return {};
    }
    return parser;
}

namespace Commands
{
    QMap<QString, QSharedPointer<Command>> s_commands;

    void setupCommands(bool interactive)
    {
        s_commands.clear();

        s_commands.insert(QStringLiteral("add"), QSharedPointer<Command>(new Add()));
        s_commands.insert(QStringLiteral("analyze"), QSharedPointer<Command>(new Analyze()));
        s_commands.insert(QStringLiteral("clip"), QSharedPointer<Command>(new Clip()));
        s_commands.insert(QStringLiteral("close"), QSharedPointer<Command>(new Close()));
        s_commands.insert(QStringLiteral("db-create"), QSharedPointer<Command>(new Create()));
        s_commands.insert(QStringLiteral("db-info"), QSharedPointer<Command>(new Info()));
        s_commands.insert(QStringLiteral("diceware"), QSharedPointer<Command>(new Diceware()));
        s_commands.insert(QStringLiteral("edit"), QSharedPointer<Command>(new Edit()));
        s_commands.insert(QStringLiteral("estimate"), QSharedPointer<Command>(new Estimate()));
        s_commands.insert(QStringLiteral("generate"), QSharedPointer<Command>(new Generate()));
        s_commands.insert(QStringLiteral("help"), QSharedPointer<Command>(new Help()));
        s_commands.insert(QStringLiteral("locate"), QSharedPointer<Command>(new Locate()));
        s_commands.insert(QStringLiteral("ls"), QSharedPointer<Command>(new List()));
        s_commands.insert(QStringLiteral("merge"), QSharedPointer<Command>(new Merge()));
        s_commands.insert(QStringLiteral("mkdir"), QSharedPointer<Command>(new AddGroup()));
        s_commands.insert(QStringLiteral("mv"), QSharedPointer<Command>(new Move()));
        s_commands.insert(QStringLiteral("open"), QSharedPointer<Command>(new Open()));
        s_commands.insert(QStringLiteral("rm"), QSharedPointer<Command>(new Remove()));
        s_commands.insert(QStringLiteral("rmdir"), QSharedPointer<Command>(new RemoveGroup()));
        s_commands.insert(QStringLiteral("show"), QSharedPointer<Command>(new Show()));

        if (interactive) {
            s_commands.insert(QStringLiteral("exit"), QSharedPointer<Command>(new Exit("exit")));
            s_commands.insert(QStringLiteral("quit"), QSharedPointer<Command>(new Exit("quit")));
        } else {
            s_commands.insert(QStringLiteral("export"), QSharedPointer<Command>(new Export()));
            s_commands.insert(QStringLiteral("import"), QSharedPointer<Command>(new Import()));
        }
    }

    QList<QSharedPointer<Command>> getCommands()
    {
        return s_commands.values();
    }

    QSharedPointer<Command> getCommand(const QString& commandName)
    {
        return s_commands.value(commandName);
    }
} // namespace Commands
