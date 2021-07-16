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

#include "Remove.h"

#include "Utils.h"
#include "core/Group.h"
#include "core/Metadata.h"

#include <QCommandLineParser>

Remove::Remove()
{
    name = QString("rm");
    description = QString("Remove an entry from the database.");
    positionalArguments.append({QString("entry"), QObject::tr("Path of the entry to remove."), QString("")});
}

int Remove::executeWithDatabase(QSharedPointer<Database> database, QSharedPointer<QCommandLineParser> parser)
{
    auto& out = parser->isSet(Command::QuietOption) ? Utils::DEVNULL : Utils::STDOUT;
    auto& err = Utils::STDERR;

    auto& entryPath = parser->positionalArguments().at(1);
    QPointer<Entry> entry = database->rootGroup()->findEntryByPath(entryPath);
    if (!entry) {
        err << QObject::tr("Entry %1 not found.").arg(entryPath) << endl;
        return EXIT_FAILURE;
    }

    QString entryTitle = entry->title();
    bool recycled = true;
    auto* recycleBin = database->metadata()->recycleBin();
    if (!database->metadata()->recycleBinEnabled() || (recycleBin && recycleBin->findEntryByUuid(entry->uuid()))) {
        delete entry;
        recycled = false;
    } else {
        database->recycleEntry(entry);
    };

    QString errorMessage;
    if (!database->save(Database::Atomic, &errorMessage)) {
        err << QObject::tr("Unable to save database to file: %1").arg(errorMessage) << endl;
        return EXIT_FAILURE;
    }

    if (recycled) {
        out << QObject::tr("Successfully recycled entry %1.").arg(entryTitle) << endl;
    } else {
        out << QObject::tr("Successfully deleted entry %1.").arg(entryTitle) << endl;
    }

    return EXIT_SUCCESS;
}
