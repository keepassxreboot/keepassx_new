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

#ifndef KEEPASSX_DETAILSWIDGET_H
#define KEEPASSX_DETAILSWIDGET_H

#include "config-keepassx.h"
#include "gui/DatabaseWidget.h"
#include "gui/TotpLineEdit.h"
#include "totp/totp.h"

#include <QWidget>

namespace Ui
{
    class EntryPreviewWidget;
}

class QTextEdit;
class EntryPreviewWidget : public QWidget
{
    Q_OBJECT

public:
    explicit EntryPreviewWidget(QWidget* parent = nullptr);
    ~EntryPreviewWidget() override;

public slots:
    void setEntry(Entry* selectedEntry);
    void setGroup(Group* selectedGroup);
    void setDatabaseMode(DatabaseWidget::Mode mode);

signals:
    void errorOccurred(const QString& error);
    void entryUrlActivated(Entry* entry);

private slots:
    void updateEntryHeaderLine();
    void updateEntryTotp();
    void updateEntryGeneralTab();
    void updateEntryAdvancedTab();
    void updateEntryAutotypeTab();
    void setPasswordVisible(bool state);
    void setEntryNotesVisible(bool state);
    void setGroupNotesVisible(bool state);
    void setNotesVisible(QTextEdit* notesWidget, const QString& notes, bool state);
    void setEntryOtpVisible(bool state);

    void updateGroupHeaderLine();
    void updateGroupGeneralTab();
#if defined(WITH_XC_KEESHARE)
    void updateGroupSharingTab();
#endif

    void updateTabIndexes();
    void openEntryUrl();
    void updateTotpWidgets();

private:
    void removeTab(QTabWidget* tabWidget, QWidget* widget);
    void setTabEnabled(QTabWidget* tabWidget, QWidget* widget, bool enabled);
    void setTotpCode();
    void setTotpRemainingSecondsLabel();

    static QString hierarchy(const Group* group, const QString& title);

    const QScopedPointer<Ui::EntryPreviewWidget> m_ui;
    bool m_locked;
    QPointer<Entry> m_currentEntry;
    QPointer<Group> m_currentGroup;
    QPointer<TotpLineEdit> m_totpLineEdit;

    QTimer m_totpTimer;
    quint8 m_selectedTabEntry;
    quint8 m_selectedTabGroup;

    uint m_totpStep;
    bool m_hasTotp;
};

#endif // KEEPASSX_DETAILSWIDGET_H
