/*
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

#ifndef KEEPASSX_DATABASESETTINGSWIDGET_H
#define KEEPASSX_DATABASESETTINGSWIDGET_H

#include "config-keepassx.h"
#include "gui/DialogyWidget.h"

#include <QPointer>
#include <QScopedPointer>
#include <QSharedPointer>

class Database;
class DatabaseSettingsWidgetGeneral;
class DatabaseSettingsWidgetEncryption;
class DatabaseSettingsWidgetDatabaseKey;
#ifdef WITH_XC_BROWSER
class DatabaseSettingsWidgetBrowser;
#endif
class DatabaseSettingsWidgetMaintenance;
class QTabWidget;

namespace Ui
{
    class DatabaseSettingsDialog;
}

class IDatabaseSettingsPage
{
public:
    virtual ~IDatabaseSettingsPage()
    {
    }
    virtual QString name() = 0;
    virtual QIcon icon() = 0;
    virtual QWidget* createWidget() = 0;
    virtual void loadSettings(QWidget* widget, QSharedPointer<Database> db) = 0;
    virtual void saveSettings(QWidget* widget) = 0;
};

class DatabaseSettingsDialog : public DialogyWidget
{
    Q_OBJECT

public:
    explicit DatabaseSettingsDialog(QWidget* parent = nullptr);
    ~DatabaseSettingsDialog() override;
    Q_DISABLE_COPY(DatabaseSettingsDialog);

    void load(const QSharedPointer<Database>& db);
    void addSettingsPage(IDatabaseSettingsPage* page);
    void showDatabaseKeySettings();

signals:
    void editFinished(bool accepted);

private slots:
    void save();
    void reject();
    void pageChanged();

private:
    enum Page
    {
        General = 0,
        Security = 1
    };

    QSharedPointer<Database> m_db;
    const QScopedPointer<Ui::DatabaseSettingsDialog> m_ui;
    QPointer<DatabaseSettingsWidgetGeneral> m_generalWidget;
    QPointer<QTabWidget> m_securityTabWidget;
    QPointer<DatabaseSettingsWidgetDatabaseKey> m_databaseKeyWidget;
    QPointer<DatabaseSettingsWidgetEncryption> m_encryptionWidget;
#ifdef WITH_XC_BROWSER
    QPointer<DatabaseSettingsWidgetBrowser> m_browserWidget;
#endif
    QPointer<DatabaseSettingsWidgetMaintenance> m_maintenanceWidget;

    class ExtraPage;
    QList<ExtraPage> m_extraPages;
};

#endif // KEEPASSX_DATABASESETTINGSWIDGET_H
