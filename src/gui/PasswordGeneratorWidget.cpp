/*
 *  Copyright (C) 2013 Felix Geyer <debfx@fobos.de>
 *  Copyright (C) 2020 KeePassXC Team <team@keepassxc.org>
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

#include "PasswordGeneratorWidget.h"
#include "ui_PasswordGeneratorWidget.h"

#include <QDir>
#include <QShortcut>
#include <QTimer>

#include "core/Config.h"
#include "core/PasswordHealth.h"
#include "core/Resources.h"
#include "gui/Clipboard.h"
#include "gui/Icons.h"
#include "gui/styles/StateColorPalette.h"

PasswordGeneratorWidget::PasswordGeneratorWidget(QWidget* parent)
    : QWidget(parent)
    , m_passwordGenerator(new PasswordGenerator())
    , m_dicewareGenerator(new PassphraseGenerator())
    , m_ui(new Ui::PasswordGeneratorWidget())
{
    m_ui->setupUi(this);

    m_ui->buttonGenerate->setIcon(icons()->icon("refresh"));
    m_ui->buttonGenerate->setToolTip(
        tr("Regenerate password (%1)").arg(m_ui->buttonGenerate->shortcut().toString(QKeySequence::NativeText)));
    m_ui->buttonCopy->setIcon(icons()->icon("clipboard-text"));
    m_ui->buttonAutoType->setIcon(icons()->icon("clipboard-text"));
    m_ui->buttonClose->setShortcut(Qt::Key_Escape);

    // Add two shortcuts to save the form CTRL+Enter and CTRL+S
    auto shortcut = new QShortcut(Qt::CTRL + Qt::Key_Return, this);
    connect(shortcut, &QShortcut::activated, this, [this] { applyPassword(); });
    shortcut = new QShortcut(Qt::CTRL + Qt::Key_S, this);
    connect(shortcut, &QShortcut::activated, this, [this] { applyPassword(); });

    connect(m_ui->editNewPassword, SIGNAL(textChanged(QString)), SLOT(updateButtonsEnabled(QString)));
    connect(m_ui->editNewPassword, SIGNAL(textChanged(QString)), SLOT(updatePasswordStrength(QString)));
    connect(m_ui->buttonAdvancedMode, SIGNAL(toggled(bool)), SLOT(setAdvancedMode(bool)));
    connect(m_ui->buttonAddHex, SIGNAL(clicked()), SLOT(excludeHexChars()));
    connect(m_ui->editAdditionalChars, SIGNAL(textChanged(QString)), SLOT(updateGenerator()));
    connect(m_ui->editExcludedChars, SIGNAL(textChanged(QString)), SLOT(updateGenerator()));
    connect(m_ui->buttonApply, SIGNAL(clicked()), SLOT(applyPassword()));
    connect(m_ui->buttonCopy, SIGNAL(clicked()), SLOT(copyPassword()));
    connect(m_ui->buttonAutoType, SIGNAL(clicked()), SLOT(autoTypePassword()));
    connect(m_ui->buttonGenerate, SIGNAL(clicked()), SLOT(regeneratePassword()));
    connect(m_ui->buttonClose, SIGNAL(clicked()), SIGNAL(closed()));

    connect(m_ui->sliderLength, SIGNAL(valueChanged(int)), SLOT(passwordLengthChanged(int)));
    connect(m_ui->spinBoxLength, SIGNAL(valueChanged(int)), SLOT(passwordLengthChanged(int)));

    connect(m_ui->sliderWordCount, SIGNAL(valueChanged(int)), SLOT(passphraseLengthChanged(int)));
    connect(m_ui->spinBoxWordCount, SIGNAL(valueChanged(int)), SLOT(passphraseLengthChanged(int)));

    connect(m_ui->editWordSeparator, SIGNAL(textChanged(QString)), SLOT(updateGenerator()));
    connect(m_ui->comboBoxWordList, SIGNAL(currentIndexChanged(int)), SLOT(updateGenerator()));
    connect(m_ui->optionButtons, SIGNAL(buttonClicked(int)), SLOT(updateGenerator()));
    connect(m_ui->tabWidget, SIGNAL(currentChanged(int)), SLOT(updateGenerator()));
    connect(m_ui->wordCaseComboBox, SIGNAL(currentIndexChanged(int)), SLOT(updateGenerator()));

    // set font size of password quality and entropy labels dynamically to 80% of
    // the default font size, but make it no smaller than 8pt
    QFont defaultFont;
    int smallerSize = static_cast<int>(defaultFont.pointSize() * 0.8f);
    if (smallerSize >= 8) {
        defaultFont.setPointSize(smallerSize);
        m_ui->entropyLabel->setFont(defaultFont);
        m_ui->strengthLabel->setFont(defaultFont);
    }

    // set default separator to Space
    m_ui->editWordSeparator->setText(PassphraseGenerator::DefaultSeparator);

    // add passphrase generator case options
    m_ui->wordCaseComboBox->addItem(tr("lower case"), PassphraseGenerator::LOWERCASE);
    m_ui->wordCaseComboBox->addItem(tr("UPPER CASE"), PassphraseGenerator::UPPERCASE);
    m_ui->wordCaseComboBox->addItem(tr("Title Case"), PassphraseGenerator::TITLECASE);

    QDir path(resources()->wordlistPath(""));
    QStringList files = path.entryList(QDir::Files);
    m_ui->comboBoxWordList->addItems(files);
    if (files.size() > 1) {
        m_ui->comboBoxWordList->setVisible(true);
        m_ui->labelWordList->setVisible(true);
    } else {
        m_ui->comboBoxWordList->setVisible(false);
        m_ui->labelWordList->setVisible(false);
    }

    loadSettings();
}

PasswordGeneratorWidget::~PasswordGeneratorWidget()
{
}

PasswordGeneratorWidget* PasswordGeneratorWidget::popupGenerator(QWidget* parent)
{
    auto pwGenerator = new PasswordGeneratorWidget(parent);
    pwGenerator->setWindowModality(Qt::ApplicationModal);
    pwGenerator->setWindowFlags(Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint);
    pwGenerator->setStandaloneMode(false);

    connect(pwGenerator, SIGNAL(closed()), pwGenerator, SLOT(deleteLater()));

    pwGenerator->show();
    pwGenerator->raise();
    pwGenerator->activateWindow();
    pwGenerator->adjustSize();

    return pwGenerator;
}

void PasswordGeneratorWidget::loadSettings()
{
    // Password config
    m_ui->checkBoxLower->setChecked(config()->get(Config::PasswordGenerator_LowerCase).toBool());
    m_ui->checkBoxUpper->setChecked(config()->get(Config::PasswordGenerator_UpperCase).toBool());
    m_ui->checkBoxNumbers->setChecked(config()->get(Config::PasswordGenerator_Numbers).toBool());
    m_ui->editAdditionalChars->setText(config()->get(Config::PasswordGenerator_AdditionalChars).toString());
    m_ui->editExcludedChars->setText(config()->get(Config::PasswordGenerator_ExcludedChars).toString());

    bool advanced = config()->get(Config::PasswordGenerator_AdvancedMode).toBool();
    if (advanced) {
        m_ui->checkBoxSpecialChars->setChecked(config()->get(Config::PasswordGenerator_Logograms).toBool());
    } else {
        m_ui->checkBoxSpecialChars->setChecked(config()->get(Config::PasswordGenerator_SpecialChars).toBool());
    }

    m_ui->checkBoxBraces->setChecked(config()->get(Config::PasswordGenerator_Braces).toBool());
    m_ui->checkBoxQuotes->setChecked(config()->get(Config::PasswordGenerator_Quotes).toBool());
    m_ui->checkBoxPunctuation->setChecked(config()->get(Config::PasswordGenerator_Punctuation).toBool());
    m_ui->checkBoxDashes->setChecked(config()->get(Config::PasswordGenerator_Dashes).toBool());
    m_ui->checkBoxMath->setChecked(config()->get(Config::PasswordGenerator_Math).toBool());

    m_ui->checkBoxExtASCII->setChecked(config()->get(Config::PasswordGenerator_EASCII).toBool());
    m_ui->checkBoxExcludeAlike->setChecked(config()->get(Config::PasswordGenerator_ExcludeAlike).toBool());
    m_ui->checkBoxEnsureEvery->setChecked(config()->get(Config::PasswordGenerator_EnsureEvery).toBool());
    m_ui->spinBoxLength->setValue(config()->get(Config::PasswordGenerator_Length).toInt());

    // Diceware config
    m_ui->spinBoxWordCount->setValue(config()->get(Config::PasswordGenerator_WordCount).toInt());
    m_ui->editWordSeparator->setText(config()->get(Config::PasswordGenerator_WordSeparator).toString());
    m_ui->comboBoxWordList->setCurrentText(config()->get(Config::PasswordGenerator_WordList).toString());
    m_ui->wordCaseComboBox->setCurrentIndex(config()->get(Config::PasswordGenerator_WordCase).toInt());

    // Password or diceware?
    m_ui->tabWidget->setCurrentIndex(config()->get(Config::PasswordGenerator_Type).toInt());

    // Set advanced mode
    m_ui->buttonAdvancedMode->setChecked(advanced);
    setAdvancedMode(advanced);
    updateGenerator();
}

void PasswordGeneratorWidget::saveSettings()
{
    // Password config
    config()->set(Config::PasswordGenerator_LowerCase, m_ui->checkBoxLower->isChecked());
    config()->set(Config::PasswordGenerator_UpperCase, m_ui->checkBoxUpper->isChecked());
    config()->set(Config::PasswordGenerator_Numbers, m_ui->checkBoxNumbers->isChecked());
    config()->set(Config::PasswordGenerator_EASCII, m_ui->checkBoxExtASCII->isChecked());

    config()->set(Config::PasswordGenerator_AdvancedMode, m_ui->buttonAdvancedMode->isChecked());
    if (m_ui->buttonAdvancedMode->isChecked()) {
        config()->set(Config::PasswordGenerator_Logograms, m_ui->checkBoxSpecialChars->isChecked());
    } else {
        config()->set(Config::PasswordGenerator_SpecialChars, m_ui->checkBoxSpecialChars->isChecked());
    }
    config()->set(Config::PasswordGenerator_Braces, m_ui->checkBoxBraces->isChecked());
    config()->set(Config::PasswordGenerator_Punctuation, m_ui->checkBoxPunctuation->isChecked());
    config()->set(Config::PasswordGenerator_Quotes, m_ui->checkBoxQuotes->isChecked());
    config()->set(Config::PasswordGenerator_Dashes, m_ui->checkBoxDashes->isChecked());
    config()->set(Config::PasswordGenerator_Math, m_ui->checkBoxMath->isChecked());

    config()->set(Config::PasswordGenerator_AdditionalChars, m_ui->editAdditionalChars->text());
    config()->set(Config::PasswordGenerator_ExcludedChars, m_ui->editExcludedChars->text());
    config()->set(Config::PasswordGenerator_ExcludeAlike, m_ui->checkBoxExcludeAlike->isChecked());
    config()->set(Config::PasswordGenerator_EnsureEvery, m_ui->checkBoxEnsureEvery->isChecked());
    config()->set(Config::PasswordGenerator_Length, m_ui->spinBoxLength->value());

    // Diceware config
    config()->set(Config::PasswordGenerator_WordCount, m_ui->spinBoxWordCount->value());
    config()->set(Config::PasswordGenerator_WordSeparator, m_ui->editWordSeparator->text());
    config()->set(Config::PasswordGenerator_WordList, m_ui->comboBoxWordList->currentText());
    config()->set(Config::PasswordGenerator_WordCase, m_ui->wordCaseComboBox->currentIndex());

    // Password or diceware?
    config()->set(Config::PasswordGenerator_Type, m_ui->tabWidget->currentIndex());
}

void PasswordGeneratorWidget::setPasswordLength(int length)
{
    if (length > 0) {
        m_ui->spinBoxLength->setValue(length);
    } else {
        m_ui->spinBoxLength->setValue(config()->get(Config::PasswordGenerator_Length).toInt());
    }
}

void PasswordGeneratorWidget::setStandaloneMode(bool standalone)
{
    m_standalone = standalone;
    if (standalone) {
        m_ui->buttonApply->setVisible(false);
        m_ui->buttonAutoType->setVisible(false);
        setPasswordVisible(true);
    } else {
        m_ui->buttonApply->setVisible(true);
        m_ui->buttonAutoType->setVisible(true);
    }
}

QString PasswordGeneratorWidget::getGeneratedPassword()
{
    return m_ui->editNewPassword->text();
}

void PasswordGeneratorWidget::regeneratePassword()
{
    if (m_ui->tabWidget->currentIndex() == Password) {
        if (m_passwordGenerator->isValid()) {
            QString password = m_passwordGenerator->generatePassword();
            m_ui->editNewPassword->setText(password);
            updatePasswordStrength(password);
        }
    } else {
        if (m_dicewareGenerator->isValid()) {
            QString password = m_dicewareGenerator->generatePassphrase();
            m_ui->editNewPassword->setText(password);
            updatePasswordStrength(password);
        }
    }
}

void PasswordGeneratorWidget::updateButtonsEnabled(const QString& password)
{
    if (!m_standalone) {
        m_ui->buttonApply->setEnabled(!password.isEmpty());
    }
    m_ui->buttonCopy->setEnabled(!password.isEmpty());
}

void PasswordGeneratorWidget::updatePasswordStrength(const QString& password)
{
    PasswordHealth health(password);
    if (m_ui->tabWidget->currentIndex() == Diceware) {
        // Diceware estimates entropy differently
        health = PasswordHealth(m_dicewareGenerator->estimateEntropy());

        m_ui->charactersInPassphraseLabel->setText(QString::number(password.length()));
    }

    m_ui->entropyLabel->setText(tr("Entropy: %1 bit").arg(QString::number(health.entropy(), 'f', 2)));

    m_ui->entropyProgressBar->setValue(std::min(int(health.entropy()), m_ui->entropyProgressBar->maximum()));

    colorStrengthIndicator(health);
}

void PasswordGeneratorWidget::applyPassword()
{
    saveSettings();
    emit appliedPassword(m_ui->editNewPassword->text());
    emit closed();
}

void PasswordGeneratorWidget::copyPassword()
{
    clipboard()->setText(m_ui->editNewPassword->text());
}

void PasswordGeneratorWidget::autoTypePassword()
{
    emit doAutoTypePassword(m_ui->editNewPassword->text());
}

void PasswordGeneratorWidget::passwordLengthChanged(int length)
{
    m_ui->spinBoxLength->blockSignals(true);
    m_ui->sliderLength->blockSignals(true);

    m_ui->spinBoxLength->setValue(length);
    m_ui->sliderLength->setValue(length);

    m_ui->spinBoxLength->blockSignals(false);
    m_ui->sliderLength->blockSignals(false);

    updateGenerator();
}

void PasswordGeneratorWidget::passphraseLengthChanged(int length)
{
    m_ui->spinBoxWordCount->blockSignals(true);
    m_ui->sliderWordCount->blockSignals(true);

    m_ui->spinBoxWordCount->setValue(length);
    m_ui->sliderWordCount->setValue(length);

    m_ui->spinBoxWordCount->blockSignals(false);
    m_ui->sliderWordCount->blockSignals(false);

    updateGenerator();
}

void PasswordGeneratorWidget::setPasswordVisible(bool visible)
{
    m_ui->editNewPassword->setShowPassword(visible);
}

bool PasswordGeneratorWidget::isPasswordVisible() const
{
    return m_ui->editNewPassword->isPasswordVisible();
}

void PasswordGeneratorWidget::setAdvancedMode(bool advanced)
{
    saveSettings();

    if (advanced) {
        m_ui->checkBoxSpecialChars->setText("# $ % && @ ^ ` ~");
        m_ui->checkBoxSpecialChars->setToolTip(tr("Logograms"));
        m_ui->checkBoxSpecialChars->setChecked(config()->get(Config::PasswordGenerator_Logograms).toBool());
    } else {
        m_ui->checkBoxSpecialChars->setText("/ * + && …");
        m_ui->checkBoxSpecialChars->setToolTip(tr("Special Characters"));
        m_ui->checkBoxSpecialChars->setChecked(config()->get(Config::PasswordGenerator_SpecialChars).toBool());
    }

    m_ui->advancedContainer->setVisible(advanced);
    m_ui->checkBoxBraces->setVisible(advanced);
    m_ui->checkBoxPunctuation->setVisible(advanced);
    m_ui->checkBoxQuotes->setVisible(advanced);
    m_ui->checkBoxMath->setVisible(advanced);
    m_ui->checkBoxDashes->setVisible(advanced);

    if (!m_standalone) {
        QTimer::singleShot(50, this, [this] { adjustSize(); });
    }
}

void PasswordGeneratorWidget::excludeHexChars()
{
    m_ui->editExcludedChars->setText("GHIJKLMNOPQRSTUVWXYZghijklmnopqrstuvwxyz");
    m_ui->checkBoxNumbers->setChecked(true);
    m_ui->checkBoxUpper->setChecked(true);

    m_ui->checkBoxLower->setChecked(false);
    m_ui->checkBoxSpecialChars->setChecked(false);
    m_ui->checkBoxExtASCII->setChecked(false);
    m_ui->checkBoxPunctuation->setChecked(false);
    m_ui->checkBoxQuotes->setChecked(false);
    m_ui->checkBoxDashes->setChecked(false);
    m_ui->checkBoxMath->setChecked(false);
    m_ui->checkBoxBraces->setChecked(false);

    updateGenerator();
}

void PasswordGeneratorWidget::colorStrengthIndicator(const PasswordHealth& health)
{
    // Take the existing stylesheet and convert the text and background color to arguments
    QString style = m_ui->entropyProgressBar->styleSheet();
    QRegularExpression re("(QProgressBar::chunk\\s*\\{.*?background-color:)[^;]+;",
                          QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    style.replace(re, "\\1 %1;");

    StateColorPalette statePalette;
    switch (health.quality()) {
    case PasswordHealth::Quality::Bad:
    case PasswordHealth::Quality::Poor:
        m_ui->entropyProgressBar->setStyleSheet(
            style.arg(statePalette.color(StateColorPalette::HealthCritical).name()));
        m_ui->strengthLabel->setText(tr("Password Quality: %1").arg(tr("Poor", "Password quality")));
        break;

    case PasswordHealth::Quality::Weak:
        m_ui->entropyProgressBar->setStyleSheet(style.arg(statePalette.color(StateColorPalette::HealthBad).name()));
        m_ui->strengthLabel->setText(tr("Password Quality: %1").arg(tr("Weak", "Password quality")));
        break;

    case PasswordHealth::Quality::Good:
        m_ui->entropyProgressBar->setStyleSheet(style.arg(statePalette.color(StateColorPalette::HealthOk).name()));
        m_ui->strengthLabel->setText(tr("Password Quality: %1").arg(tr("Good", "Password quality")));
        break;

    case PasswordHealth::Quality::Excellent:
        m_ui->entropyProgressBar->setStyleSheet(
            style.arg(statePalette.color(StateColorPalette::HealthExcellent).name()));
        m_ui->strengthLabel->setText(tr("Password Quality: %1").arg(tr("Excellent", "Password quality")));
        break;
    }
}

PasswordGenerator::CharClasses PasswordGeneratorWidget::charClasses()
{
    PasswordGenerator::CharClasses classes;

    if (m_ui->checkBoxLower->isChecked()) {
        classes |= PasswordGenerator::LowerLetters;
    }

    if (m_ui->checkBoxUpper->isChecked()) {
        classes |= PasswordGenerator::UpperLetters;
    }

    if (m_ui->checkBoxNumbers->isChecked()) {
        classes |= PasswordGenerator::Numbers;
    }

    if (m_ui->checkBoxExtASCII->isChecked()) {
        classes |= PasswordGenerator::EASCII;
    }

    if (!m_ui->buttonAdvancedMode->isChecked()) {
        if (m_ui->checkBoxSpecialChars->isChecked()) {
            classes |= PasswordGenerator::SpecialCharacters;
        }
    } else {
        if (m_ui->checkBoxBraces->isChecked()) {
            classes |= PasswordGenerator::Braces;
        }

        if (m_ui->checkBoxPunctuation->isChecked()) {
            classes |= PasswordGenerator::Punctuation;
        }

        if (m_ui->checkBoxQuotes->isChecked()) {
            classes |= PasswordGenerator::Quotes;
        }

        if (m_ui->checkBoxDashes->isChecked()) {
            classes |= PasswordGenerator::Dashes;
        }

        if (m_ui->checkBoxMath->isChecked()) {
            classes |= PasswordGenerator::Math;
        }

        if (m_ui->checkBoxSpecialChars->isChecked()) {
            classes |= PasswordGenerator::Logograms;
        }
    }

    return classes;
}

PasswordGenerator::GeneratorFlags PasswordGeneratorWidget::generatorFlags()
{
    PasswordGenerator::GeneratorFlags flags;

    if (m_ui->buttonAdvancedMode->isChecked()) {
        if (m_ui->checkBoxExcludeAlike->isChecked()) {
            flags |= PasswordGenerator::ExcludeLookAlike;
        }

        if (m_ui->checkBoxEnsureEvery->isChecked()) {
            flags |= PasswordGenerator::CharFromEveryGroup;
        }
    }

    return flags;
}

void PasswordGeneratorWidget::updateGenerator()
{
    if (m_ui->tabWidget->currentIndex() == Password) {
        auto classes = charClasses();
        auto flags = generatorFlags();

        int length = 0;
        if (flags.testFlag(PasswordGenerator::CharFromEveryGroup)) {
            if (classes.testFlag(PasswordGenerator::LowerLetters)) {
                ++length;
            }
            if (classes.testFlag(PasswordGenerator::UpperLetters)) {
                ++length;
            }
            if (classes.testFlag(PasswordGenerator::Numbers)) {
                ++length;
            }
            if (classes.testFlag(PasswordGenerator::Braces)) {
                ++length;
            }
            if (classes.testFlag(PasswordGenerator::Punctuation)) {
                ++length;
            }
            if (classes.testFlag(PasswordGenerator::Quotes)) {
                ++length;
            }
            if (classes.testFlag(PasswordGenerator::Dashes)) {
                ++length;
            }
            if (classes.testFlag(PasswordGenerator::Math)) {
                ++length;
            }
            if (classes.testFlag(PasswordGenerator::Logograms)) {
                ++length;
            }
            if (classes.testFlag(PasswordGenerator::EASCII)) {
                ++length;
            }
        }

        length = qMax(length, m_ui->spinBoxLength->value());
        m_passwordGenerator->setLength(length);
        m_passwordGenerator->setCharClasses(classes);
        m_passwordGenerator->setFlags(flags);
        if (m_ui->buttonAdvancedMode->isChecked()) {
            m_passwordGenerator->setAdditionalChars(m_ui->editAdditionalChars->text());
            m_passwordGenerator->setExcludedChars(m_ui->editExcludedChars->text());
        } else {
            m_passwordGenerator->setAdditionalChars("");
            m_passwordGenerator->setExcludedChars("");
        }

        if (m_passwordGenerator->isValid()) {
            m_ui->buttonGenerate->setEnabled(true);
        } else {
            m_ui->buttonGenerate->setEnabled(false);
        }
    } else {
        m_dicewareGenerator->setWordCase(
            static_cast<PassphraseGenerator::PassphraseWordCase>(m_ui->wordCaseComboBox->currentData().toInt()));

        m_dicewareGenerator->setWordCount(m_ui->spinBoxWordCount->value());
        if (!m_ui->comboBoxWordList->currentText().isEmpty()) {
            QString path = resources()->wordlistPath(m_ui->comboBoxWordList->currentText());
            m_dicewareGenerator->setWordList(path);
        }
        m_dicewareGenerator->setWordSeparator(m_ui->editWordSeparator->text());

        if (m_dicewareGenerator->isValid()) {
            m_ui->buttonGenerate->setEnabled(true);
        } else {
            m_ui->buttonGenerate->setEnabled(false);
        }
    }

    regeneratePassword();
}
