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

#include "IconDownloaderDialog.h"
#include "ui_IconDownloaderDialog.h"

#include "core/AsyncTask.h"
#include "core/Config.h"
#include "core/Entry.h"
#include "core/Global.h"
#include "core/Group.h"
#include "core/Metadata.h"
#include "core/Tools.h"
#include "gui/IconDownloader.h"
#include "gui/IconModels.h"
#include "gui/Icons.h"
#ifdef Q_OS_MACOS
#include "gui/osutils/macutils/MacUtils.h"
#endif

#include <QMutexLocker>

IconDownloaderDialog::IconDownloaderDialog(QWidget* parent)
    : QDialog(parent)
    , m_ui(new Ui::IconDownloaderDialog())
    , m_dataModel(new QStandardItemModel(this))
{
    setWindowFlags(Qt::Window);
    setAttribute(Qt::WA_DeleteOnClose);

    m_ui->setupUi(this);
    showFallbackMessage(false);

    m_dataModel->clear();
    m_dataModel->setHorizontalHeaderLabels({tr("URL"), tr("Status")});

    m_ui->tableView->setModel(m_dataModel);
    m_ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    connect(m_ui->cancelButton, SIGNAL(clicked()), SLOT(abortDownloads()));
    connect(m_ui->closeButton, SIGNAL(clicked()), SLOT(close()));
}

IconDownloaderDialog::~IconDownloaderDialog()
{
    abortDownloads();
}

void IconDownloaderDialog::downloadFavicons(const QSharedPointer<Database>& database,
                                            const QList<Entry*>& entries,
                                            bool force)
{
    m_db = database;
    m_urlToEntries.clear();
    abortDownloads();
    for (const auto& e : entries) {
        // Only consider entries with a valid URL and without a custom icon
        auto webUrl = e->webUrl();
        if (!webUrl.isEmpty() && (force || e->iconUuid().isNull())) {
            m_urlToEntries.insert(webUrl, e);
        }
    }

    if (m_urlToEntries.count() > 0) {
#ifdef Q_OS_MACOS
        macUtils()->raiseOwnWindow();
        Tools::wait(100);
#endif
        showFallbackMessage(false);
        m_ui->progressLabel->setText(tr("Please wait, processing entry list…"));
        open();
        QApplication::processEvents();

        for (const auto& url : m_urlToEntries.uniqueKeys()) {
            m_dataModel->appendRow(QList<QStandardItem*>()
                                   << new QStandardItem(url) << new QStandardItem(tr("Downloading…")));
            m_activeDownloaders.append(createDownloader(url));
        }

        // Setup the dialog
        updateProgressBar();
        updateCancelButton();
        QApplication::processEvents();

        // Start the downloads
        for (auto downloader : m_activeDownloaders) {
            downloader->download();
        }
    }
}

IconDownloader* IconDownloaderDialog::createDownloader(const QString& url)
{
    auto downloader = new IconDownloader();
    connect(downloader,
            SIGNAL(finished(const QString&, const QImage&)),
            this,
            SLOT(downloadFinished(const QString&, const QImage&)));

    downloader->setUrl(url);
    return downloader;
}

void IconDownloaderDialog::downloadFinished(const QString& url, const QImage& icon)
{
    // Prevent re-entrance from multiple calls finishing at the same time
    QMutexLocker locker(&m_mutex);

    // Cleanup the icon downloader that sent this signal
    auto downloader = qobject_cast<IconDownloader*>(sender());
    if (downloader) {
        downloader->deleteLater();
        m_activeDownloaders.removeAll(downloader);
    }

    updateProgressBar();
    updateCancelButton();

    if (m_db && !icon.isNull()) {
        // Don't add an icon larger than 128x128, but retain original size if smaller
        auto scaledIcon = icon;
        if (icon.width() > 128 || icon.height() > 128) {
            scaledIcon = icon.scaled(128, 128);
        }

        QByteArray serializedIcon = Icons::saveToBytes(scaledIcon);
        QUuid uuid = m_db->metadata()->findCustomIcon(serializedIcon);
        if (uuid.isNull()) {
            uuid = QUuid::createUuid();
            m_db->metadata()->addCustomIcon(uuid, serializedIcon);
            updateTable(url, tr("Ok"));
        } else {
            updateTable(url, tr("Already Exists"));
        }

        // Set the icon on all the entries associated with this url
        for (const auto entry : m_urlToEntries.values(url)) {
            entry->setIcon(uuid);
        }
    } else {
        showFallbackMessage(true);
        updateTable(url, tr("Download Failed"));
        return;
    }
}

void IconDownloaderDialog::showFallbackMessage(bool state)
{
    // Show fallback message if the option is not active
    bool show = state && !config()->get(Config::Security_IconDownloadFallback).toBool();
    m_ui->fallbackLabel->setVisible(show);
}

void IconDownloaderDialog::updateProgressBar()
{
    int total = m_urlToEntries.uniqueKeys().count();
    int value = total - m_activeDownloaders.count();
    m_ui->progressBar->setValue(value);
    m_ui->progressBar->setMaximum(total);
    m_ui->progressLabel->setText(
        tr("Downloading favicons (%1/%2)…").arg(QString::number(value), QString::number(total)));
}

void IconDownloaderDialog::updateCancelButton()
{
    m_ui->cancelButton->setEnabled(!m_activeDownloaders.isEmpty());
}

void IconDownloaderDialog::updateTable(const QString& url, const QString& message)
{
    for (int i = 0; i < m_dataModel->rowCount(); ++i) {
        if (m_dataModel->item(i, 0)->text() == url) {
            m_dataModel->item(i, 1)->setText(message);
        }
    }
}

void IconDownloaderDialog::abortDownloads()
{
    for (auto* downloader : m_activeDownloaders) {
        delete downloader;
    }
    m_activeDownloaders.clear();
    updateProgressBar();
    updateCancelButton();
}
