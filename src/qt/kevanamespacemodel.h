// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_KEVANAMESPACEMODEL_H
#define BITCOIN_QT_KEVANAMESPACEMODEL_H

#include <qt/walletmodel.h>

#include <QAbstractTableModel>
#include <QStringList>
#include <QDateTime>

class CWallet;

class NamespaceEntry
{
public:
    NamespaceEntry():confirmed(true) { }

    std::string id;
    std::string name;
    bool confirmed;
};

class NamespaceEntryLessThan
{
public:
    NamespaceEntryLessThan(int nColumn, Qt::SortOrder fOrder):
        column(nColumn), order(fOrder) {}
    bool operator()(NamespaceEntry &left, NamespaceEntry &right) const;

private:
    int column;
    Qt::SortOrder order;
};

/** Model for list of recently generated payment requests / bitcoin: URIs.
 * Part of wallet model.
 */
class KevaNamespaceModel: public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit KevaNamespaceModel(CWallet *wallet, WalletModel *parent);
    ~KevaNamespaceModel();

    enum ColumnIndex {
        Id = 0,
        Name = 1,
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

    const NamespaceEntry &entry(int row) const { return list[row]; }
    void setNamespace(std::vector<NamespaceEntry> vNamespaceEntries);

public Q_SLOTS:
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder);

private:
    WalletModel *walletModel;
    QStringList columns;
    QList<NamespaceEntry> list;
    int64_t nReceiveRequestsMaxId;
};

#endif // BITCOIN_QT_KEVANAMESPACEMODEL_H
