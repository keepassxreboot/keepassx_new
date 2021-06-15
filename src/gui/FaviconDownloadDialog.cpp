/*
 *  Copyright (C) 2017 KeePassXC Team <team@keepassxc.org>
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

#include "FaviconDownloadDialog.h"
#include "ui_FaviconDownloadDialog.h"

#include "core/Config.h"
#include "core/Group.h"
#include "core/Tools.h"
#include "gui/IconModels.h"
#include "gui/MessageBox.h"

#ifdef WITH_XC_NETWORKING
#include "gui/IconDownloader.h"
#endif

FaviconDownloadDialog::FaviconDownloadDialog(EditWidgetIcons* parent)
    : QDialog(parent)
    , m_ui(new Ui::FaviconDownloadDialog)
{
    m_ui->setupUi(this);
#ifdef WITH_XC_NETWORKING
    m_downloader = parent->m_downloader;
    m_ui->inputtedURL->setText(parent->m_url);
#endif
    setFixedSize(this->size());
    setAttribute(Qt::WA_DeleteOnClose);

    connect(m_ui->buttonBox, SIGNAL(rejected()), SLOT(close()));
    connect(m_ui->buttonBox, SIGNAL(accepted()), SLOT(downloadFavicon()));
    connect(m_downloader.data(), SIGNAL(finished(const QString&, const QImage&)), SLOT(close()));
}

void FaviconDownloadDialog::downloadFavicon()
{
#ifdef WITH_XC_NETWORKING
    QString url = m_ui->inputtedURL->text();
    if (!url.isEmpty()) {
        m_downloader->setUrl(url);
        m_downloader->download();
    }
#endif
}

FaviconDownloadDialog::~FaviconDownloadDialog()
{
}
