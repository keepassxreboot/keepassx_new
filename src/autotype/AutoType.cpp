/*
 *  Copyright (C) 2012 Felix Geyer <debfx@fobos.de>
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

#include "AutoType.h"

#include <QApplication>
#include <QPluginLoader>
#include <QWindow>

#include "config-keepassx.h"

#include "autotype/AutoTypePlatformPlugin.h"
#include "autotype/AutoTypeSelectDialog.h"
#include "autotype/PickcharsDialog.h"
#include "core/Config.h"
#include "core/Database.h"
#include "core/Entry.h"
#include "core/Group.h"
#include "core/Resources.h"
#include "core/Tools.h"
#include "gui/MainWindow.h"
#include "gui/MessageBox.h"
#include "gui/osutils/OSUtils.h"

namespace
{
    // Basic Auto-Type placeholder associations
    const QHash<QString, Qt::Key> g_placeholderToKey = {{"tab", Qt::Key_Tab},
                                                        {"enter", Qt::Key_Enter},
                                                        {"space", Qt::Key_Space},
                                                        {"up", Qt::Key_Up},
                                                        {"down", Qt::Key_Down},
                                                        {"left", Qt::Key_Left},
                                                        {"right", Qt::Key_Right},
                                                        {"insert", Qt::Key_Insert},
                                                        {"ins", Qt::Key_Insert},
                                                        {"delete", Qt::Key_Delete},
                                                        {"del", Qt::Key_Delete},
                                                        {"home", Qt::Key_Home},
                                                        {"end", Qt::Key_End},
                                                        {"pgup", Qt::Key_PageUp},
                                                        {"pgdown", Qt::Key_PageDown},
                                                        {"backspace", Qt::Key_Backspace},
                                                        {"bs", Qt::Key_Backspace},
                                                        {"bksp", Qt::Key_Backspace},
                                                        {"break", Qt::Key_Pause},
                                                        {"capslock", Qt::Key_CapsLock},
                                                        {"esc", Qt::Key_Escape},
                                                        {"help", Qt::Key_Help},
                                                        {"numlock", Qt::Key_NumLock},
                                                        {"ptrsc", Qt::Key_Print},
                                                        {"scrolllock", Qt::Key_ScrollLock},
                                                        {"win", Qt::Key_Meta},
                                                        {"lwin", Qt::Key_Meta},
                                                        {"rwin", Qt::Key_Meta},
                                                        {"apps", Qt::Key_Menu},
                                                        {"help", Qt::Key_Help},
                                                        {"add", Qt::Key_Plus},
                                                        {"subtract", Qt::Key_Minus},
                                                        {"multiply", Qt::Key_Asterisk},
                                                        {"divide", Qt::Key_Slash},
                                                        {"leftbrace", Qt::Key_BraceLeft},
                                                        {"{", Qt::Key_BraceLeft},
                                                        {"rightbrace", Qt::Key_BraceRight},
                                                        {"}", Qt::Key_BraceRight},
                                                        {"leftparen", Qt::Key_ParenLeft},
                                                        {"(", Qt::Key_ParenLeft},
                                                        {"rightparen", Qt::Key_ParenRight},
                                                        {")", Qt::Key_ParenRight},
                                                        {"[", Qt::Key_BracketLeft},
                                                        {"]", Qt::Key_BracketRight},
                                                        {"+", Qt::Key_Plus},
                                                        {"%", Qt::Key_Percent},
                                                        {"^", Qt::Key_AsciiCircum},
                                                        {"~", Qt::Key_AsciiTilde},
                                                        {"numpad0", Qt::Key_0},
                                                        {"numpad1", Qt::Key_1},
                                                        {"numpad2", Qt::Key_2},
                                                        {"numpad3", Qt::Key_3},
                                                        {"numpad4", Qt::Key_4},
                                                        {"numpad5", Qt::Key_5},
                                                        {"numpad6", Qt::Key_6},
                                                        {"numpad7", Qt::Key_7},
                                                        {"numpad8", Qt::Key_8},
                                                        {"numpad9", Qt::Key_9},
                                                        {"f1", Qt::Key_F1},
                                                        {"f2", Qt::Key_F2},
                                                        {"f3", Qt::Key_F3},
                                                        {"f4", Qt::Key_F4},
                                                        {"f5", Qt::Key_F5},
                                                        {"f6", Qt::Key_F6},
                                                        {"f7", Qt::Key_F7},
                                                        {"f8", Qt::Key_F8},
                                                        {"f9", Qt::Key_F9},
                                                        {"f10", Qt::Key_F10},
                                                        {"f11", Qt::Key_F11},
                                                        {"f12", Qt::Key_F12},
                                                        {"f13", Qt::Key_F13},
                                                        {"f14", Qt::Key_F14},
                                                        {"f15", Qt::Key_F15},
                                                        {"f16", Qt::Key_F16}};
} // namespace

AutoType* AutoType::m_instance = nullptr;

AutoType::AutoType(QObject* parent, bool test)
    : QObject(parent)
    , m_pluginLoader(new QPluginLoader(this))
    , m_plugin(nullptr)
    , m_executor(nullptr)
    , m_windowState(WindowState::Normal)
    , m_windowForGlobal(0)
{
    // prevent crash when the plugin has unresolved symbols
    m_pluginLoader->setLoadHints(QLibrary::ResolveAllSymbolsHint);

    QString pluginName = "keepassxc-autotype-";
    if (!test) {
        pluginName += QApplication::platformName();
    } else {
        pluginName += "test";
    }

    QString pluginPath = resources()->pluginPath(pluginName);

    if (!pluginPath.isEmpty()) {
#ifdef WITH_XC_AUTOTYPE
        loadPlugin(pluginPath);
#endif
    }

    connect(qApp, SIGNAL(aboutToQuit()), SLOT(unloadPlugin()));
}

AutoType::~AutoType()
{
    if (m_executor) {
        delete m_executor;
        m_executor = nullptr;
    }
}

void AutoType::loadPlugin(const QString& pluginPath)
{
    m_pluginLoader->setFileName(pluginPath);

    QObject* pluginInstance = m_pluginLoader->instance();
    if (pluginInstance) {
        m_plugin = qobject_cast<AutoTypePlatformInterface*>(pluginInstance);
        m_executor = nullptr;

        if (m_plugin) {
            if (m_plugin->isAvailable()) {
                m_executor = m_plugin->createExecutor();
                connect(osUtils,
                        &OSUtilsBase::globalShortcutTriggered,
                        this,
                        [this](const QString& name, const QString& url) {
                            if (name == "autotype") {
                                startGlobalAutoType(url);
                            }
                        });
            } else {
                unloadPlugin();
            }
        }
    }

    if (!m_plugin) {
        qWarning("Unable to load auto-type plugin:\n%s", qPrintable(m_pluginLoader->errorString()));
    }
}

void AutoType::unloadPlugin()
{
    if (m_executor) {
        delete m_executor;
        m_executor = nullptr;
    }

    if (m_plugin) {
        m_plugin->unload();
        m_plugin = nullptr;
    }
}

AutoType* AutoType::instance()
{
    if (!m_instance) {
        m_instance = new AutoType(qApp);
    }

    return m_instance;
}

void AutoType::createTestInstance()
{
    Q_ASSERT(!m_instance);

    m_instance = new AutoType(qApp, true);
}

QStringList AutoType::windowTitles()
{
    if (!m_plugin) {
        return QStringList();
    }

    return m_plugin->windowTitles();
}

void AutoType::raiseWindow()
{
#if defined(Q_OS_MACOS)
    m_plugin->raiseOwnWindow();
#endif
}

bool AutoType::registerGlobalShortcut(Qt::Key key, Qt::KeyboardModifiers modifiers, QString* error)
{
    if (!m_plugin) {
        return false;
    }

    return osUtils->registerGlobalShortcut("autotype", key, modifiers, error);
}

void AutoType::unregisterGlobalShortcut()
{
    osUtils->unregisterGlobalShortcut("autotype");
}

/**
 * Core Autotype function that will execute actions
 */
void AutoType::executeAutoTypeActions(const Entry* entry, QWidget* hideWindow, const QString& sequence, WId window)
{
    QString error;
    auto actions = parseSequence(sequence, entry, error);

    if (!error.isEmpty()) {
        auto errorMsg = tr("The requested Auto-Type sequence cannot be used due to an error:");
        errorMsg.append(QString("\n%1\n%2").arg(sequence, error));
        if (getMainWindow()) {
            MessageBox::critical(getMainWindow(), tr("Auto-Type Error"), errorMsg);
        }
        qWarning() << errorMsg;
        emit autotypeRejected();
        return;
    }

    if (!m_inAutoType.tryLock()) {
        return;
    }

    if (hideWindow) {
#if defined(Q_OS_MACOS)
        // Check for accessibility permission
        if (!macUtils()->enableAccessibility()) {
            MessageBox::information(nullptr,
                                    tr("Permission Required"),
                                    tr("KeePassXC requires the Accessibility permission in order to perform entry "
                                       "level Auto-Type. If you already granted permission, you may have to restart "
                                       "KeePassXC."));
            return;
        }

        macUtils()->raiseLastActiveWindow();
        m_plugin->hideOwnWindow();
#else
        getMainWindow()->minimizeOrHide();
#endif
    }

    // Restore window state in case app stole focus
    restoreWindowState();
    QCoreApplication::processEvents();
    m_plugin->raiseWindow(m_windowForGlobal);

    // Used only for selected entry auto-type
    if (!window) {
        window = m_plugin->activeWindow();
    }

    Tools::wait(qMax(100, config()->get(Config::AutoTypeStartDelay).toInt()));

    for (const auto& action : asConst(actions)) {
        if (m_plugin->activeWindow() != window) {
            qWarning("Active window changed, interrupting auto-type.");
            emit autotypeRejected();
            m_inAutoType.unlock();
            return;
        }

        action->exec(m_executor);
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }

    m_windowForGlobal = 0;
    m_windowTitleForGlobal.clear();

    emit autotypePerformed();
    m_inAutoType.unlock();
}

/**
 * Single Autotype entry-point function
 * Look up the Auto-Type sequence for the given entry then perfom Auto-Type in the active window
 */
void AutoType::performAutoType(const Entry* entry, QWidget* hideWindow)
{
    if (!m_plugin) {
        return;
    }

    auto sequences = entry->autoTypeSequences();
    if (!sequences.isEmpty()) {
        executeAutoTypeActions(entry, hideWindow, sequences.first());
    }
}

/**
 * Extra Autotype entry-point function
 * Perfom Auto-Type of the directly specified sequence in the active window
 */
void AutoType::performAutoTypeWithSequence(const Entry* entry, const QString& sequence, QWidget* hideWindow)
{
    if (!m_plugin) {
        return;
    }

    executeAutoTypeActions(entry, hideWindow, sequence);
}

void AutoType::startGlobalAutoType(const QString& search)
{
    // Never Auto-Type into KeePassXC itself
    if (qApp->focusWindow()) {
        return;
    }

    m_windowForGlobal = m_plugin->activeWindow();
    m_windowTitleForGlobal = m_plugin->activeWindowTitle();
#ifdef Q_OS_MACOS
    // Determine if the user has given proper permissions to KeePassXC to perform Auto-Type
    static bool accessibilityChecked = false;
    if (!accessibilityChecked) {
        if (macUtils()->enableAccessibility() && macUtils()->enableScreenRecording()) {
            accessibilityChecked = true;
        } else if (getMainWindow()) {
            // Does not have required permissions to Auto-Type, ignore the event
            MessageBox::information(
                nullptr,
                tr("Permission Required"),
                tr("KeePassXC requires the Accessibility and Screen Recorder permission in order to perform global "
                   "Auto-Type. Screen Recording is necessary to use the window title to find entries. If you "
                   "already granted permission, you may have to restart KeePassXC."));
            return;
        }
    }

    m_windowState = WindowState::Normal;
    if (getMainWindow()) {
        if (getMainWindow()->isMinimized()) {
            m_windowState = WindowState::Minimized;
        }
        if (getMainWindow()->isHidden()) {
            m_windowState = WindowState::Hidden;
        }
    }
#endif

    emit globalAutoTypeTriggered(search);
}

/**
 * Global Autotype entry-point function
 * Perform global Auto-Type on the active window
 */
void AutoType::performGlobalAutoType(const QList<QSharedPointer<Database>>& dbList, const QString& search)
{
    if (!m_plugin) {
        return;
    }

    if (!m_inGlobalAutoTypeDialog.tryLock()) {
        return;
    }

    if (m_windowTitleForGlobal.isEmpty()) {
        m_inGlobalAutoTypeDialog.unlock();
        return;
    }

    QList<AutoTypeMatch> matchList;
    bool hideExpired = config()->get(Config::AutoTypeHideExpiredEntry).toBool();

    for (const auto& db : dbList) {
        const QList<Entry*> dbEntries = db->rootGroup()->entriesRecursive();
        for (auto entry : dbEntries) {
            auto group = entry->group();
            if (!group || !group->resolveAutoTypeEnabled() || !entry->autoTypeEnabled()) {
                continue;
            }

            if (hideExpired && entry->isExpired()) {
                continue;
            }
            auto sequences = entry->autoTypeSequences(m_windowTitleForGlobal).toSet();
            for (const auto& sequence : sequences) {
                matchList << AutoTypeMatch(entry, sequence);
            }
        }
    }

    // Show the selection dialog if we always ask, have multiple matches, or no matches
    if (getMainWindow()
        && (config()->get(Config::Security_AutoTypeAsk).toBool() || matchList.size() > 1 || matchList.isEmpty())) {
        // Close any open modal windows that would interfere with the process
        getMainWindow()->closeModalWindow();

        auto* selectDialog = new AutoTypeSelectDialog();
        selectDialog->setMatches(matchList, dbList);

        if (!search.isEmpty()) {
            selectDialog->setSearchString(search);
        }

        connect(getMainWindow(), &MainWindow::databaseLocked, selectDialog, &AutoTypeSelectDialog::reject);
        connect(selectDialog, &AutoTypeSelectDialog::matchActivated, this, [this](const AutoTypeMatch& match) {
            executeAutoTypeActions(match.first, nullptr, match.second, m_windowForGlobal);
            resetAutoTypeState();
        });
        connect(selectDialog, &QDialog::rejected, this, [this] {
            restoreWindowState();
            resetAutoTypeState();
            emit autotypeRejected();
        });

#ifdef Q_OS_MACOS
        m_plugin->raiseOwnWindow();
        Tools::wait(50);
#endif
        selectDialog->show();
        selectDialog->raise();
        selectDialog->activateWindow();
    } else if (!matchList.isEmpty()) {
        // Only one match and not asking, do it!
        executeAutoTypeActions(matchList.first().first, nullptr, matchList.first().second, m_windowForGlobal);
        resetAutoTypeState();
    } else {
        // We should never get here
        resetAutoTypeState();
        emit autotypeRejected();
    }
}

void AutoType::restoreWindowState()
{
#ifdef Q_OS_MAC
    if (getMainWindow()) {
        if (m_windowState == WindowState::Minimized) {
            getMainWindow()->showMinimized();
        } else if (m_windowState == WindowState::Hidden) {
            getMainWindow()->hideWindow();
        }
    }
#endif
}

void AutoType::resetAutoTypeState()
{
    m_windowForGlobal = 0;
    m_windowTitleForGlobal.clear();
    Q_UNUSED(m_inGlobalAutoTypeDialog.tryLock());
    m_inGlobalAutoTypeDialog.unlock();
}

/**
 * Parse an autotype sequence into a list of AutoTypeActions.
 * If error is provided then syntax checking will be performed.
 */
QList<QSharedPointer<AutoTypeAction>>
AutoType::parseSequence(const QString& entrySequence, const Entry* entry, QString& error, bool syntaxOnly)
{
    const int maxTypeDelay = 100;
    const int maxWaitDelay = 10000;
    const int maxRepetition = 100;

    QList<QSharedPointer<AutoTypeAction>> actions;
    actions << QSharedPointer<AutoTypeDelay>::create(qMax(0, config()->get(Config::AutoTypeDelay).toInt()), true);

    // Replace escaped braces with a template for easier regex
    QString sequence = entrySequence;
    sequence.replace("{{}", "{LEFTBRACE}");
    sequence.replace("{}}", "{RIGHTBRACE}");

    // Quick test for bracket syntax
    if (sequence.count("{") != sequence.count("}")) {
        error = tr("Bracket imbalance detected, found extra { or }");
        return {};
    }

    // Group 1 = modifier key (opt)
    // Group 2 = full placeholder
    // Group 3 = inner placeholder (allows nested placeholders)
    // Group 4 = repeat (opt)
    // Group 5 = character
    QRegularExpression regex("([+%^]*)({((?>[^{}]+?|(?2))+?)(?:\\s+(\\d+))?})|(.)");
    auto results = regex.globalMatch(sequence);
    while (results.hasNext()) {
        auto match = results.next();

        // Parse modifier keys
        Qt::KeyboardModifiers modifiers;
        if (match.captured(1).contains('+')) {
            modifiers |= Qt::ShiftModifier;
        }
        if (match.captured(1).contains('^')) {
            modifiers |= Qt::ControlModifier;
        }
        if (match.captured(1).contains('%')) {
            modifiers |= Qt::AltModifier;
        }

        const auto fullPlaceholder = match.captured(2);
        auto placeholder = match.captured(3).toLower();
        auto repeat = 1;
        if (!match.captured(4).isEmpty()) {
            repeat = match.captured(4).toInt();
        }
        auto character = match.captured(5);

        if (placeholder.isEmpty()) {
            if (!character.isEmpty()) {
                // Type a single character with modifier
                actions << QSharedPointer<AutoTypeKey>::create(character[0], modifiers);
            }
            continue;
        }

        if (g_placeholderToKey.contains(placeholder)) {
            // Basic key placeholder, allow repeat
            if (repeat > maxRepetition) {
                error = tr("Too many repetitions detected, max is %1: %2").arg(maxRepetition).arg(fullPlaceholder);
                return {};
            }
            auto action = QSharedPointer<AutoTypeKey>::create(g_placeholderToKey[placeholder], modifiers);
            for (int i = 1; i <= repeat && i <= maxRepetition; ++i) {
                actions << action;
            }
        } else if (placeholder.startsWith("delay=")) {
            // Change keypress delay
            int delay = placeholder.replace("delay=", "").toInt();
            if (delay > maxTypeDelay) {
                error = tr("Very slow key press detected, max is %1: %2").arg(maxTypeDelay).arg(fullPlaceholder);
                return {};
            }
            actions << QSharedPointer<AutoTypeDelay>::create(qBound(0, delay, maxTypeDelay), true);
        } else if (placeholder == "delay") {
            // Mid typing delay (wait)
            if (repeat > maxWaitDelay) {
                error = tr("Very long delay detected, max is %1: %2").arg(maxWaitDelay).arg(fullPlaceholder);
                return {};
            }
            actions << QSharedPointer<AutoTypeDelay>::create(qBound(0, repeat, maxWaitDelay));
        } else if (placeholder == "clearfield") {
            // Platform-specific field clearing
            actions << QSharedPointer<AutoTypeClearField>::create();
        } else if (placeholder == "totp") {
            // Entry totp (requires special handling)
            QString totp = entry->totp();
            for (const auto& ch : totp) {
                actions << QSharedPointer<AutoTypeKey>::create(ch);
            }
        } else if (placeholder == "pickchars") {
            // Ignore this if we are syntax checking
            if (syntaxOnly) {
                continue;
            }
            // Show pickchars dialog for entry's password
            auto password = entry->resolvePlaceholder(entry->password());
            if (!password.isEmpty()) {
                PickcharsDialog pickcharsDialog(password);
                if (pickcharsDialog.exec() == QDialog::Accepted && !pickcharsDialog.selectedChars().isEmpty()) {
                    for (const auto& ch : pickcharsDialog.selectedChars()) {
                        actions << QSharedPointer<AutoTypeKey>::create(ch);
                        if (pickcharsDialog.pressTab()) {
                            actions << QSharedPointer<AutoTypeKey>::create(g_placeholderToKey["tab"]);
                        }
                    }
                    if (pickcharsDialog.pressTab()) {
                        // Remove extra tab
                        actions.removeLast();
                    }
                }
            }
        } else if (placeholder.startsWith("t-conv:")) {
            // Reset to the original capture to preserve case
            placeholder = match.captured(3);
            placeholder.replace("t-conv:", "", Qt::CaseInsensitive);
            if (!placeholder.isEmpty()) {
                auto sep = placeholder[0];
                auto parts = placeholder.split(sep);
                if (parts.size() >= 4) {
                    auto resolved = entry->resolveMultiplePlaceholders(parts[1]);
                    auto type = parts[2].toLower();

                    if (type == "base64") {
                        resolved = resolved.toUtf8().toBase64();
                    } else if (type == "hex") {
                        resolved = resolved.toUtf8().toHex();
                    } else if (type == "uri") {
                        resolved = QUrl::toPercentEncoding(resolved.toUtf8());
                    } else if (type == "uri-dec") {
                        resolved = QUrl::fromPercentEncoding(resolved.toUtf8());
                    } else if (type.startsWith("u")) {
                        resolved = resolved.toUpper();
                    } else if (type.startsWith("l")) {
                        resolved = resolved.toLower();
                    } else {
                        error = tr("Invalid conversion type: %1").arg(type);
                        return {};
                    }
                    for (const QChar& ch : resolved) {
                        actions << QSharedPointer<AutoTypeKey>::create(ch);
                    }
                } else {
                    error = tr("Invalid conversion syntax: %1").arg(fullPlaceholder);
                    return {};
                }
            } else {
                error = tr("Invalid conversion syntax: %1").arg(fullPlaceholder);
                return {};
            }
        } else if (placeholder.startsWith("t-replace-rx:")) {
            // Reset to the original capture to preserve case
            placeholder = match.captured(3);
            placeholder.replace("t-replace-rx:", "", Qt::CaseInsensitive);
            if (!placeholder.isEmpty()) {
                auto sep = placeholder[0];
                auto parts = placeholder.split(sep);
                if (parts.size() >= 5) {
                    auto resolvedText = entry->resolveMultiplePlaceholders(parts[1]);
                    auto resolvedSearch = entry->resolveMultiplePlaceholders(parts[2]);
                    auto resolvedReplace = entry->resolveMultiplePlaceholders(parts[3]);
                    // Replace $<num> with \\<num> to support Qt substitutions
                    resolvedReplace.replace(QRegularExpression(R"(\$(\d+))"), R"(\\1)");

                    auto searchRegex = QRegularExpression(resolvedSearch);
                    if (!searchRegex.isValid()) {
                        error = tr("Invalid regular expression syntax %1\n%2")
                                    .arg(resolvedSearch, searchRegex.errorString());
                        return {};
                    }

                    auto resolved = resolvedText.replace(searchRegex, resolvedReplace);
                    for (const QChar& ch : resolved) {
                        actions << QSharedPointer<AutoTypeKey>::create(ch);
                    }
                } else {
                    error = tr("Invalid conversion syntax: %1").arg(fullPlaceholder);
                    return {};
                }
            } else {
                error = tr("Invalid conversion syntax: %1").arg(fullPlaceholder);
                return {};
            }
        } else if (placeholder == "beep" || placeholder.startsWith("vkey") || placeholder.startsWith("appactivate")
                   || placeholder.startsWith("c:")) {
            // Ignore these commands
        } else {
            // Attempt to resolve an entry attribute
            auto resolved = entry->resolvePlaceholder(fullPlaceholder);
            if (resolved != fullPlaceholder) {
                // Attribute resolved, add characters to action list
                for (const QChar& ch : resolved) {
                    if (ch == '\n') {
                        actions << QSharedPointer<AutoTypeKey>::create(g_placeholderToKey["enter"]);
                    } else if (ch == '\t') {
                        actions << QSharedPointer<AutoTypeKey>::create(g_placeholderToKey["tab"]);
                    } else {
                        actions << QSharedPointer<AutoTypeKey>::create(ch);
                    }
                }
            } else {
                // Invalid placeholder, issue error and stop processing
                error = tr("Invalid placeholder: %1").arg(fullPlaceholder);
                return {};
            }
        }
    }

    return actions;
}

/**
 * Verify if the syntax of an autotype sequence is correct and doesn't have invalid parameters
 */
bool AutoType::verifyAutoTypeSyntax(const QString& sequence, const Entry* entry, QString& error)
{
    error.clear();
    if (!sequence.isEmpty()) {
        parseSequence(sequence, entry, error, true);
    }
    return error.isEmpty();
}
