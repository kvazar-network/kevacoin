// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/kevaaddkeydialog.h>
#include <qt/forms/ui_kevaaddkeydialog.h>

#include <qt/kevatablemodel.h>
#include <qt/kevadialog.h>

#include <QPushButton>

KevaAddKeyDialog::KevaAddKeyDialog(QWidget *parent, QString &nameSpace) :
    QDialog(parent),
    ui(new Ui::KevaAddKeyDialog)
{
    ui->setupUi(this);
    this->nameSpace = nameSpace;
    connect(ui->buttonBox->button(QDialogButtonBox::Cancel), SIGNAL(clicked()), this, SLOT(cancel()));
    connect(ui->buttonBox->button(QDialogButtonBox::Save), SIGNAL(clicked()), this, SLOT(create()));
    connect(ui->keyText, SIGNAL(textChanged(const QString &)), this, SLOT(onKeyChanged(const QString &)));
    connect(ui->valueText, SIGNAL(textChanged()), this, SLOT(onValueChanged()));
    ui->buttonBox->button(QDialogButtonBox::Save)->setEnabled(false);
}

KevaAddKeyDialog::~KevaAddKeyDialog()
{
    delete ui;
}

void KevaAddKeyDialog::setTitle(const QString &value)
{
    setWindowTitle(
        value
    );
}

void KevaAddKeyDialog::setKey(const QString &value)
{
    ui->keyText->setEnabled(
        true
    );

    ui->keyText->setText(
        value
    );

    ui->keyText->setFocus();
}

void KevaAddKeyDialog::setValue(const QString &value)
{
    ui->valueText->setPlainText(
        value
    );

    ui->valueText->setFocus();
}

void KevaAddKeyDialog::create()
{
    KevaDialog* dialog = (KevaDialog*)this->parentWidget();
    std::string keyText  = ui->keyText->text().toStdString();
    std::string valueText  = ui->valueText->toPlainText().toStdString();
    std::string ns = nameSpace.toStdString();
    if (!dialog->addKeyValue(ns, keyText, valueText)) {
        QDialog::close();
        return;
    }
    dialog->showNamespace(nameSpace);
    QDialog::close();
}

void KevaAddKeyDialog::cancel()
{
    QDialog::close();
}

void KevaAddKeyDialog::onKeyChanged(const QString& key)
{
    // Calculate current size
    int keySize   = key.toStdString().size();
    int valueSize = ui->valueText->toPlainText().toStdString().size();

    // Update counter value
    ui->keyCounter->setText(
        QString::number(
            keySize
        ) + "/" + QString::number(
            255
        )
    );

    // Update counter palette
    QPalette keyCounterPalette = ui->keyCounter->palette();

    keyCounterPalette.setColor(
        QPalette::WindowText,
        keySize > 255 ? QColor(230, 120, 105) : QColor(150, 227, 138)
    );

    ui->keyCounter->setPalette(
        keyCounterPalette
    );

    // Update button status
    bool enabled = keySize > 0    && valueSize > 0 &&
                   keySize <= 255 && valueSize < MAX_SCRIPT_ELEMENT_SIZE + 1;

    ui->buttonBox->button(QDialogButtonBox::Save)->setEnabled(enabled);
}

void KevaAddKeyDialog::onValueChanged()
{
    // Calculate current size
    int keySize   = ui->keyText->text().toStdString().size();
    int valueSize = ui->valueText->toPlainText().toStdString().size();

    // Update counter value
    ui->valueCounter->setText(
        QString::number(
            valueSize
        ) + "/" + QString::number(
            MAX_SCRIPT_ELEMENT_SIZE
        )
    );

    // Update counter palette
    QPalette valueCounterPalette = ui->valueCounter->palette();

    valueCounterPalette.setColor(
        QPalette::WindowText,
        valueSize > MAX_SCRIPT_ELEMENT_SIZE ? QColor(230, 120, 105) : QColor(150, 227, 138)
    );

    ui->valueCounter->setPalette(
        valueCounterPalette
    );

    // Update button status
    bool enabled = keySize > 0    && valueSize > 0 &&
                   keySize <= 255 && valueSize < MAX_SCRIPT_ELEMENT_SIZE + 1;

    ui->buttonBox->button(QDialogButtonBox::Save)->setEnabled(enabled);
}
