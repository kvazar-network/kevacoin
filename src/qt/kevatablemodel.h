// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_KEVATABLEMODEL_H
#define BITCOIN_QT_KEVATABLEMODEL_H

#include <qt/walletmodel.h>

#include <QAbstractTableModel>
#include <QStringList>
#include <QDateTime>

class CWallet;

class KevaEntry
{
public:
    KevaEntry() { }

    std::string key;
    std::string value;
    int64_t block;
    QDateTime date;
};

class KevaEntryLessThan
{
public:
    KevaEntryLessThan(int nColumn, Qt::SortOrder fOrder):
        column(nColumn), order(fOrder) {}
    bool operator()(KevaEntry &left, KevaEntry &right) const;

private:
    int column;
    Qt::SortOrder order;
};

/** Model for list of recently generated payment requests / bitcoin: URIs.
 * Part of wallet model.
 */
class KevaTableModel: public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit KevaTableModel(CWallet *wallet, WalletModel *parent);
    ~KevaTableModel();

    enum ColumnIndex {
        Date = 0,
        Key = 1,
        Value = 2,
        Block = 3,
        NUMBER_OF_COLUMNS
    };

    /** @name Methods overridden from QAbstractTableModel
        @{*/
    int rowCount(const QModelIndex &parent) const;
    int columnCount(const QModelIndex &parent) const;
    QVariant data(const QModelIndex &index, int role) const;
    bool setData(const QModelIndex &index, const QVariant &value, int role);
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    QModelIndex index(int row, int column, const QModelIndex &parent) const;
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex());
    Qt::ItemFlags flags(const QModelIndex &index) const;
    /*@}*/

    const KevaEntry &entry(int row) const { return list[row]; }
    void setKeva(std::vector<KevaEntry> vKevaEntries);

public Q_SLOTS:
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder);
    void updateDisplayUnit();

private:
    WalletModel *walletModel;
    QStringList columns;
    QList<KevaEntry> list;
    int64_t nReceiveRequestsMaxId;

    /** Updates the column title to "Amount (DisplayUnit)" and emits headerDataChanged() signal for table headers to react. */
    void updateAmountColumnTitle();
    /** Gets title for amount column including current display unit if optionsModel reference available. */
    QString getAmountTitle();
};

#endif // BITCOIN_QT_KEVATABLEMODEL_H
