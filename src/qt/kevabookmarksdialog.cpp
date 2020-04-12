// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/kevabookmarksdialog.h>
#include <qt/forms/ui_kevabookmarksdialog.h>

#include <qt/kevabookmarksmodel.h>
#include <qt/kevadialog.h>

#include <QPushButton>
#include <QModelIndex>

KevaBookmarksDialog::KevaBookmarksDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::KevaBookmarksDialog)
{
    ui->setupUi(this);
    ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
    ui->buttonBox->button(QDialogButtonBox::Apply)->setText(tr("Show"));
    connect(ui->buttonBox->button(QDialogButtonBox::Cancel), SIGNAL(clicked()), this, SLOT(reject()));
    connect(ui->buttonBox->button(QDialogButtonBox::Apply), SIGNAL(clicked()), this, SLOT(apply()));
}

void KevaBookmarksDialog::setModel(WalletModel *_model)
{
    this->model = _model;

    if(_model && _model->getOptionsModel())
    {
        _model->getKevaBookmarksModel()->sort(KevaBookmarksModel::Name, Qt::DescendingOrder);
        QTableView* tableView = ui->namespaceView;

        tableView->verticalHeader()->hide();
        tableView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        tableView->setModel(_model->getKevaBookmarksModel());
        tableView->setAlternatingRowColors(true);
        tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
        tableView->setSelectionMode(QAbstractItemView::ContiguousSelection);
        tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

        connect(tableView->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
        this, SLOT(namespaceView_selectionChanged()));
    }
}


void KevaBookmarksDialog::namespaceView_selectionChanged()
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

void KevaBookmarksDialog::apply()
{
    QModelIndex idIdx = selectedIndex.sibling(selectedIndex.row(), KevaBookmarksModel::Id);
    QString idStr = idIdx.data(Qt::DisplayRole).toString();
    KevaDialog* dialog = (KevaDialog*)this->parentWidget();
    dialog->showNamespace(idStr);
    QDialog::close();
}

void KevaBookmarksDialog::reject()
{
    QDialog::reject();
}

KevaBookmarksDialog::~KevaBookmarksDialog()
{
    delete ui;
}