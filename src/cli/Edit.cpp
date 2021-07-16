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

#include "Edit.h"

#include "Add.h"
#include "Generate.h"
#include "Utils.h"
#include "core/Group.h"
#include "core/PasswordGenerator.h"

#include <QCommandLineParser>

const QCommandLineOption Edit::TitleOption = QCommandLineOption(QStringList() << "t"
                                                                              << "title",
                                                                QObject::tr("Title for the entry."),
                                                                QObject::tr("title"));

Edit::Edit()
{
    name = QString("edit");
    description = QObject::tr("Edit an entry.");
    // Using some of the options from the Add command since they are the same.
    options.append(Add::UsernameOption);
    options.append(Add::UrlOption);
    options.append(Add::NotesOption);
    options.append(Add::PasswordPromptOption);
    options.append(Edit::TitleOption);
    positionalArguments.append({QString("entry"), QObject::tr("Path of the entry to edit."), QString("")});

    // Password generation options.
    options.append(Add::GenerateOption);
    options.append(Generate::PasswordLengthOption);
    options.append(Generate::LowerCaseOption);
    options.append(Generate::UpperCaseOption);
    options.append(Generate::NumbersOption);
    options.append(Generate::SpecialCharsOption);
    options.append(Generate::ExtendedAsciiOption);
    options.append(Generate::ExcludeCharsOption);
    options.append(Generate::ExcludeSimilarCharsOption);
    options.append(Generate::IncludeEveryGroupOption);
}

int Edit::executeWithDatabase(QSharedPointer<Database> database, QSharedPointer<QCommandLineParser> parser)
{
    auto& out = parser->isSet(Command::QuietOption) ? Utils::DEVNULL : Utils::STDOUT;
    auto& err = Utils::STDERR;

    const QStringList args = parser->positionalArguments();
    const QString& entryPath = args.at(1);

    // Cannot use those 2 options at the same time!
    if (parser->isSet(Add::GenerateOption) && parser->isSet(Add::PasswordPromptOption)) {
        err << QObject::tr("Cannot generate a password and prompt at the same time.") << endl;
        return EXIT_FAILURE;
    }

    // Validating the password generator here, before we actually start
    // the update.
    QSharedPointer<PasswordGenerator> passwordGenerator;
    bool generate = parser->isSet(Add::GenerateOption);
    if (generate) {
        passwordGenerator = Generate::createGenerator(parser);
        if (passwordGenerator.isNull()) {
            return EXIT_FAILURE;
        }
    }

    Entry* entry = database->rootGroup()->findEntryByPath(entryPath);
    if (!entry) {
        err << QObject::tr("Could not find entry with path %1.").arg(entryPath) << endl;
        return EXIT_FAILURE;
    }

    QString username = parser->value(Add::UsernameOption);
    QString url = parser->value(Add::UrlOption);
    QString notes = parser->value(Add::NotesOption);
    QString title = parser->value(Edit::TitleOption);
    bool prompt = parser->isSet(Add::PasswordPromptOption);
    if (username.isEmpty() && url.isEmpty() && notes.isEmpty() && title.isEmpty() && !prompt && !generate) {
        err << QObject::tr("Not changing any field for entry %1.").arg(entryPath) << endl;
        return EXIT_FAILURE;
    }

    entry->beginUpdate();

    if (!title.isEmpty()) {
        entry->setTitle(title);
    }

    if (!username.isEmpty()) {
        entry->setUsername(username);
    }

    if (!notes.isEmpty()) {
        entry->setNotes(notes.replace("\\n", "\n"));
    }

    if (!url.isEmpty()) {
        entry->setUrl(url);
    }

    if (prompt) {
        out << QObject::tr("Enter new password for entry: ") << flush;
        QString password = Utils::getPassword(parser->isSet(Command::QuietOption));
        entry->setPassword(password);
    } else if (generate) {
        QString password = passwordGenerator->generatePassword();
        entry->setPassword(password);
    }

    entry->endUpdate();

    QString errorMessage;
    if (!database->save(Database::Atomic, &errorMessage)) {
        err << QObject::tr("Writing the database failed: %1").arg(errorMessage) << endl;
        return EXIT_FAILURE;
    }

    out << QObject::tr("Successfully edited entry %1.").arg(entry->title()) << endl;
    return EXIT_SUCCESS;
}
