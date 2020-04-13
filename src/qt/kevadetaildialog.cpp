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
    QModelIndex keyIdx = idx.sibling(idx.row(), KevaTableModel::Key);
    QModelIndex valueIdx = idx.sibling(idx.row(), KevaTableModel::Value);
    this->nameSpace = nameSpace;
    key = keyIdx.data(Qt::DisplayRole).toString();
    setWindowTitle(tr("Value for %1").arg(key));
    QString desc = valueIdx.data(Qt::DisplayRole).toString();
    connect(ui->detailText, SIGNAL(textChanged()), this, SLOT(onValueChanged()));
    //ui->detailText->setHtml(desc);
    ui->detailText->setPlainText(desc);
    ui->buttonBox->button(QDialogButtonBox::Save)->setEnabled(false);
    connect(ui->buttonBox->button(QDialogButtonBox::Save), SIGNAL(clicked()), this, SLOT(onSave()));
}

KevaDetailDialog::~KevaDetailDialog()
{
    delete ui;
}

void KevaDetailDialog::onValueChanged()
{
    bool enabled = ui->detailText->toPlainText().length() > 0;
    ui->buttonBox->button(QDialogButtonBox::Save)->setEnabled(enabled);
}

void KevaDetailDialog::onSave()
{
    KevaDialog* dialog = (KevaDialog*)this->parentWidget();
    std::string keyText  = key.toStdString();
    std::string valueText  = ui->detailText->toPlainText().toStdString();
    std::string ns = nameSpace.toStdString();
    if (!dialog->addKeyValue(ns, keyText, valueText)) {
        QDialog::close();
        return;
    }
    dialog->showNamespace(nameSpace);
    QDialog::close();
}
