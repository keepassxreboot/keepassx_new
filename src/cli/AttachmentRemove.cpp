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

#include "AttachmentRemove.h"

#include "core/Group.h"

AttachmentRemove::AttachmentRemove()
{
    name = QString("attachment-rm");
    description = QObject::tr("Remove an attachment of an entry.");
    positionalArguments.append(
        {QString("entry"), QObject::tr("Path of the entry with the target attachment."), QString("")});
    positionalArguments.append({QString("name"), QObject::tr("Name of the attachment to be removed."), QString("")});
}

int AttachmentRemove::executeWithDatabase(QSharedPointer<Database> database, QSharedPointer<QCommandLineParser> parser)
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
    if (!attachments->hasKey(attachmentName)) {
        err << QObject::tr("Could not find attachment with name %1.").arg(attachmentName) << endl;
        return EXIT_FAILURE;
    }

    entry->beginUpdate();
    attachments->remove(attachmentName);
    entry->endUpdate();

    QString errorMessage;
    if (!database->save(&errorMessage, true, false)) {
        err << QObject::tr("Writing the database failed %1.").arg(errorMessage) << endl;
        return EXIT_FAILURE;
    }

    out << QObject::tr("Successfully removed attachment %1 from entry %2.").arg(attachmentName, entryPath) << endl;
    return EXIT_SUCCESS;
}
