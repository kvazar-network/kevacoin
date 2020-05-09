// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/kevamynamespacesdialog.h>
#include <qt/forms/ui_kevamynamespacesdialog.h>

#include <qt/kevanamespacemodel.h>
#include <qt/kevadialog.h>

#include <QPushButton>
#include <QModelIndex>

KevaMyNamespacesDialog::KevaMyNamespacesDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::KevaMyNamespacesDialog)
{
    ui->setupUi(this);
    ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
    ui->buttonBox->button(QDialogButtonBox::Apply)->setText(tr("Show"));
    connect(ui->buttonBox->button(QDialogButtonBox::Cancel), SIGNAL(clicked()), this, SLOT(reject()));
    connect(ui->buttonBox->button(QDialogButtonBox::Apply), SIGNAL(clicked()), this, SLOT(apply()));
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

        connect(tableView->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
        this, SLOT(namespaceView_selectionChanged()));
    }
}


void KevaMyNamespacesDialog::namespaceView_selectionChanged()
{
    bool enable = !ui->namespaceView->selectionModel()->selectedRows().isEmpty();
    ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(enable);

    if (enable) {
        selectedIndex = ui->namespaceView->selectionModel()->currentIndex();
    } else {
        QModelIndex empty;
        selectedIndex = empty;
    }
}

void KevaMyNamespacesDialog::on_namespaceView_doubleClicked(const QModelIndex &index)
{
    selectedIndex = ui->namespaceView->selectionModel()->currentIndex();
    this->apply();
}

void KevaMyNamespacesDialog::apply()
{
    QModelIndex idIdx = selectedIndex.sibling(selectedIndex.row(), KevaNamespaceModel::Id);
    QString idStr = idIdx.data(Qt::DisplayRole).toString();
    KevaDialog* dialog = (KevaDialog*)this->parentWidget();
    dialog->showNamespace(idStr);
    QDialog::close();
}

void KevaMyNamespacesDialog::reject()
{
    QDialog::reject();
}

KevaMyNamespacesDialog::~KevaMyNamespacesDialog()
{
    delete ui;
}