// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/kevamynamespacesdialog.h>
#include <qt/forms/ui_kevamynamespacesdialog.h>

#include <qt/kevanamespacemodel.h>

#include <QModelIndex>

KevaMyNamespacesDialog::KevaMyNamespacesDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::KevaMyNamespacesDialog)
{
    ui->setupUi(this);
}

void KevaMyNamespacesDialog::setModel(WalletModel *_model)
{
    this->model = _model;

    if(_model && _model->getOptionsModel())
    {
        _model->getKevaNamespaceModel()->sort(KevaNamespaceModel::Name, Qt::DescendingOrder);
        QTableView* tableView = ui->namespaceView;

        tableView->verticalHeader()->hide();
        tableView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        tableView->setModel(_model->getKevaNamespaceModel());
        tableView->setAlternatingRowColors(true);
        tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
        tableView->setSelectionMode(QAbstractItemView::ContiguousSelection);
        //tableView->setColumnWidth(KevaNamespaceModel::Id, ID_COLUMN_WIDTH);
        //tableView->setColumnWidth(KevaNamespaceModel::Name, NAME_COLUMN_WIDTH);
        tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

        connect(tableView->selectionModel(),
            SIGNAL(selectionChanged(QItemSelection, QItemSelection)), this,
            SLOT(recentRequestsView_selectionChanged(QItemSelection, QItemSelection)));
    }
}

void KevaMyNamespacesDialog::accept()
{
    // Create the namespace here.
    QDialog::accept();
}

KevaMyNamespacesDialog::~KevaMyNamespacesDialog()
{
    delete ui;
}