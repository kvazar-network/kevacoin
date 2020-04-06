// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_KEVADIALOG_H
#define BITCOIN_QT_KEVADIALOG_H

#include <qt/guiutil.h>

#include <QDialog>
#include <QHeaderView>
#include <QItemSelection>
#include <QKeyEvent>
#include <QMenu>
#include <QPoint>
#include <QVariant>

class PlatformStyle;
class WalletModel;

namespace Ui {
    class KevaDialog;
}

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

/** Dialog for requesting payment of bitcoins */
class KevaDialog : public QDialog
{
    Q_OBJECT

public:
    enum ColumnWidths {
        DATE_COLUMN_WIDTH = 130,
        KEY_COLUMN_WIDTH = 120,
        BLOCK_MINIMUM_COLUMN_WIDTH = 100,
        MINIMUM_COLUMN_WIDTH = 100
    };

    explicit KevaDialog(const PlatformStyle *platformStyle, QWidget *parent = 0);
    ~KevaDialog();

    void setModel(WalletModel *model);

public Q_SLOTS:
    void clear();
    void reject();
    void accept();

protected:
    virtual void keyPressEvent(QKeyEvent *event);

private:
    Ui::KevaDialog *ui;
    GUIUtil::TableViewLastColumnResizingFixer *columnResizingFixer;
    WalletModel *model;
    QMenu *contextMenu;
    const PlatformStyle *platformStyle;

    QModelIndex selectedRow();
    void copyColumnToClipboard(int column);
    virtual void resizeEvent(QResizeEvent *event);

private Q_SLOTS:
    void on_showContent_clicked();
    void on_showRequestButton_clicked();
    void on_removeRequestButton_clicked();
    void on_kevaView_doubleClicked(const QModelIndex &index);
    void kevaView_selectionChanged(const QItemSelection &selected, const QItemSelection &deselected);
    void updateDisplayUnit();
    void showMenu(const QPoint &point);
    void copyURI();
    void copyLabel();
    void copyMessage();
    void copyAmount();
};

#endif // BITCOIN_QT_KEVADIALOG_H
