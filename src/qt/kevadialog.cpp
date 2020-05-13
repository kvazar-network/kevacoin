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
#include <qt/kevatablemodel.h>
#include <qt/kevanamespacemodel.h>
#include <qt/kevabookmarksmodel.h>
#include <qt/kevadetaildialog.h>
#include <qt/kevaaddkeydialog.h>
#include <qt/kevabookmarksdialog.h>
#include <qt/kevanewnamespacedialog.h>
#include <qt/kevamynamespacesdialog.h>
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
        ui->bookmarksButton->setIcon(QIcon());
        ui->showValueButton->setIcon(QIcon());
        ui->removeButton->setIcon(QIcon());
    } else {
        ui->bookmarksButton->setIcon(_platformStyle->SingleColorIcon(":/icons/address-book"));
        ui->showValueButton->setIcon(_platformStyle->SingleColorIcon(":/icons/edit"));
        ui->removeButton->setIcon(_platformStyle->SingleColorIcon(":/icons/remove"));
        ui->addKVButton->setIcon(_platformStyle->SingleColorIcon(":/icons/add"));
    }

    // context menu actions
    QAction *copyURIAction = new QAction(tr("Copy URI"), this);
    QAction *copyLabelAction = new QAction(tr("Copy label"), this);
    QAction *copyMessageAction = new QAction(tr("Copy message"), this);
    QAction *copyAmountAction = new QAction(tr("Copy amount"), this);

    bookmarks = new KevaBookmarksModel(NULL, NULL);
    bookmarks->loadBookmarks();

    ui->kevaView->setTextElideMode(Qt::ElideRight);
    ui->kevaView->setWordWrap(false);
    QHeaderView *verticalHeader = ui->kevaView->verticalHeader();
    verticalHeader->setSectionResizeMode(QHeaderView::Fixed);
    verticalHeader->setDefaultSectionSize(36);

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

    connect(ui->nameSpace, SIGNAL(textChanged(const QString &)), this, SLOT(onNamespaceChanged(const QString &)));
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

        connect(ui->kevaView->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
                this, SLOT(kevaView_selectionChanged()));

        // Last 2 columns are set by the columnResizingFixer, when the table geometry is ready.
        columnResizingFixer = new GUIUtil::TableViewLastColumnResizingFixer(tableView, BLOCK_MINIMUM_COLUMN_WIDTH, DATE_COLUMN_WIDTH, this);
    }
}

void KevaDialog::showNamespace(QString ns)
{
    ui->nameSpace->setText(ns);
    on_showContent_clicked();
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

void KevaDialog::on_createNamespace_clicked()
{
    if(!model || !model->getKevaTableModel())
        return;

    KevaNewNamespaceDialog *dialog = new KevaNewNamespaceDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void KevaDialog::onNamespaceChanged(const QString& nameSpace)
{
    std::string namespaceStr = nameSpace.toStdString();
    valtype nameSpaceVal;
    bool isValidNamespace = false;
    if (DecodeKevaNamespace(namespaceStr, Params(), nameSpaceVal)) {
        ui->addKVButton->setEnabled(true);
        isValidNamespace = true;
    } else {
        ui->addKVButton->setEnabled(false);
        ui->bookmarkNamespace->setIcon(QIcon(":/icons/star_empty"));
    }

    if (!isValidNamespace) {
        return;
    }

    if (bookmarks->isBookmarked(namespaceStr)) {
        ui->bookmarkNamespace->setIcon(QIcon(":/icons/star"));
    } else {
        ui->bookmarkNamespace->setIcon(QIcon(":/icons/star_empty"));
    }
}


void KevaDialog::on_listNamespaces_clicked()
{
    if(!model || !model->getKevaTableModel())
        return;

    KevaMyNamespacesDialog *dialog = new KevaMyNamespacesDialog(this);

    std::vector<NamespaceEntry> vNamespaceEntries;
    model->getNamespaceEntries(vNamespaceEntries);
    model->getKevaNamespaceModel()->setNamespace(std::move(vNamespaceEntries));
    model->getKevaNamespaceModel()->sort(KevaNamespaceModel::Name, Qt::DescendingOrder);

    dialog->setModel(model);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void KevaDialog::on_bookmarksButton_clicked()
{
    if(!model || !model->getKevaTableModel())
        return;

    KevaBookmarksDialog *dialog = new KevaBookmarksDialog(this);

    model->getKevaBookmarksModel()->loadBookmarks();
    model->getKevaBookmarksModel()->sort(KevaBookmarksModel::Name, Qt::DescendingOrder);

    dialog->setModel(model);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
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
    QString nameSpace = ui->nameSpace->text();
    KevaDetailDialog *dialog = new KevaDetailDialog(index, this, nameSpace);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void KevaDialog::on_bookmarkNamespace_clicked()
{
    valtype namespaceVal;
    QString nameSpace = ui->nameSpace->text();
    std::string namespaceStr = nameSpace.toStdString();
    if (!DecodeKevaNamespace(namespaceStr, Params(), namespaceVal)) {
        return;
    }
    QJsonArray array;
    bookmarks->loadBookmarks(array);

    int index = -1;
    for (int i = 0; i < array.size(); ++i) {
        QJsonObject obj = array[i].toObject();
        BookmarkEntry entry;
        std::string id = obj["id"].toString().toStdString();
        if (id == namespaceStr) {
            index = i;
            break;
        }
    }

    if (index >= 0) {
        // Remove bookmark
        array.removeAt(index);
        ui->bookmarkNamespace->setIcon(QIcon(":/icons/star_empty"));
    } else {
        // Add bookmark
        QJsonObject entry;
        entry["id"] = namespaceStr.c_str();
        entry["name"] = "";

        std::vector<NamespaceEntry> vNamespaceEntries;
        this->model->getNamespaceEntries(vNamespaceEntries);
        for(auto it = vNamespaceEntries.begin(); it != vNamespaceEntries.end(); it++) {
            if ((*it).id == namespaceStr) {
                entry["name"] = (*it).name.c_str();
                break;
            }
        }

        array.prepend(entry);
        ui->bookmarkNamespace->setIcon(QIcon(":/icons/star"));
    }

    bookmarks->saveBookmarks(array);
}

void KevaDialog::kevaView_selectionChanged()
{
    // Enable Show/Remove buttons only if anything is selected.
    bool enable = !ui->kevaView->selectionModel()->selectedRows().isEmpty();
    ui->showValueButton->setEnabled(enable);
    ui->removeButton->setEnabled(enable);
}

void KevaDialog::on_showValueButton_clicked()
{
    if(!model || !model->getKevaTableModel() || !ui->kevaView->selectionModel())
        return;
    QModelIndexList selection = ui->kevaView->selectionModel()->selectedRows();

    for (const QModelIndex& index : selection) {
        on_kevaView_doubleClicked(index);
    }
}

void KevaDialog::on_removeButton_clicked()
{
    if(!model || !model->getKevaTableModel() || !ui->kevaView->selectionModel())
        return;
    QModelIndexList selection = ui->kevaView->selectionModel()->selectedRows();
    if(selection.empty())
        return;

    QMessageBox::StandardButton reply;
    QModelIndex index = selection.at(0);
    QModelIndex keyIdx = index.sibling(index.row(), KevaTableModel::Key);
    QString keyStr = keyIdx.data(Qt::DisplayRole).toString();
    reply = QMessageBox::warning(this, tr("Warning"), tr("Delete the key \"%1\"?").arg(keyStr),
                                QMessageBox::Cancel|QMessageBox::Ok);

    if (reply == QMessageBox::Cancel) {
        return;
    }

    std::string nameSpace = ui->nameSpace->text().toStdString();
    std::string key = keyStr.toStdString();

    int ret = this->model->deleteKevaEntry(nameSpace, key);
    if (ret > 0) {
        QString msg;
        switch (ret) {
            case WalletModel::InvalidNamespace:
                msg = tr("Invalid namespace \"%1\"").arg(ui->nameSpace->text());
                break;
            case WalletModel::KeyNotFound:
                msg = tr("Key not found: \"%1\".").arg(keyStr);
                break;
            case WalletModel::InsufficientFund:
                msg = tr("Insufficient funds");
                break;
            case WalletModel::WalletLocked:
                return;
            default:
                msg = tr("Unknown error.");
        }
        QMessageBox::critical(this, tr("Error"), msg, QMessageBox::Ok);
        return;
    }

    // correct for selection mode ContiguousSelection
    QModelIndex firstIndex = selection.at(0);
    model->getKevaTableModel()->removeRows(firstIndex.row(), selection.length(), firstIndex.parent());
}

void KevaDialog::on_addKVButton_clicked()
{
    QString ns = ui->nameSpace->text();
    KevaAddKeyDialog *dialog = new KevaAddKeyDialog(this, ns);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
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


int KevaDialog::createNamespace(std::string displayName, std::string& namespaceId)
{
    if (!this->model) {
        return 0;
    }

    int ret = this->model->createNamespace(displayName, namespaceId);
    if (ret > 0) {
        QString msg;
        switch (ret) {
            case WalletModel::NamespaceTooLong:
                msg = tr("Namespace too long \"%1\"").arg(QString::fromStdString(displayName));
                break;
            case WalletModel::InsufficientFund:
                msg = tr("Insufficient funds");
                break;
            case WalletModel::WalletLocked:
                return 0;
            default:
                msg = tr("Unknown error.");
        }
        QMessageBox::critical(this, tr("Error"), msg, QMessageBox::Ok);
        return 0;
    }
    return 1;
}

int KevaDialog::addKeyValue(std::string& namespaceId, std::string& key, std::string& value)
{
    if (!this->model) {
        return 0;
    }

    int ret = this->model->addKeyValue(namespaceId, key, value);
    if (ret > 0) {
        QString msg;
        switch (ret) {
            case WalletModel::CannotUpdate:
                msg = tr("Cannot add key-value. Make sure you own this namespace.");
                break;
            case WalletModel::KeyTooLong:
                msg = tr("Key too long.");
                break;
            case WalletModel::ValueTooLong:
                msg = tr("Value too long.");
                break;
            case WalletModel::InsufficientFund:
                msg = tr("Insufficient funds");
                break;
            case WalletModel::WalletLocked:
                return 0;
            default:
                msg = tr("Unknown error.");
        }
        QMessageBox::critical(this, tr("Error"), msg, QMessageBox::Ok);
        return 0;
    }
    return 1;
}
