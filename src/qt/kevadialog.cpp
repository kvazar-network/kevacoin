// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/wallet.h>
#include <keva/common.h>

#include <qt/kevadialog.h>
#include <qt/forms/ui_kevadialog.h>

#include <qt/addressbookpage.h>
#include <qt/addresstablemodel.h>
#include <qt/bitcoinunits.h>
#include <qt/optionsmodel.h>
#include <qt/platformstyle.h>
#include <qt/receiverequestdialog.h>
#include <qt/kevatablemodel.h>
#include <qt/kevadetaildialog.h>
#include <qt/walletmodel.h>

#include <QAction>
#include <QCursor>
#include <QMessageBox>
#include <QScrollBar>
#include <QTextDocument>

KevaDialog::KevaDialog(const PlatformStyle *_platformStyle, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::KevaDialog),
    columnResizingFixer(0),
    model(0),
    platformStyle(_platformStyle)
{
    ui->setupUi(this);

    if (!_platformStyle->getImagesOnButtons()) {
        ui->receiveButton->setIcon(QIcon());
        ui->showRequestButton->setIcon(QIcon());
        ui->removeRequestButton->setIcon(QIcon());
    } else {
        ui->receiveButton->setIcon(_platformStyle->SingleColorIcon(":/icons/address-book"));
        ui->showRequestButton->setIcon(_platformStyle->SingleColorIcon(":/icons/edit"));
        ui->removeRequestButton->setIcon(_platformStyle->SingleColorIcon(":/icons/remove"));
    }

    // context menu actions
    QAction *copyURIAction = new QAction(tr("Copy URI"), this);
    QAction *copyLabelAction = new QAction(tr("Copy label"), this);
    QAction *copyMessageAction = new QAction(tr("Copy message"), this);
    QAction *copyAmountAction = new QAction(tr("Copy amount"), this);

    // context menu
    contextMenu = new QMenu(this);
    contextMenu->addAction(copyURIAction);
    contextMenu->addAction(copyLabelAction);
    contextMenu->addAction(copyMessageAction);
    contextMenu->addAction(copyAmountAction);

    // context menu signals
    connect(ui->kevaView, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showMenu(QPoint)));
    connect(copyURIAction, SIGNAL(triggered()), this, SLOT(copyURI()));
    connect(copyLabelAction, SIGNAL(triggered()), this, SLOT(copyLabel()));
    connect(copyMessageAction, SIGNAL(triggered()), this, SLOT(copyMessage()));
    connect(copyAmountAction, SIGNAL(triggered()), this, SLOT(copyAmount()));

}

void KevaDialog::setModel(WalletModel *_model)
{
    this->model = _model;

    if(_model && _model->getOptionsModel())
    {
        _model->getKevaTableModel()->sort(KevaTableModel::Block, Qt::DescendingOrder);
        QTableView* tableView = ui->kevaView;

        tableView->verticalHeader()->hide();
        tableView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        tableView->setModel(_model->getKevaTableModel());
        tableView->setAlternatingRowColors(true);
        tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
        tableView->setSelectionMode(QAbstractItemView::ContiguousSelection);
        tableView->setColumnWidth(KevaTableModel::Date, DATE_COLUMN_WIDTH);
        tableView->setColumnWidth(KevaTableModel::Key, KEY_COLUMN_WIDTH);
        tableView->setColumnWidth(KevaTableModel::Block, BLOCK_MINIMUM_COLUMN_WIDTH);

        connect(tableView->selectionModel(),
            SIGNAL(selectionChanged(QItemSelection, QItemSelection)), this,
            SLOT(recentRequestsView_selectionChanged(QItemSelection, QItemSelection)));
        // Last 2 columns are set by the columnResizingFixer, when the table geometry is ready.
        columnResizingFixer = new GUIUtil::TableViewLastColumnResizingFixer(tableView, BLOCK_MINIMUM_COLUMN_WIDTH, DATE_COLUMN_WIDTH, this);
    }
}

KevaDialog::~KevaDialog()
{
    delete ui;
}

void KevaDialog::clear()
{
    ui->nameSpace->setText("");
    updateDisplayUnit();
}

void KevaDialog::reject()
{
    clear();
}

void KevaDialog::accept()
{
    clear();
}

void KevaDialog::updateDisplayUnit()
{
    if(model && model->getOptionsModel())
    {
    }
}

void KevaDialog::on_showContent_clicked()
{
    if(!model || !model->getKevaTableModel())
        return;

    valtype namespaceVal;
    QString nameSpace = ui->nameSpace->text();
    if (!DecodeKevaNamespace(nameSpace.toStdString(), Params(), namespaceVal)) {
        // TODO: show error dialog
        return;
    }

    std::vector<KevaEntry> vKevaEntries;
    model->getKevaEntries(vKevaEntries, ValtypeToString(namespaceVal));
    model->getKevaTableModel()->setKeva(std::move(vKevaEntries));
    model->getKevaTableModel()->sort(KevaTableModel::Date, Qt::DescendingOrder);
}

void KevaDialog::on_kevaView_doubleClicked(const QModelIndex &index)
{
    KevaDetailDialog *dialog = new KevaDetailDialog(index, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void KevaDialog::kevaView_selectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    // Enable Show/Remove buttons only if anything is selected.
    bool enable = !ui->kevaView->selectionModel()->selectedRows().isEmpty();
    ui->showRequestButton->setEnabled(enable);
    ui->removeRequestButton->setEnabled(enable);
}

void KevaDialog::on_showRequestButton_clicked()
{
    if(!model || !model->getKevaTableModel() || !ui->kevaView->selectionModel())
        return;
    QModelIndexList selection = ui->kevaView->selectionModel()->selectedRows();

    for (const QModelIndex& index : selection) {
        on_kevaView_doubleClicked(index);
    }
}

void KevaDialog::on_removeRequestButton_clicked()
{
    if(!model || !model->getKevaTableModel() || !ui->kevaView->selectionModel())
        return;
    QModelIndexList selection = ui->kevaView->selectionModel()->selectedRows();
    if(selection.empty())
        return;
    // correct for selection mode ContiguousSelection
    QModelIndex firstIndex = selection.at(0);
    model->getKevaTableModel()->removeRows(firstIndex.row(), selection.length(), firstIndex.parent());
}

// We override the virtual resizeEvent of the QWidget to adjust tables column
// sizes as the tables width is proportional to the dialogs width.
void KevaDialog::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    columnResizingFixer->stretchColumnWidth(KevaTableModel::Block);
}

void KevaDialog::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Return)
    {
        // press return -> submit form
        if (ui->nameSpace->hasFocus())
        {
            event->ignore();
            on_showContent_clicked();
            return;
        }
    }

    this->QDialog::keyPressEvent(event);
}

QModelIndex KevaDialog::selectedRow()
{
    if(!model || !model->getKevaTableModel() || !ui->kevaView->selectionModel())
        return QModelIndex();
    QModelIndexList selection = ui->kevaView->selectionModel()->selectedRows();
    if(selection.empty())
        return QModelIndex();
    // correct for selection mode ContiguousSelection
    QModelIndex firstIndex = selection.at(0);
    return firstIndex;
}

// copy column of selected row to clipboard
void KevaDialog::copyColumnToClipboard(int column)
{
    QModelIndex firstIndex = selectedRow();
    if (!firstIndex.isValid()) {
        return;
    }
    GUIUtil::setClipboard(model->getKevaTableModel()->data(firstIndex.child(firstIndex.row(), column), Qt::EditRole).toString());
}

// context menu
void KevaDialog::showMenu(const QPoint &point)
{
    if (!selectedRow().isValid()) {
        return;
    }
    contextMenu->exec(QCursor::pos());
}

// context menu action: copy URI
void KevaDialog::copyURI()
{
#if 0
    QModelIndex sel = selectedRow();
    if (!sel.isValid()) {
        return;
    }

    const KevaTableModel * const submodel = model->getKevaTableModel();
    const QString uri = GUIUtil::formatBitcoinURI(submodel->entry(sel.row()).recipient);
    GUIUtil::setClipboard(uri);
#endif
}

// context menu action: copy label
void KevaDialog::copyLabel()
{
    copyColumnToClipboard(KevaTableModel::Key);
}

// context menu action: copy message
void KevaDialog::copyMessage()
{
    copyColumnToClipboard(KevaTableModel::Value);
}

// context menu action: copy amount
void KevaDialog::copyAmount()
{
    copyColumnToClipboard(KevaTableModel::Block);
}
