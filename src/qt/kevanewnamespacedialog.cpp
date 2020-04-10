// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/kevanewnamespacedialog.h>
#include <qt/forms/ui_kevanewnamespacedialog.h>

#include <qt/kevatablemodel.h>

#include <QModelIndex>

KevaNewNamespaceDialog::KevaNewNamespaceDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::KevaNewNamespaceDialog)
{
    ui->setupUi(this);
}

void KevaNewNamespaceDialog::accept()
{
    // Create the namespace here.
    QDialog::accept();
}

KevaNewNamespaceDialog::~KevaNewNamespaceDialog()
{
    delete ui;
}