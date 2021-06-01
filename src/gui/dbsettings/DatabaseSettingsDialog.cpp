/*
 *  Copyright (C) 2018 KeePassXC Team <team@keepassxc.org>
 *  Copyright (C) 2012 Felix Geyer <debfx@fobos.de>
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

#include "DatabaseSettingsDialog.h"
#include "ui_DatabaseSettingsDialog.h"

#include "DatabaseSettingsWidgetDatabaseKey.h"
#include "DatabaseSettingsWidgetEncryption.h"
#include "DatabaseSettingsWidgetGeneral.h"
#ifdef WITH_XC_BROWSER
#include "DatabaseSettingsWidgetBrowser.h"
#endif
#include "DatabaseSettingsWidgetMaintenance.h"
#if defined(WITH_XC_KEESHARE)
#include "keeshare/DatabaseSettingsPageKeeShare.h"
#endif
#if defined(WITH_XC_FDOSECRETS)
#include "fdosecrets/DatabaseSettingsPageFdoSecrets.h"
#endif

#include "core/Database.h"
#include "core/Global.h"
#include "gui/Icons.h"
#include "touchid/TouchID.h"

#include <QScrollArea>

class DatabaseSettingsDialog::ExtraPage
{
public:
    ExtraPage(IDatabaseSettingsPage* page, QWidget* widget)
        : settingsPage(page)
        , widget(widget)
    {
    }
    void loadSettings(QSharedPointer<Database> db) const
    {
        settingsPage->loadSettings(widget, db);
    }
    void saveSettings() const
    {
        settingsPage->saveSettings(widget);
    }

private:
    QSharedPointer<IDatabaseSettingsPage> settingsPage;
    QWidget* widget;
};

DatabaseSettingsDialog::DatabaseSettingsDialog(QWidget* parent)
    : DialogyWidget(parent)
    , m_ui(new Ui::DatabaseSettingsDialog())
    , m_generalWidget(new DatabaseSettingsWidgetGeneral(this))
    , m_securityTabWidget(new QTabWidget(this))
    , m_databaseKeyWidget(new DatabaseSettingsWidgetDatabaseKey(this))
    , m_encryptionWidget(new DatabaseSettingsWidgetEncryption(this))
#ifdef WITH_XC_BROWSER
    , m_browserWidget(new DatabaseSettingsWidgetBrowser(this))
#endif
    , m_maintenanceWidget(new DatabaseSettingsWidgetMaintenance(this))
{
    m_ui->setupUi(this);

    connect(m_ui->buttonBox, SIGNAL(accepted()), SLOT(save()));
    connect(m_ui->buttonBox, SIGNAL(rejected()), SLOT(reject()));

    m_ui->categoryList->addCategory(tr("General"), icons()->icon("preferences-other"));
    m_ui->categoryList->addCategory(tr("Security"), icons()->icon("security-high"));
    m_ui->stackedWidget->addWidget(m_generalWidget);

    m_ui->stackedWidget->addWidget(m_securityTabWidget);

    auto* scrollArea = new QScrollArea(parent);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setFrameShadow(QFrame::Plain);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setSizeAdjustPolicy(QScrollArea::AdjustToContents);
    scrollArea->setWidgetResizable(true);
    scrollArea->setWidget(m_databaseKeyWidget);
    m_securityTabWidget->addTab(scrollArea, tr("Database Credentials"));

    m_securityTabWidget->addTab(m_encryptionWidget, tr("Encryption Settings"));

#if defined(WITH_XC_KEESHARE)
    addSettingsPage(new DatabaseSettingsPageKeeShare());
#endif

#if defined(WITH_XC_FDOSECRETS)
    addSettingsPage(new DatabaseSettingsPageFdoSecrets());
#endif

    m_ui->stackedWidget->setCurrentIndex(0);
    m_securityTabWidget->setCurrentIndex(0);

    connect(m_securityTabWidget, SIGNAL(currentChanged(int)), SLOT(pageChanged()));
    connect(m_ui->categoryList, SIGNAL(categoryChanged(int)), m_ui->stackedWidget, SLOT(setCurrentIndex(int)));

#ifdef WITH_XC_BROWSER
    m_ui->categoryList->addCategory(tr("Browser Integration"), icons()->icon("internet-web-browser"));
    m_ui->stackedWidget->addWidget(m_browserWidget);
#endif

    m_ui->categoryList->addCategory(tr("Maintenance"), icons()->icon("hammer-wrench"));
    m_ui->stackedWidget->addWidget(m_maintenanceWidget);

    pageChanged();
}

DatabaseSettingsDialog::~DatabaseSettingsDialog()
{
}

void DatabaseSettingsDialog::load(const QSharedPointer<Database>& db)
{
    m_ui->categoryList->setCurrentCategory(0);
    m_generalWidget->load(db);
    m_databaseKeyWidget->load(db);
    m_encryptionWidget->load(db);
#ifdef WITH_XC_BROWSER
    m_browserWidget->load(db);
#endif
    m_maintenanceWidget->load(db);
    for (const ExtraPage& page : asConst(m_extraPages)) {
        page.loadSettings(db);
    }
    m_db = db;
}

void DatabaseSettingsDialog::addSettingsPage(IDatabaseSettingsPage* page)
{
    const int category = m_ui->categoryList->currentCategory();
    QWidget* widget = page->createWidget();
    widget->setParent(this);
    m_extraPages.append(ExtraPage(page, widget));
    m_ui->stackedWidget->addWidget(widget);
    m_ui->categoryList->addCategory(page->name(), page->icon());
    m_ui->categoryList->setCurrentCategory(category);
}

/**
 * Show page and tab with database database key settings.
 */
void DatabaseSettingsDialog::showDatabaseKeySettings()
{
    m_ui->categoryList->setCurrentCategory(1);
    m_securityTabWidget->setCurrentIndex(0);
}

void DatabaseSettingsDialog::save()
{
    if (!m_generalWidget->save()) {
        return;
    }

    if (!m_databaseKeyWidget->save()) {
        return;
    }

    if (!m_encryptionWidget->save()) {
        return;
    }

    for (const ExtraPage& extraPage : asConst(m_extraPages)) {
        extraPage.saveSettings();
    }

#ifdef WITH_XC_TOUCHID
    TouchID::getInstance().reset(m_db ? m_db->filePath() : "");
#endif

    emit editFinished(true);
}

void DatabaseSettingsDialog::reject()
{
    emit editFinished(false);
}

void DatabaseSettingsDialog::pageChanged()
{
    m_ui->stackedWidget->currentIndex();
}
