// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/kevadetaildialog.h>
#include <qt/forms/ui_kevadetaildialog.h>

#include <qt/kevatablemodel.h>

#include <QModelIndex>

KevaDetailDialog::KevaDetailDialog(const QModelIndex &idx, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::KevaDetailDialog)
{
    ui->setupUi(this);
    QModelIndex keyIdx = idx.sibling(idx.row(), KevaTableModel::Key);
    QModelIndex valueIdx = idx.sibling(idx.row(), KevaTableModel::Value);
    setWindowTitle(tr("Value for %1").arg(keyIdx.data(Qt::DisplayRole).toString()));
    QString desc = valueIdx.data(Qt::DisplayRole).toString();
    ui->detailText->setHtml(desc);
}

KevaDetailDialog::~KevaDetailDialog()
{
    delete ui;
}