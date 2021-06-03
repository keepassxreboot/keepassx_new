/*
 *  Copyright (C) 2017 KeePassXC Team <team@keepassxc.org>
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

#include "Clipboard.h"

#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QTimer>

#include "core/Config.h"

Clipboard* Clipboard::m_instance(nullptr);
#ifdef Q_OS_MACOS
QPointer<MacPasteboard> Clipboard::m_pasteboard(nullptr);
#endif

Clipboard::Clipboard(QObject* parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
{
#ifdef Q_OS_MACOS
    if (!m_pasteboard) {
        m_pasteboard = new MacPasteboard();
    }
#endif
    connect(m_timer, SIGNAL(timeout()), SLOT(countdownTick()));
    connect(qApp, SIGNAL(aboutToQuit()), SLOT(clearCopiedText()));
}

void Clipboard::setText(const QString& text, bool clear)
{
    auto* clipboard = QApplication::clipboard();
    if (!clipboard) {
        qWarning("Unable to access the clipboard.");
        return;
    }

    auto* mime = new QMimeData;
#ifdef Q_OS_MACOS
    mime->setText(text);
    mime->setData("application/x-nspasteboard-concealed-type", text.toUtf8());
    clipboard->setMimeData(mime, QClipboard::Clipboard);
#else
    mime->setText(text);
#ifdef Q_OS_LINUX
    mime->setData("x-kde-passwordManagerHint", QByteArrayLiteral("secret"));
#endif
#ifdef Q_OS_WIN
    mime->setData("ExcludeClipboardContentFromMonitorProcessing", QByteArrayLiteral("1"));
#endif
    clipboard->setMimeData(mime, QClipboard::Clipboard);

    if (clipboard->supportsSelection()) {
        clipboard->setMimeData(mime, QClipboard::Selection);
    }
#endif

    if (clear) {
        m_lastCopied = text;
        if (config()->get(Config::Security_ClearClipboard).toBool()) {
            int timeout = config()->get(Config::Security_ClearClipboardTimeout).toInt();
            if (timeout > 0) {
                m_secondsElapsed = -1;
                countdownTick();
                m_timer->start(1000);
            }
        }
    }
}

int Clipboard::secondsElapsed()
{
    return m_secondsElapsed;
}

void Clipboard::clearCopiedText()
{
    m_timer->stop();
    emit updateCountdown(-1, "");

    auto* clipboard = QApplication::clipboard();
    if (!clipboard) {
        qWarning("Unable to access the clipboard.");
        return;
    }

    if (m_lastCopied == clipboard->text(QClipboard::Clipboard)
        || m_lastCopied == clipboard->text(QClipboard::Selection)) {
        setText("", false);
    }

    m_lastCopied.clear();
}

void Clipboard::countdownTick()
{
    m_secondsElapsed++;
    int timeout = config()->get(Config::Security_ClearClipboardTimeout).toInt();
    int timeLeft = timeout - m_secondsElapsed;
    if (timeLeft <= 0) {
        clearCopiedText();
    } else {
        emit updateCountdown(100 * timeLeft / timeout,
                             QObject::tr("Clearing the clipboard in %1 second(s)…", "", timeLeft).arg(timeLeft));
    }
}

Clipboard* Clipboard::instance()
{
    if (!m_instance) {
        m_instance = new Clipboard(qApp);
    }

    return m_instance;
}
