// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/kevabookmarksdialog.h>
#include <qt/forms/ui_kevabookmarksdialog.h>
#include <qt/kevaeditbookmarkdialog.h>

#include <qt/kevabookmarksmodel.h>
#include <qt/kevadialog.h>

#include <QPushButton>
#include <QModelIndex>

KevaBookmarksDialog::KevaBookmarksDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::KevaBookmarksDialog)
{
    ui->setupUi(this);

    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("&Show"));
    ui->buttonBox->button(QDialogButtonBox::Save)->setEnabled(false);
    ui->buttonBox->button(QDialogButtonBox::Save)->setText(tr("&Edit"));
    connect(ui->buttonBox->button(QDialogButtonBox::Close), SIGNAL(clicked()), this, SLOT(reject()));
    connect(ui->buttonBox->button(QDialogButtonBox::Ok), SIGNAL(clicked()), this, SLOT(apply()));
    connect(ui->buttonBox->button(QDialogButtonBox::Save), SIGNAL(clicked()), this, SLOT(rename()));
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
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(enable);
    ui->buttonBox->button(QDialogButtonBox::Save)->setEnabled(enable);

    if (enable) {
        selectedIndex = ui->namespaceView->selectionModel()->currentIndex();
    } else {
        QModelIndex empty;
        selectedIndex = empty;
    }
}

void KevaBookmarksDialog::on_namespaceView_doubleClicked(const QModelIndex &index)
{
    selectedIndex = index;
    this->apply();
}

void KevaBookmarksDialog::apply()
{
    QModelIndex idIdx = selectedIndex.sibling(selectedIndex.row(), KevaBookmarksModel::Id);
    QString idStr = idIdx.data(Qt::DisplayRole).toString();
    KevaDialog* dialog = (KevaDialog*)this->parentWidget();
    dialog->showNamespace(idStr);
    QDialog::close();
}

void KevaBookmarksDialog::rename()
{
    QModelIndex idIdx = selectedIndex.sibling(selectedIndex.row(), KevaBookmarksModel::Id);
    QString idStr = idIdx.data(Qt::DisplayRole).toString();
    QModelIndex nameIdx = selectedIndex.sibling(selectedIndex.row(), KevaBookmarksModel::Name);
    QString nameStr = nameIdx.data(Qt::DisplayRole).toString();
    KevaEditBookmarkDialog *dialog = new KevaEditBookmarkDialog(this, idStr, nameStr);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void KevaBookmarksDialog::saveName(const QString& id, const QString& name)
{
    QJsonArray array;
    KevaBookmarksModel* bookmarksModel = this->model->getKevaBookmarksModel();
    bookmarksModel->loadBookmarks(array);

    for (int i = 0; i < array.size(); ++i) {
        QJsonObject obj = array[i].toObject();
        BookmarkEntry entry;
        QString idStr = obj["id"].toString();
        if (idStr == id) {
            QJsonObject entry;
            entry["id"] = id;
            entry["name"] = name;
            array.replace(i, entry);
            break;
        }
    }
    bookmarksModel->saveBookmarks(array);
    bookmarksModel->loadBookmarks();
}

void KevaBookmarksDialog::reject()
{
    QDialog::reject();
}

KevaBookmarksDialog::~KevaBookmarksDialog()
{
    delete ui;
}