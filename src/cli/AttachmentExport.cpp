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

#include "AttachmentExport.h"

#include "core/Group.h"

#include <QFile>

AttachmentExport::AttachmentExport()
{
    name = QString("attachment-export");
    description = QObject::tr("Export an attachment of an entry.");
    positionalArguments.append({QString("entry"), QObject::tr("Path of the entry with the target attachment."), QString("")});
    positionalArguments.append({QString("name"), QObject::tr("Name of the attachment to be exported."), QString("")});
}

int AttachmentExport::executeWithDatabase(QSharedPointer<Database> database, QSharedPointer<QCommandLineParser> parser)
{
    auto& err = Utils::STDERR;

    const QStringList args = parser->positionalArguments();
    const QString entryPath = args.at(1);

    Entry* entry = database->rootGroup()->findEntryByPath(entryPath);
    if (!entry) {
        err << QObject::tr("Could not find entry with path %1.").arg(entryPath) << endl;
        return EXIT_FAILURE;
    }

    const QString attachmentName = args.at(2);

    const EntryAttachments* attachments = entry->attachments();
    if (!attachments->hasKey(attachmentName)) {
        err << QObject::tr("Could not find attachment with name %1.").arg(attachmentName) << endl;
        return EXIT_FAILURE;
    }

    QFile out;
    if (!out.open(stdout, QIODevice::WriteOnly)) {
        err << QObject::tr("Could not open stdout.") << endl;
        return EXIT_FAILURE;
    }

    const QByteArray attachment = attachments->value(attachmentName);
    out.write(attachment);

    return EXIT_SUCCESS;
}
