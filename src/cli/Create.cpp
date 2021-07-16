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

#include "Create.h"

#include "Utils.h"
#include "keys/FileKey.h"

#include <QCommandLineParser>
#include <QFileInfo>

const QCommandLineOption Create::DecryptionTimeOption =
    QCommandLineOption(QStringList() << "t"
                                     << "decryption-time",
                       QObject::tr("Target decryption time in MS for the database."),
                       QObject::tr("time"));

const QCommandLineOption Create::SetKeyFileOption =
    QCommandLineOption(QStringList() << "k"
                                     << "set-key-file",
                       QObject::tr("Set the key file for the database."),
                       QObject::tr("path"));

const QCommandLineOption Create::SetPasswordOption =
    QCommandLineOption(QStringList() << "p"
                                     << "set-password",
                       QObject::tr("Set a password for the database."));

Create::Create()
{
    name = QString("db-create");
    description = QObject::tr("Create a new database.");
    positionalArguments.append({QString("database"), QObject::tr("Path of the database."), QString("")});
    options.append(Create::SetKeyFileOption);
    options.append(Create::SetPasswordOption);
    options.append(Create::DecryptionTimeOption);
}

QSharedPointer<Database> Create::initializeDatabaseFromOptions(const QSharedPointer<QCommandLineParser>& parser)
{
    if (parser.isNull()) {
        return {};
    }

    auto& out = parser->isSet(Command::QuietOption) ? Utils::DEVNULL : Utils::STDOUT;
    auto& err = Utils::STDERR;

    // Validate the decryption time before asking for a password.
    QString decryptionTimeValue = parser->value(Create::DecryptionTimeOption);
    int decryptionTime = 0;
    if (decryptionTimeValue.length() != 0) {
        decryptionTime = decryptionTimeValue.toInt();
        if (decryptionTime <= 0) {
            err << QObject::tr("Invalid decryption time %1.").arg(decryptionTimeValue) << endl;
            return {};
        }
        if (decryptionTime < Kdf::MIN_ENCRYPTION_TIME || decryptionTime > Kdf::MAX_ENCRYPTION_TIME) {
            err << QObject::tr("Target decryption time must be between %1 and %2.")
                       .arg(QString::number(Kdf::MIN_ENCRYPTION_TIME), QString::number(Kdf::MAX_ENCRYPTION_TIME))
                << endl;
            return {};
        }
    }

    auto key = QSharedPointer<CompositeKey>::create();

    if (parser->isSet(Create::SetPasswordOption)) {
        auto passwordKey = Utils::getConfirmedPassword();
        if (passwordKey.isNull()) {
            err << QObject::tr("Failed to set database password.") << endl;
            return {};
        }
        key->addKey(passwordKey);
    }

    if (parser->isSet(Create::SetKeyFileOption)) {
        QSharedPointer<FileKey> fileKey;

        if (!Utils::loadFileKey(parser->value(Create::SetKeyFileOption), fileKey)) {
            err << QObject::tr("Loading the key file failed") << endl;
            return {};
        }

        if (!fileKey.isNull()) {
            key->addKey(fileKey);
        }
    }

    if (key->isEmpty()) {
        err << QObject::tr("No key is set. Aborting database creation.") << endl;
        return {};
    }

    auto db = QSharedPointer<Database>::create();
    db->setKey(key);

    if (decryptionTime != 0) {
        auto kdf = db->kdf();
        Q_ASSERT(kdf);

        out << QObject::tr("Benchmarking key derivation function for %1ms delay.").arg(decryptionTimeValue) << endl;
        int rounds = kdf->benchmark(decryptionTime);
        out << QObject::tr("Setting %1 rounds for key derivation function.").arg(QString::number(rounds)) << endl;
        kdf->setRounds(rounds);

        bool ok = db->changeKdf(kdf);

        if (!ok) {
            err << QObject::tr("error while setting database key derivation settings.") << endl;
            return {};
        }
    }

    return db;
}

/**
 * Create a database file using the command line. A key file and/or
 * password can be specified to encrypt the password. If none is
 * specified the function will fail.
 *
 * If a key file is specified but it can't be loaded, the function will
 * fail.
 *
 * If the database is being saved in a non existant directory, the
 * function will fail.
 *
 * @return EXIT_SUCCESS on success, or EXIT_FAILURE on failure
 */
int Create::execute(const QStringList& arguments)
{
    QSharedPointer<QCommandLineParser> parser = getCommandLineParser(arguments);
    if (parser.isNull()) {
        return EXIT_FAILURE;
    }

    auto& out = parser->isSet(Command::QuietOption) ? Utils::DEVNULL : Utils::STDOUT;
    auto& err = Utils::STDERR;

    const QStringList args = parser->positionalArguments();

    const QString& databaseFilename = args.at(0);
    if (QFileInfo::exists(databaseFilename)) {
        err << QObject::tr("File %1 already exists.").arg(databaseFilename) << endl;
        return EXIT_FAILURE;
    }

    QSharedPointer<Database> db = Create::initializeDatabaseFromOptions(parser);
    if (!db) {
        return EXIT_FAILURE;
    }

    QString errorMessage;
    if (!db->saveAs(databaseFilename, Database::Atomic, &errorMessage)) {
        err << QObject::tr("Failed to save the database: %1.").arg(errorMessage) << endl;
        return EXIT_FAILURE;
    }

    out << QObject::tr("Successfully created new database.") << endl;
    return EXIT_SUCCESS;
}
