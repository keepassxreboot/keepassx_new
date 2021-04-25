/*
 *  Copyright (C) 2018 KeePassXC Team <team@keepassxc.org>
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

#include "KeeShare.h"
#include "core/CustomData.h"
#include "core/Database.h"
#include "core/Group.h"
#include "core/Metadata.h"
#include "gui/DatabaseIcons.h"
#include "keeshare/ShareObserver.h"
#include "keeshare/Signature.h"

#include <QMessageBox>
#include <QPainter>
#include <QPushButton>

namespace
{
    static const QString KeeShare_Reference("KeeShare/Reference");
}

KeeShare* KeeShare::m_instance = nullptr;

KeeShare* KeeShare::instance()
{
    if (!m_instance) {
        qFatal("Race condition: instance wanted before it was initialized, this is a bug.");
    }

    return m_instance;
}

KeeShare::KeeShare(QObject* parent)
    : QObject(parent)
{
    connect(config(), &Config::changed, this, &KeeShare::handleSettingsChanged);
}

void KeeShare::init(QObject* parent)
{
    Q_ASSERT(!m_instance);
    m_instance = new KeeShare(parent);
}

KeeShareSettings::Own KeeShare::own()
{
    return KeeShareSettings::Own::deserialize(config()->get(Config::KeeShare_Own).toString());
}

KeeShareSettings::Active KeeShare::active()
{
    return KeeShareSettings::Active::deserialize(config()->get(Config::KeeShare_Active).toString());
}

KeeShareSettings::Foreign KeeShare::foreign()
{
    return KeeShareSettings::Foreign::deserialize(config()->get(Config::KeeShare_Foreign).toString());
}

void KeeShare::setForeign(const KeeShareSettings::Foreign& foreign)
{
    config()->set(Config::KeeShare_Foreign, KeeShareSettings::Foreign::serialize(foreign));
}

void KeeShare::setActive(const KeeShareSettings::Active& active)
{
    config()->set(Config::KeeShare_Active, KeeShareSettings::Active::serialize(active));
}

void KeeShare::setOwn(const KeeShareSettings::Own& own)
{
    config()->set(Config::KeeShare_Own, KeeShareSettings::Own::serialize(own));
}

bool KeeShare::isShared(const Group* group)
{
    return group && group->customData()->contains(KeeShare_Reference);
}

KeeShareSettings::Reference KeeShare::referenceOf(const Group* group)
{
    static const KeeShareSettings::Reference s_emptyReference;
    const CustomData* customData = group->customData();
    if (!customData->contains(KeeShare_Reference)) {
        return s_emptyReference;
    }
    const auto encoded = customData->value(KeeShare_Reference);
    const auto serialized = QString::fromUtf8(QByteArray::fromBase64(encoded.toLatin1()));
    KeeShareSettings::Reference reference = KeeShareSettings::Reference::deserialize(serialized);
    if (reference.isNull()) {
        qWarning("Invalid sharing reference detected - sharing disabled");
        return s_emptyReference;
    }
    return reference;
}

void KeeShare::setReferenceTo(Group* group, const KeeShareSettings::Reference& reference)
{
    CustomData* customData = group->customData();
    if (reference.isNull()) {
        customData->remove(KeeShare_Reference);
        return;
    }
    const auto serialized = KeeShareSettings::Reference::serialize(reference);
    customData->set(KeeShare_Reference, serialized.toUtf8().toBase64());
}

bool KeeShare::isEnabled(const Group* group)
{
    const auto reference = KeeShare::referenceOf(group);
#if !defined(WITH_XC_KEESHARE_SECURE)
    if (reference.path.endsWith(signedContainerFileType(), Qt::CaseInsensitive)) {
        return false;
    }
#endif
#if !defined(WITH_XC_KEESHARE_INSECURE)
    if (reference.path.endsWith(unsignedContainerFileType(), Qt::CaseInsensitive)) {
        return false;
    }
#endif
    const auto active = KeeShare::active();
    return (reference.isImporting() && active.in) || (reference.isExporting() && active.out);
}

const Group* KeeShare::resolveSharedGroup(const Group* group)
{
    while (group && group != group->database()->rootGroup()) {
        if (isShared(group)) {
            return group;
        }
        group = group->parentGroup();
    }

    return nullptr;
}

QString KeeShare::sharingLabel(const Group* group)
{
    auto* share = resolveSharedGroup(group);
    if (!share) {
        return {};
    }

    const auto reference = referenceOf(share);
    if (!reference.isValid()) {
        return tr("Invalid sharing reference");
    }
    QStringList messages;
    switch (reference.type) {
    case KeeShareSettings::Inactive:
        messages << tr("Inactive share %1").arg(reference.path);
        break;
    case KeeShareSettings::ImportFrom:
        messages << tr("Imported from %1").arg(reference.path);
        break;
    case KeeShareSettings::ExportTo:
        messages << tr("Exported to %1").arg(reference.path);
        break;
    case KeeShareSettings::SynchronizeWith:
        messages << tr("Synchronized with %1").arg(reference.path);
        break;
    }
    const auto active = KeeShare::active();
    if (reference.isImporting() && !active.in) {
        messages << tr("Import is disabled in settings");
    }
    if (reference.isExporting() && !active.out) {
        messages << tr("Export is disabled in settings");
    }
    return messages.join("\n");
}

QPixmap KeeShare::indicatorBadge(const Group* group, QPixmap pixmap)
{
    if (!isShared(group)) {
        return pixmap;
    }

    if (isEnabled(group)) {
        return databaseIcons()->applyBadge(pixmap, DatabaseIcons::Badges::ShareActive);
    }
    return databaseIcons()->applyBadge(pixmap, DatabaseIcons::Badges::ShareInactive);
}

QString KeeShare::referenceTypeLabel(const KeeShareSettings::Reference& reference)
{
    switch (reference.type) {
    case KeeShareSettings::Inactive:
        return tr("Inactive share");
    case KeeShareSettings::ImportFrom:
        return tr("Imported from");
    case KeeShareSettings::ExportTo:
        return tr("Exported to");
    case KeeShareSettings::SynchronizeWith:
        return tr("Synchronized with");
    }
    return "";
}

QString KeeShare::indicatorSuffix(const Group* group, const QString& text)
{
    // we not adjust the display name for now - it's just an alternative to the icon
    Q_UNUSED(group);
    return text;
}

void KeeShare::connectDatabase(QSharedPointer<Database> newDb, QSharedPointer<Database> oldDb)
{
    if (oldDb && m_observersByDatabase.contains(oldDb->uuid())) {
        QPointer<ShareObserver> observer = m_observersByDatabase.take(oldDb->uuid());
        if (observer) {
            delete observer;
        }
    }

    if (newDb && !m_observersByDatabase.contains(newDb->uuid())) {
        QPointer<ShareObserver> observer(new ShareObserver(newDb, this));
        m_observersByDatabase[newDb->uuid()] = observer;
        connect(observer.data(),
                SIGNAL(sharingMessage(QString, MessageWidget::MessageType)),
                SIGNAL(sharingMessage(QString, MessageWidget::MessageType)));
    }
}

const QString KeeShare::signedContainerFileType()
{
    static const QString filetype("kdbx.share");
    return filetype;
}

const QString KeeShare::unsignedContainerFileType()
{
    static const QString filetype("kdbx");
    return filetype;
}

bool KeeShare::isContainerType(const QFileInfo& fileInfo, const QString type)
{
    return fileInfo.fileName().endsWith(type, Qt::CaseInsensitive);
}

void KeeShare::handleSettingsChanged(Config::ConfigKey key)
{
    if (key == Config::KeeShare_Active) {
        emit activeChanged();
    }
}

const QString KeeShare::signatureFileName()
{
    static const QString fileName("container.share.signature");
    return fileName;
}

const QString KeeShare::containerFileName()
{
    static const QString fileName("container.share.kdbx");
    return fileName;
}
