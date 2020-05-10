// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/kevaeditbookmarkdialog.h>
#include <qt/forms/ui_kevaeditbookmarkdialog.h>
#include <qt/kevabookmarksdialog.h>

#include <qt/kevatablemodel.h>
#include <qt/kevadialog.h>

#include <QPushButton>
#include <QModelIndex>

KevaEditBookmarkDialog::KevaEditBookmarkDialog(QWidget *parent, const QString& id, const QString& name) :
    QDialog(parent), id(id), name(name),
    ui(new Ui::KevaEditBookmarkDialog)
{
    ui->setupUi(this);
    ui->bookmarkName->setText(name);
    this->parentDialog = (KevaBookmarksDialog*)parent;

    connect(ui->buttonBox->button(QDialogButtonBox::Cancel), SIGNAL(clicked()), this, SLOT(close()));
    connect(ui->buttonBox->button(QDialogButtonBox::Save), SIGNAL(clicked()), this, SLOT(save()));
    connect(ui->bookmarkName, SIGNAL(textChanged(const QString &)), this, SLOT(onNameChanged(const QString &)));
    ui->buttonBox->button(QDialogButtonBox::Save)->setEnabled(false);
}

void KevaEditBookmarkDialog::onNameChanged(const QString & name)
{
    int length = name.length();
    bool enabled = length > 0;
    ui->buttonBox->button(QDialogButtonBox::Save)->setEnabled(enabled);
    this->name = name;
}

void KevaEditBookmarkDialog::save()
{
    KevaDialog* dialog = (KevaDialog*)this->parentWidget();
    QString bookmarkText  = ui->bookmarkName->text();
    this->parentDialog->saveName(id, name);
    QDialog::close();
}

void KevaEditBookmarkDialog::close()
{
    QDialog::close();
}

KevaEditBookmarkDialog::~KevaEditBookmarkDialog()
{
    delete ui;
}