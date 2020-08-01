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

#include <QFileInfo>
#include <QString>
#include <QTextStream>

#include "Import.h"

#include "cli/TextStream.h"
#include "cli/Utils.h"
#include "core/Database.h"
#include "keys/CompositeKey.h"
#include "keys/Key.h"

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


CommandArgs Import::getParserArgs(const CommandCtx& ctx) const
{
    Q_UNUSED(ctx);
    static const CommandArgs args {
        {
            { "xml", QObject::tr("Path of the XML database export."), "" },
            { "database", QObject::tr("Path of the new database."), "" }
        },
        {},
        {}
    };
    return args;
}

int Import::execImpl(CommandCtx& ctx, const QCommandLineParser& parser)
{
    Q_UNUSED(ctx);

    auto& out = parser.isSet(Command::QuietOption) ? Utils::DEVNULL : Utils::STDOUT;
    auto& err = Utils::STDERR;

    const QStringList& args = parser.positionalArguments();
    const QString& xmlExportPath = getArg(0, ctx.getRunmode(), args);
    const QString& dbPath = getArg(1, ctx.getRunmode(), args);

    if (QFileInfo::exists(dbPath)) {
        err << QObject::tr("File %1 already exists.").arg(dbPath) << endl;
        return EXIT_FAILURE;
    }

    auto key = QSharedPointer<CompositeKey>::create();

    auto passwordKey = Utils::getConfirmedPassword();
    if (passwordKey.isNull()) {
        err << QObject::tr("Failed to set database password.") << endl;
        return EXIT_FAILURE;
    }
    key->addKey(passwordKey);

    if (key->isEmpty()) {
        err << QObject::tr("No key is set. Aborting database creation.") << endl;
        return EXIT_FAILURE;
    }

    QString errorMessage;
    Database db;
    db.setKdf(KeePass2::uuidToKdf(KeePass2::KDF_ARGON2));
    db.setKey(key);

    if (!db.import(xmlExportPath, &errorMessage)) {
        err << QObject::tr("Unable to import XML database: %1").arg(errorMessage) << endl;
        return EXIT_FAILURE;
    }

    if (!db.saveAs(dbPath, &errorMessage, true, false)) {
        err << QObject::tr("Failed to save the database: %1.").arg(errorMessage) << endl;
        return EXIT_FAILURE;
    }

    out << QObject::tr("Successfully imported database.") << endl;
    return EXIT_SUCCESS;
}
