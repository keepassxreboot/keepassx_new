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

#include "AttachmentImport.h"

#include "core/Group.h"

#include <QFile>

const QCommandLineOption AttachmentImport::ForceOption =
    QCommandLineOption(QStringList() << "f"
                                     << "force",
                       QObject::tr("Overwrite existing attachments."));

AttachmentImport::AttachmentImport()
{
    name = QString("attachment-import");
    description = QObject::tr("Imports an attachment to an entry.");
    options.append(AttachmentImport::ForceOption);
    positionalArguments.append({QString("entry"), QObject::tr("Path of the entry."), QString("")});
    positionalArguments.append({QString("name"), QObject::tr("Name of the attachment to be added."), QString("")});
    positionalArguments.append(
        {QString("attachment"), QObject::tr("Path of the attachment to be added."), QString("")});
}

int AttachmentImport::executeWithDatabase(QSharedPointer<Database> database, QSharedPointer<QCommandLineParser> parser)
{
    auto& out = parser->isSet(Command::QuietOption) ? Utils::DEVNULL : Utils::STDOUT;
    auto& err = Utils::STDERR;

    const QStringList args = parser->positionalArguments();
    const QString entryPath = args.at(1);

    Entry* entry = database->rootGroup()->findEntryByPath(entryPath);
    if (!entry) {
        err << QObject::tr("Could not find entry with path %1.").arg(entryPath) << endl;
        return EXIT_FAILURE;
    }

    const QString attachmentName = args.at(2);

    EntryAttachments* attachments = entry->attachments();
    if (attachments->hasKey(attachmentName) && !parser->isSet(AttachmentImport::ForceOption)) {
        err << QObject::tr("Attachment %1 already exists for entry %2.").arg(attachmentName, entryPath) << endl;
        return EXIT_FAILURE;
    }

    const QString attachmentPath = args.at(3);

    QFile in(attachmentPath);
    if (!in.open(QIODevice::ReadOnly)) {
        err << QObject::tr("Could not open attachment file %1.").arg(attachmentPath) << endl;
        return EXIT_FAILURE;
    }

    entry->beginUpdate();
    attachments->set(attachmentName, in.readAll());
    entry->endUpdate();

    QString errorMessage;
    if (!database->save(&errorMessage, true, false)) {
        err << QObject::tr("Writing the database failed %1.").arg(errorMessage) << endl;
        return EXIT_FAILURE;
    }

    out << QObject::tr("Successfully imported attachment %1 as %2 to entry %3.")
               .arg(attachmentPath, attachmentName, entryPath)
        << endl;
    return EXIT_SUCCESS;
}
