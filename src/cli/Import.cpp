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

#include "Import.h"

#include "Create.h"
#include "Utils.h"

#include <QCommandLineParser>
#include <QFileInfo>

/**
 * Create a database file from an XML export of another database.
 * A password can be specified to encrypt the database.
 * If none is specified the function will fail.
 *
 * If the database is being saved in a non existant directory, the
 * function will fail.
 *
 * @return EXIT_SUCCESS on success, or EXIT_FAILURE on failure
 */

Import::Import()
{
    name = QString("import");
    description = QObject::tr("Import the contents of an XML database.");
    positionalArguments.append({QString("xml"), QObject::tr("Path of the XML database export."), QString("")});
    positionalArguments.append({QString("database"), QObject::tr("Path of the new database."), QString("")});
    options.append(Create::SetKeyFileOption);
    options.append(Create::SetPasswordOption);
    options.append(Create::DecryptionTimeOption);
}

int Import::execute(const QStringList& arguments)
{
    QSharedPointer<QCommandLineParser> parser = getCommandLineParser(arguments);
    if (parser.isNull()) {
        return EXIT_FAILURE;
    }

    auto& out = parser->isSet(Command::QuietOption) ? Utils::DEVNULL : Utils::STDOUT;
    auto& err = Utils::STDERR;

    const QStringList args = parser->positionalArguments();
    const QString& xmlExportPath = args.at(0);
    const QString& dbPath = args.at(1);

    if (QFileInfo::exists(dbPath)) {
        err << QObject::tr("File %1 already exists.").arg(dbPath) << endl;
        return EXIT_FAILURE;
    }

    QSharedPointer<Database> db = Create::initializeDatabaseFromOptions(parser);
    if (!db) {
        return EXIT_FAILURE;
    }

    QString errorMessage;
    if (!db->import(xmlExportPath, &errorMessage)) {
        err << QObject::tr("Unable to import XML database: %1").arg(errorMessage) << endl;
        return EXIT_FAILURE;
    }

    if (!db->saveAs(dbPath, Database::Atomic, &errorMessage)) {
        err << QObject::tr("Failed to save the database: %1.").arg(errorMessage) << endl;
        return EXIT_FAILURE;
    }

    out << QObject::tr("Successfully imported database.") << endl;
    return EXIT_SUCCESS;
}
