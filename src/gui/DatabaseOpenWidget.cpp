/*
 *  Copyright (C) 2011 Felix Geyer <debfx@fobos.de>
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

#include "DatabaseOpenWidget.h"
#include "ui_DatabaseOpenWidget.h"

#include "core/Config.h"
#include "core/Database.h"
#include "crypto/Random.h"
#include "format/KeePass2Reader.h"
#include "gui/FileDialog.h"
#include "gui/Icons.h"
#include "gui/MainWindow.h"
#include "gui/MessageBox.h"
#include "keys/FileKey.h"
#include "keys/PasswordKey.h"
#include "keys/YkChallengeResponseKey.h"
#include "touchid/TouchID.h"
#ifdef WITH_XC_WINDOWSHELLO
#include "winhello/WindowsHello.h"
#endif

#include "config-keepassx.h"

#include <QDesktopServices>
#include <QFont>
#include <QSharedPointer>

namespace
{
    constexpr int clearFormsDelay = 30000;
}

DatabaseOpenWidget::DatabaseOpenWidget(QWidget* parent)
    : DialogyWidget(parent)
    , m_ui(new Ui::DatabaseOpenWidget())
    , m_db(nullptr)
{
    m_ui->setupUi(this);

    m_ui->messageWidget->setHidden(true);

    m_hideTimer.setInterval(clearFormsDelay);
    m_hideTimer.setSingleShot(true);
    connect(&m_hideTimer, &QTimer::timeout, this, [this] {
        // Reset the password field after being hidden for a set time
        m_ui->editPassword->setText("");
        m_ui->editPassword->setShowPassword(false);
    });

    QFont font;
    font.setPointSize(font.pointSize() + 4);
    font.setBold(true);
    m_ui->labelHeadline->setFont(font);
    m_ui->labelHeadline->setText(tr("Unlock KeePassXC Database"));

    connect(m_ui->buttonBrowseFile, SIGNAL(clicked()), SLOT(browseKeyFile()));

    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Unlock"));
    connect(m_ui->buttonBox, &QDialogButtonBox::accepted, [this](){ openDatabase(); });
    connect(m_ui->buttonBox, SIGNAL(rejected()), SLOT(reject()));

    m_ui->hardwareKeyLabelHelp->setIcon(icons()->icon("system-help").pixmap(QSize(12, 12)));
    connect(m_ui->hardwareKeyLabelHelp, SIGNAL(clicked(bool)), SLOT(openHardwareKeyHelp()));
    m_ui->keyFileLabelHelp->setIcon(icons()->icon("system-help").pixmap(QSize(12, 12)));
    connect(m_ui->keyFileLabelHelp, SIGNAL(clicked(bool)), SLOT(openKeyFileHelp()));

    m_ui->buttonWinHelloUnlock->setVisible(false);
    m_ui->buttonWinHelloUnlock->setIcon(icons()->icon("winhello"));
    connect(m_ui->buttonWinHelloUnlock, &QPushButton::clicked, [this](){ openDatabase(/*useOsStoredKey=*/true); });

#ifdef WITH_XC_YUBIKEY
    m_ui->hardwareKeyProgress->setVisible(false);
    QSizePolicy sp = m_ui->hardwareKeyProgress->sizePolicy();
    sp.setRetainSizeWhenHidden(true);
    m_ui->hardwareKeyProgress->setSizePolicy(sp);

    connect(m_ui->buttonRedetectYubikey, SIGNAL(clicked()), SLOT(pollHardwareKey()));
    connect(YubiKey::instance(), SIGNAL(detectComplete(bool)), SLOT(hardwareKeyResponse(bool)), Qt::QueuedConnection);

    connect(YubiKey::instance(), &YubiKey::userInteractionRequest, this, [this] {
        // Show the press notification if we are in an independent window (e.g., DatabaseOpenDialog)
        if (window() != getMainWindow()) {
            m_ui->messageWidget->showMessage(tr("Please touch the button on your YubiKey!"),
                                             MessageWidget::Information,
                                             MessageWidget::DisableAutoHide);
        }
    });
    connect(YubiKey::instance(), &YubiKey::challengeCompleted, this, [this] { m_ui->messageWidget->hide(); });
#else
    m_ui->hardwareKeyLabel->setVisible(false);
    m_ui->hardwareKeyLabelHelp->setVisible(false);
    m_ui->buttonRedetectYubikey->setVisible(false);
    m_ui->challengeResponseCombo->setVisible(false);
    m_ui->hardwareKeyProgress->setVisible(false);
#endif

#ifndef WITH_XC_TOUCHID
    m_ui->touchIDContainer->setVisible(false);
#else
    if (!TouchID::getInstance().isAvailable()) {
        m_ui->checkTouchID->setVisible(false);
    }
#endif
}

DatabaseOpenWidget::~DatabaseOpenWidget()
{
}

void DatabaseOpenWidget::showEvent(QShowEvent* event)
{
    DialogyWidget::showEvent(event);
    m_ui->editPassword->setFocus();
    m_hideTimer.stop();
#ifdef WITH_XC_WINDOWSHELLO
    m_ui->buttonWinHelloUnlock->setVisible(WindowsHello::containsKey(m_filename));
#endif
}

void DatabaseOpenWidget::hideEvent(QHideEvent* event)
{
    DialogyWidget::hideEvent(event);

    // Schedule form clearing if we are hidden
    if (!isVisible()) {
        m_hideTimer.start();
    }
}

void DatabaseOpenWidget::load(const QString& filename)
{
    clearForms();

    m_filename = filename;
    m_ui->fileNameLabel->setRawText(m_filename);

    if (config()->get(Config::RememberLastKeyFiles).toBool()) {
        auto lastKeyFiles = config()->get(Config::LastKeyFiles).toHash();
        if (lastKeyFiles.contains(m_filename)) {
            m_ui->keyFileLineEdit->setText(lastKeyFiles[m_filename].toString());
        }
    }

    QHash<QString, QVariant> useTouchID = config()->get(Config::UseTouchID).toHash();
    m_ui->checkTouchID->setChecked(useTouchID.value(m_filename, false).toBool());

#ifdef WITH_XC_WINDOWSHELLO
    if (WindowsHello::isAvailable()) {
        m_ui->buttonWinHelloUnlock->setVisible(WindowsHello::containsKey(m_filename));
    }
#endif

#ifdef WITH_XC_YUBIKEY
    // Only auto-poll for hardware keys if we previously used one with this database file
    if (config()->get(Config::RememberLastKeyFiles).toBool()) {
        auto lastChallengeResponse = config()->get(Config::LastChallengeResponse).toHash();
        if (lastChallengeResponse.contains(m_filename)) {
            pollHardwareKey();
        }
    }
#endif
}

void DatabaseOpenWidget::clearForms()
{
    m_ui->editPassword->setText("");
    m_ui->editPassword->setShowPassword(false);
    m_ui->keyFileLineEdit->clear();
    m_ui->keyFileLineEdit->setShowPassword(false);
    m_ui->checkTouchID->setChecked(false);
    m_ui->challengeResponseCombo->clear();
    m_db.reset();
}

QSharedPointer<Database> DatabaseOpenWidget::database()
{
    return m_db;
}

QString DatabaseOpenWidget::filename()
{
    return m_filename;
}

void DatabaseOpenWidget::enterKey(const QString& pw, const QString& keyFile)
{
    m_ui->editPassword->setText(pw);
    m_ui->keyFileLineEdit->setText(keyFile);
    openDatabase();
}

void DatabaseOpenWidget::openDatabase(bool useOsStoredKey)
{
    m_ui->messageWidget->hide();

    QSharedPointer<CompositeKey> databaseKey = buildDatabaseKey(useOsStoredKey);
    if (!databaseKey) {
        return;
    }

    m_ui->editPassword->setShowPassword(false);
    QCoreApplication::processEvents();

    m_db.reset(new Database());
    QString error;

    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    m_ui->passwordFormFrame->setEnabled(false);
    QCoreApplication::processEvents();
    bool ok = m_db->open(m_filename, databaseKey, &error, false);
    QApplication::restoreOverrideCursor();
    m_ui->passwordFormFrame->setEnabled(true);

    if (ok) {
#ifdef WITH_XC_WINDOWSHELLO
        if(WindowsHello::isAvailable()
            && WindowsHello::containsKey(m_filename)
            && !m_ui->editPassword->text().isEmpty()) {
            // In case db password was changed on some other device, update password stored in windows hello
            WindowsHello(this).storeKey(m_filename, PasswordKey(m_ui->editPassword->text()).rawKey());
        }
#elif defined(WITH_XC_TOUCHID)
        QHash<QString, QVariant> useTouchID = config()->get(Config::UseTouchID).toHash();

        // check if TouchID can & should be used to unlock the database next time
        if (m_ui->checkTouchID->isChecked() && TouchID::getInstance().isAvailable()) {
            // encrypt and store key blob
            if (TouchID::getInstance().storeKey(m_filename, PasswordKey(m_ui->editPassword->text()).rawKey())) {
                useTouchID.insert(m_filename, true);
            }
        } else {
            // when TouchID not available or unchecked, reset for the current database
            TouchID::getInstance().reset(m_filename);
            useTouchID.insert(m_filename, false);
        }

        config()->set(Config::UseTouchID, useTouchID);
#endif
        emit dialogFinished(true);
        clearForms();
    } else {
        if (m_ui->editPassword->text().isEmpty() && !m_retryUnlockWithEmptyPassword) {
            QScopedPointer<QMessageBox> msgBox(new QMessageBox(this));
            msgBox->setIcon(QMessageBox::Critical);
            msgBox->setWindowTitle(tr("Unlock failed and no password given"));
            msgBox->setText(tr("Unlocking the database failed and you did not enter a password.\n"
                               "Do you want to retry with an \"empty\" password instead?\n\n"
                               "To prevent this error from appearing, you must go to "
                               "\"Database Settings / Security\" and reset your password."));
            auto btn = msgBox->addButton(tr("Retry with empty password"), QMessageBox::ButtonRole::AcceptRole);
            msgBox->setDefaultButton(btn);
            msgBox->addButton(QMessageBox::Cancel);
            msgBox->exec();

            if (msgBox->clickedButton() == btn) {
                m_retryUnlockWithEmptyPassword = true;
                openDatabase();
                return;
            }
        }

        m_retryUnlockWithEmptyPassword = false;
        m_ui->messageWidget->showMessage(error, MessageWidget::MessageType::Error);
        // Focus on the password field and select the input for easy retry
        m_ui->editPassword->selectAll();
        m_ui->editPassword->setFocus();

#ifdef WITH_XC_WINDOWSHELLO
        if(useOsStoredKey
            && WindowsHello::isAvailable()
            && m_ui->editPassword->text().isEmpty()) {
            // FIX: This is bug, if the wrong key file was used

            // User probably tried to open DB with Windows Hello.
            // We assume stored DB key is not valid anymore, so let's remove it from store.
            WindowsHello(this).removeKey(m_filename);
            m_ui->buttonWinHelloUnlock->setVisible(false);
        }
#elif defined(WITH_XC_TOUCHID)
        // unable to unlock database, reset TouchID for the current database
        TouchID::getInstance().reset(m_filename);
#endif
    }
}

QSharedPointer<CompositeKey> DatabaseOpenWidget::buildDatabaseKey(bool useOsStoredKey)
{
    auto databaseKey = QSharedPointer<CompositeKey>::create();

    if (!m_ui->editPassword->text().isEmpty() || m_retryUnlockWithEmptyPassword) {
        databaseKey->addKey(QSharedPointer<PasswordKey>::create(m_ui->editPassword->text()));
    }

#ifdef WITH_XC_WINDOWSHELLO
    if (useOsStoredKey
        && WindowsHello::isAvailable()
        && WindowsHello::containsKey(m_filename)
        && m_ui->editPassword->text().isEmpty()) {
        // clear empty password from composite key
        databaseKey->clear();

        // try to get, decrypt and use PasswordKey
        QSharedPointer<QByteArray> passwordKey = WindowsHello(this).getKey(m_filename);
        if (passwordKey.isNull()) { // If null, either user cancelled or an error has occurred
            m_ui->buttonWinHelloUnlock->setVisible(WindowsHello::containsKey(m_filename));
            return QSharedPointer<CompositeKey>();
        }
        databaseKey->addKey(PasswordKey::fromRawKey(*passwordKey));
    }
#elif defined(WITH_XC_TOUCHID)
    // check if TouchID is available and enabled for unlocking the database
    if (m_ui->checkTouchID->isChecked() && TouchID::getInstance().isAvailable()
        && m_ui->editPassword->text().isEmpty()) {
        // clear empty password from composite key
        databaseKey->clear();

        // try to get, decrypt and use PasswordKey
        QSharedPointer<QByteArray> passwordKey = TouchID::getInstance().getKey(m_filename);
        if (passwordKey != NULL) {
            // check if the user cancelled the operation
            if (passwordKey.isNull())
                return QSharedPointer<CompositeKey>();

            databaseKey->addKey(PasswordKey::fromRawKey(*passwordKey));
        }
    }
#endif

    auto lastKeyFiles = config()->get(Config::LastKeyFiles).toHash();
    lastKeyFiles.remove(m_filename);

    auto key = QSharedPointer<FileKey>::create();
    QString keyFilename = m_ui->keyFileLineEdit->text();
    if (!keyFilename.isEmpty()) {
        QString errorMsg;
        if (!key->load(keyFilename, &errorMsg)) {
            m_ui->messageWidget->showMessage(tr("Failed to open key file: %1").arg(errorMsg), MessageWidget::Error);
            return {};
        }
        if (key->type() != FileKey::KeePass2XMLv2 && key->type() != FileKey::Hashed
            && !config()->get(Config::Messages_NoLegacyKeyFileWarning).toBool()) {
            QMessageBox legacyWarning;
            legacyWarning.setWindowTitle(tr("Old key file format"));
            legacyWarning.setText(tr("You are using an old key file format which KeePassXC may<br>"
                                     "stop supporting in the future.<br><br>"
                                     "Please consider generating a new key file by going to:<br>"
                                     "<strong>Database / Database Security / Change Key File.</strong><br>"));
            legacyWarning.setIcon(QMessageBox::Icon::Warning);
            legacyWarning.addButton(QMessageBox::Ok);
            legacyWarning.setDefaultButton(QMessageBox::Ok);
            legacyWarning.setCheckBox(new QCheckBox(tr("Don't show this warning again")));

            connect(legacyWarning.checkBox(), &QCheckBox::stateChanged, this, [](int state) {
                config()->set(Config::Messages_NoLegacyKeyFileWarning, state == Qt::CheckState::Checked);
            });

            legacyWarning.exec();
        }
        databaseKey->addKey(key);
        lastKeyFiles.insert(m_filename, keyFilename);
    }

    if (config()->get(Config::RememberLastKeyFiles).toBool()) {
        config()->set(Config::LastKeyFiles, lastKeyFiles);
    }

#ifdef WITH_XC_YUBIKEY
    auto lastChallengeResponse = config()->get(Config::LastChallengeResponse).toHash();
    lastChallengeResponse.remove(m_filename);

    int selectionIndex = m_ui->challengeResponseCombo->currentIndex();
    if (selectionIndex > 0) {
        auto slot = m_ui->challengeResponseCombo->itemData(selectionIndex).value<YubiKeySlot>();
        auto crKey = QSharedPointer<YkChallengeResponseKey>(new YkChallengeResponseKey(slot));
        databaseKey->addChallengeResponseKey(crKey);

        // Qt doesn't read custom types in settings so stuff into a QString
        lastChallengeResponse.insert(m_filename, QStringLiteral("%1:%2").arg(slot.first).arg(slot.second));
    }

    if (config()->get(Config::RememberLastKeyFiles).toBool()) {
        config()->set(Config::LastChallengeResponse, lastChallengeResponse);
    }
#endif

    return databaseKey;
}

void DatabaseOpenWidget::reject()
{
    emit dialogFinished(false);
}

void DatabaseOpenWidget::browseKeyFile()
{
    QString filters = QString("%1 (*);;%2 (*.keyx; *.key)").arg(tr("All files"), tr("Key files"));
    if (!config()->get(Config::RememberLastKeyFiles).toBool()) {
        fileDialog()->setNextForgetDialog();
    }
    QString filename = fileDialog()->getOpenFileName(this, tr("Select key file"), QString(), filters);

    if (QFileInfo(filename).canonicalFilePath() == QFileInfo(m_filename).canonicalFilePath()) {
        MessageBox::warning(this,
                            tr("Cannot use database file as key file"),
                            tr("You cannot use your database file as a key file.\nIf you do not have a key file, "
                               "please leave the field empty."),
                            MessageBox::Button::Ok);
        filename = "";
    }

    if (!filename.isEmpty()) {
        m_ui->keyFileLineEdit->setText(filename);
    }
}

void DatabaseOpenWidget::clearKeyFileText()
{
    m_ui->keyFileLineEdit->clear();
    m_ui->keyFileLineEdit->setShowPassword(false);
}

void DatabaseOpenWidget::pollHardwareKey()
{
    if (m_pollingHardwareKey) {
        return;
    }

    m_ui->challengeResponseCombo->clear();
    m_ui->challengeResponseCombo->addItem(tr("Detecting hardware keys…"));

    m_ui->buttonRedetectYubikey->setEnabled(false);
    m_ui->challengeResponseCombo->setEnabled(false);
    m_ui->hardwareKeyProgress->setVisible(true);
    m_pollingHardwareKey = true;

    YubiKey::instance()->findValidKeys();
}

void DatabaseOpenWidget::hardwareKeyResponse(bool found)
{
    m_ui->challengeResponseCombo->clear();
    m_ui->buttonRedetectYubikey->setEnabled(true);
    m_ui->hardwareKeyProgress->setVisible(false);
    m_pollingHardwareKey = false;

    if (!found) {
        m_ui->challengeResponseCombo->addItem(tr("No hardware keys detected"));
        m_ui->challengeResponseCombo->setEnabled(false);
        return;
    } else {
        m_ui->challengeResponseCombo->addItem(tr("Select hardware key…"));
    }

    YubiKeySlot lastUsedSlot;
    if (config()->get(Config::RememberLastKeyFiles).toBool()) {
        auto lastChallengeResponse = config()->get(Config::LastChallengeResponse).toHash();
        if (lastChallengeResponse.contains(m_filename)) {
            // Qt doesn't read custom types in settings so extract from QString
            auto split = lastChallengeResponse.value(m_filename).toString().split(":");
            if (split.size() > 1) {
                lastUsedSlot = YubiKeySlot(split[0].toUInt(), split[1].toInt());
            }
        }
    }

    int selectedIndex = 0;
    for (auto& slot : YubiKey::instance()->foundKeys()) {
        // add detected YubiKey to combo box
        m_ui->challengeResponseCombo->addItem(YubiKey::instance()->getDisplayName(slot), QVariant::fromValue(slot));
        // Select this YubiKey + Slot if we used it in the past
        if (lastUsedSlot == slot) {
            selectedIndex = m_ui->challengeResponseCombo->count() - 1;
        }
    }

    m_ui->challengeResponseCombo->setCurrentIndex(selectedIndex);
    m_ui->challengeResponseCombo->setEnabled(true);
}

void DatabaseOpenWidget::openHardwareKeyHelp()
{
    QDesktopServices::openUrl(QUrl("https://keepassxc.org/docs#faq-cat-yubikey"));
}

void DatabaseOpenWidget::openKeyFileHelp()
{
    QDesktopServices::openUrl(QUrl("https://keepassxc.org/docs#faq-cat-keyfile"));
}
