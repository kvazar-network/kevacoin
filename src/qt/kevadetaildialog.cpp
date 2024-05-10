// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/kevadetaildialog.h>
#include <qt/forms/ui_kevadetaildialog.h>

#include <qt/kevatablemodel.h>
#include <qt/kevadialog.h>

#include <QModelIndex>
#include <QPushButton>


KevaDetailDialog::KevaDetailDialog(const QModelIndex &idx, QWidget *parent, const QString &nameSpace) :
    QDialog(parent),
    ui(new Ui::KevaDetailDialog)
{
    ui->setupUi(this);

    this->nameSpace = nameSpace;

    setWindowTitle(
        idx.sibling(
            idx.row(),
            KevaTableModel::Key
        ).data(
            Qt::DisplayRole
        ).toString()
    );

    /* @TODO multi-tab data/meta browser
        ui->detailKey->setText(
        idx.sibling(
            idx.row(),
            KevaTableModel::Key
        ).data(
            Qt::DisplayRole
        ).toString()
    );
    */

    ui->detailValue->setText(
        idx.sibling(
            idx.row(),
            KevaTableModel::Value
        ).data(
            Qt::DisplayRole
        ).toString()
    );
}

KevaDetailDialog::~KevaDetailDialog()
{
    delete ui;
}