// Copyright (c) 2011-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_KEVABOOKMARKSDIALOG_H
#define BITCOIN_QT_KEVABOOKMARKSDIALOG_H

#include <QObject>
#include <QString>

#include <QDialog>
#include <QItemSelection>
#include <QAbstractItemView>

class WalletModel;

namespace Ui {
    class KevaBookmarksDialog;
}

/** Dialog showing namepsace creation. */
class KevaBookmarksDialog : public QDialog
{
    Q_OBJECT

    enum ColumnWidths {
        ID_COLUMN_WIDTH = 260,
        NAME_COLUMN_WIDTH = 260,
        MINIMUM_COLUMN_WIDTH = 260
    };

public:
    explicit KevaBookmarksDialog(QWidget *parent = 0);
    ~KevaBookmarksDialog();
    void setModel(WalletModel *_model);

public Q_SLOTS:
    void apply();
    void reject();
    void rename();
    void saveName(const QString& id, const QString& name);

private Q_SLOTS:
    void namespaceView_selectionChanged();
    void on_namespaceView_doubleClicked(const QModelIndex &index);

private:
    Ui::KevaBookmarksDialog *ui;
    WalletModel *model;
    QModelIndex selectedIndex;
};

#endif // BITCOIN_QT_KEVABOOKMARKSDIALOG_H
