// Copyright (c) 2011-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_KEVAMYNMAESPACESDIALOG_H
#define BITCOIN_QT_KEVAMYNMAESPACESDIALOG_H

#include <QObject>
#include <QString>

#include <QDialog>
#include <QItemSelection>
#include <QAbstractItemView>

class WalletModel;

namespace Ui {
    class KevaMyNamespacesDialog;
}


/** Dialog showing namepsace creation. */
class KevaMyNamespacesDialog : public QDialog
{
    Q_OBJECT

    enum ColumnWidths {
        ID_COLUMN_WIDTH = 260,
        NAME_COLUMN_WIDTH = 260,
        MINIMUM_COLUMN_WIDTH = 260
    };

public:
    explicit KevaMyNamespacesDialog(QWidget *parent = 0);
    ~KevaMyNamespacesDialog();
    void setModel(WalletModel *_model);

public Q_SLOTS:
    void apply();
    void reject();

private Q_SLOTS:
    void namespaceView_selectionChanged();
    void on_namespaceView_doubleClicked(const QModelIndex &index);

private:
    Ui::KevaMyNamespacesDialog *ui;
    WalletModel *model;
    QModelIndex selectedIndex;
};

#endif // BITCOIN_QT_KEVAMYNMAESPACESDIALOG_H
