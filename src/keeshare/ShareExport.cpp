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
#include "ShareExport.h"
#include "config-keepassx.h"
#include "core/Group.h"
#include "core/Metadata.h"
#include "format/KeePass2Writer.h"
#include "gui/Icons.h"
#include "keeshare/KeeShare.h"
#include "keeshare/Signature.h"
#include "keys/PasswordKey.h"

#include <QBuffer>
#include <QTextStream>

#if defined(WITH_XC_KEESHARE_SECURE)
#include <quazip.h>
#include <quazipfile.h>
#endif

namespace
{
    void resolveReferenceAttributes(Entry* targetEntry, const Database* sourceDb)
    {
        for (const auto& attribute : EntryAttributes::DefaultAttributes) {
            const auto standardValue = targetEntry->attributes()->value(attribute);
            const auto type = targetEntry->placeholderType(standardValue);
            if (type != Entry::PlaceholderType::Reference) {
                // No reference to resolve
                continue;
            }
            const auto* referencedTargetEntry = targetEntry->resolveReference(standardValue);
            if (referencedTargetEntry) {
                // References is within scope, no resolving needed
                continue;
            }
            // We could do more sophisticated **** trying to point the reference to the next in-scope reference
            // but those cases with high propability constructed examples and very rare in real usage
            const auto* sourceReference = sourceDb->rootGroup()->findEntryByUuid(targetEntry->uuid());
            const auto resolvedValue = sourceReference->resolveMultiplePlaceholders(standardValue);
            targetEntry->setUpdateTimeinfo(false);
            targetEntry->attributes()->set(attribute, resolvedValue, targetEntry->attributes()->isProtected(attribute));
            targetEntry->setUpdateTimeinfo(true);
        }
    }

    Database* extractIntoDatabase(const KeeShareSettings::Reference& reference, const Group* sourceRoot)
    {
        const auto* sourceDb = sourceRoot->database();
        auto* targetDb = new Database();
        auto* targetMetadata = targetDb->metadata();
        targetMetadata->setRecycleBinEnabled(false);
        auto key = QSharedPointer<CompositeKey>::create();
        key->addKey(QSharedPointer<PasswordKey>::create(reference.password));

        // Copy the source root as the root of the export database, memory manage the old root node
        auto* targetRoot = sourceRoot->clone(Entry::CloneNoFlags, Group::CloneNoFlags);
        const bool updateTimeinfo = targetRoot->canUpdateTimeinfo();
        targetRoot->setUpdateTimeinfo(false);
        KeeShare::setReferenceTo(targetRoot, KeeShareSettings::Reference());
        targetRoot->setUpdateTimeinfo(updateTimeinfo);
        const auto sourceEntries = sourceRoot->entriesRecursive(false);
        for (const Entry* sourceEntry : sourceEntries) {
            auto* targetEntry = sourceEntry->clone(Entry::CloneIncludeHistory);
            const bool updateTimeinfoEntry = targetEntry->canUpdateTimeinfo();
            targetEntry->setUpdateTimeinfo(false);
            targetEntry->setGroup(targetRoot);
            targetEntry->setUpdateTimeinfo(updateTimeinfoEntry);
            const auto iconUuid = targetEntry->iconUuid();
            if (!iconUuid.isNull() && !targetMetadata->hasCustomIcon(iconUuid)) {
                targetMetadata->addCustomIcon(iconUuid, sourceEntry->database()->metadata()->customIcon(iconUuid));
            }
        }

        targetDb->setKey(key);
        auto* obsoleteRoot = targetDb->rootGroup();
        targetDb->setRootGroup(targetRoot);
        delete obsoleteRoot;

        targetDb->metadata()->setName(sourceRoot->name());

        // Push all deletions of the source database to the target
        // simple moving out of a share group will not trigger a deletion in the
        // target - a more elaborate mechanism may need the use of another custom
        // attribute to share unshared entries from the target db
        for (const auto& object : sourceDb->deletedObjects()) {
            targetDb->addDeletedObject(object);
        }
        for (auto* targetEntry : targetRoot->entriesRecursive(false)) {
            if (targetEntry->hasReferences()) {
                resolveReferenceAttributes(targetEntry, sourceDb);
            }
        }
        return targetDb;
    }

    ShareObserver::Result
    intoSignedContainer(const QString& resolvedPath, const KeeShareSettings::Reference& reference, Database* targetDb)
    {
#if !defined(WITH_XC_KEESHARE_SECURE)
        Q_UNUSED(targetDb);
        Q_UNUSED(resolvedPath);
        return {reference.path,
                ShareObserver::Result::Warning,
                ShareExport::tr("Overwriting signed share container is not supported - export prevented")};
#else
        QByteArray bytes;
        {
            QBuffer buffer(&bytes);
            buffer.open(QIODevice::WriteOnly);
            KeePass2Writer writer;
            writer.writeDatabase(&buffer, targetDb);
            if (writer.hasError()) {
                qWarning("Serializing export dabase failed: %s.", writer.errorString().toLatin1().data());
                return {reference.path, ShareObserver::Result::Error, writer.errorString()};
            }
        }
        const auto own = KeeShare::own();
        QuaZip zip(resolvedPath);
        zip.setFileNameCodec("UTF-8");
        const bool zipOpened = zip.open(QuaZip::mdCreate);
        if (!zipOpened) {
            ::qWarning("Opening export file failed: %d", zip.getZipError());
            return {reference.path,
                    ShareObserver::Result::Error,
                    ShareExport::tr("Could not write export container (%1)").arg(zip.getZipError())};
        }
        {
            QuaZipFile file(&zip);
            const auto signatureOpened = file.open(QIODevice::WriteOnly, QuaZipNewInfo(KeeShare::signatureFileName()));
            if (!signatureOpened) {
                ::qWarning("Embedding signature failed: Could not open file to write (%d)", zip.getZipError());
                return {reference.path,
                        ShareObserver::Result::Error,
                        ShareExport::tr("Could not embed signature: Could not open file to write (%1)")
                            .arg(file.getZipError())};
            }
            QTextStream stream(&file);
            KeeShareSettings::Sign sign;
            // TODO: check for false return
            Signature::create(bytes, own.key.key, sign.signature);
            sign.certificate = own.certificate;
            stream << KeeShareSettings::Sign::serialize(sign);
            stream.flush();
            if (file.getZipError() != ZIP_OK) {
                ::qWarning("Embedding signature failed: Could not write file (%d)", zip.getZipError());
                return {
                    reference.path,
                    ShareObserver::Result::Error,
                    ShareExport::tr("Could not embed signature: Could not write file (%1)").arg(file.getZipError())};
            }
            file.close();
        }
        {
            QuaZipFile file(&zip);
            const auto dbOpened = file.open(QIODevice::WriteOnly, QuaZipNewInfo(KeeShare::containerFileName()));
            if (!dbOpened) {
                ::qWarning("Embedding database failed: Could not open file to write (%d)", zip.getZipError());
                return {reference.path,
                        ShareObserver::Result::Error,
                        ShareExport::tr("Could not embed database: Could not open file to write (%1)")
                            .arg(file.getZipError())};
            }
            file.write(bytes);
            if (file.getZipError() != ZIP_OK) {
                ::qWarning("Embedding database failed: Could not write file (%d)", zip.getZipError());
                return {reference.path,
                        ShareObserver::Result::Error,
                        ShareExport::tr("Could not embed database: Could not write file (%1)").arg(file.getZipError())};
            }
            file.close();
        }
        zip.close();
        return {reference.path};
#endif
    }

    ShareObserver::Result
    intoUnsignedContainer(const QString& resolvedPath, const KeeShareSettings::Reference& reference, Database* targetDb)
    {
#if !defined(WITH_XC_KEESHARE_INSECURE)
        Q_UNUSED(targetDb);
        Q_UNUSED(resolvedPath);
        return {reference.path,
                ShareObserver::Result::Warning,
                ShareExport::tr("Overwriting unsigned share container is not supported - export prevented")};
#else
        QFile file(resolvedPath);
        const bool fileOpened = file.open(QIODevice::WriteOnly);
        if (!fileOpened) {
            ::qWarning("Opening export file failed");
            return {reference.path, ShareObserver::Result::Error, ShareExport::tr("Could not write export container")};
        }
        KeePass2Writer writer;
        writer.writeDatabase(&file, targetDb);
        if (writer.hasError()) {
            qWarning("Exporting dabase failed: %s.", writer.errorString().toLatin1().data());
            return {reference.path, ShareObserver::Result::Error, writer.errorString()};
        }
        file.close();
#endif
        return {reference.path};
    }

} // namespace

ShareObserver::Result ShareExport::intoContainer(const QString& resolvedPath,
                                                 const KeeShareSettings::Reference& reference,
                                                 const Group* group)
{
    QScopedPointer<Database> targetDb(extractIntoDatabase(reference, group));
    const QFileInfo info(resolvedPath);
    if (KeeShare::isContainerType(info, KeeShare::signedContainerFileType())) {
        return intoSignedContainer(resolvedPath, reference, targetDb.data());
    }
    return intoUnsignedContainer(resolvedPath, reference, targetDb.data());
}
