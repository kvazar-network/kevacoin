// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/kevaaddkeydialog.h>
#include <qt/forms/ui_kevaaddkeydialog.h>

#include <qt/kevatablemodel.h>

#include <QPushButton>

KevaAddKeyDialog::KevaAddKeyDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::KevaAddKeyDialog)
{
    ui->setupUi(this);
    connect(ui->buttonBox->button(QDialogButtonBox::Cancel), SIGNAL(clicked()), this, SLOT(cancel()));
    connect(ui->buttonBox->button(QDialogButtonBox::Save), SIGNAL(clicked()), this, SLOT(create()));
    connect(ui->keyText, SIGNAL(textChanged(const QString &)), this, SLOT(onKeyChanged(const QString &)));
    ui->buttonBox->button(QDialogButtonBox::Save)->setEnabled(false);
}

KevaAddKeyDialog::~KevaAddKeyDialog()
{
    delete ui;
}

void KevaAddKeyDialog::create()
{
    QDialog::close();
}

void KevaAddKeyDialog::cancel()
{
    QDialog::close();
}

void KevaAddKeyDialog::onKeyChanged(const QString& key)
{
    bool enabled = key.length() > 0;
    ui->buttonBox->button(QDialogButtonBox::Save)->setEnabled(enabled);
}
