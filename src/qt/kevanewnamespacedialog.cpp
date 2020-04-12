// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/kevanewnamespacedialog.h>
#include <qt/forms/ui_kevanewnamespacedialog.h>

#include <qt/kevatablemodel.h>
#include <qt/kevadialog.h>

#include <QPushButton>
#include <QModelIndex>

KevaNewNamespaceDialog::KevaNewNamespaceDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::KevaNewNamespaceDialog)
{
    ui->setupUi(this);
    connect(ui->buttonBox->button(QDialogButtonBox::Cancel), SIGNAL(clicked()), this, SLOT(reject()));
    connect(ui->buttonBox->button(QDialogButtonBox::Save), SIGNAL(clicked()), this, SLOT(accept()));
    connect(ui->namespaceText, SIGNAL(textChanged(const QString &)), this, SLOT(onNamespaceChanged(const QString &)));
    ui->buttonBox->button(QDialogButtonBox::Save)->setEnabled(false);
}

void KevaNewNamespaceDialog::onNamespaceChanged(const QString & ns)
{
    int length = ns.length();
    bool enabled = length > 0;
    ui->buttonBox->button(QDialogButtonBox::Save)->setEnabled(enabled);
}

void KevaNewNamespaceDialog::accept()
{
    KevaDialog* dialog = (KevaDialog*)this->parentWidget();
    QString nsText  = ui->namespaceText->text();
    std::string namespaceId;
    if (!dialog->createNamespace(nsText.toStdString(), namespaceId)) {
        //TODO: error message.
        return;
    }
    dialog->showNamespace(QString::fromStdString(namespaceId));
    QDialog::accept();
}

void KevaNewNamespaceDialog::reject()
{
}

KevaNewNamespaceDialog::~KevaNewNamespaceDialog()
{
    delete ui;
}