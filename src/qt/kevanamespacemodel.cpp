// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/kevanamespacemodel.h>

#include <qt/bitcoinunits.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>

#include <clientversion.h>
#include <streams.h>


KevaNamespaceModel::KevaNamespaceModel(CWallet *wallet, WalletModel *parent) :
    QAbstractTableModel(parent), walletModel(parent)
{
    Q_UNUSED(wallet)

    /* These columns must match the indices in the ColumnIndex enumeration */
    columns << tr("Id") << tr("Name");
}

KevaNamespaceModel::~KevaNamespaceModel()
{
    /* Intentionally left empty */
}

int KevaNamespaceModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);

    return list.length();
}

int KevaNamespaceModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);

    return columns.length();
}

QVariant KevaNamespaceModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid() || index.row() >= list.length())
        return QVariant();

    if (role == Qt::TextColorRole)
    {
        const NamespaceEntry *rec = &list[index.row()];
        if (!rec->confirmed) {
            return QVariant(QBrush (QColor(Qt::gray)));
        }
        return QVariant();
    }
    else if(role == Qt::DisplayRole || role == Qt::EditRole)
    {
        const NamespaceEntry *rec = &list[index.row()];
        switch(index.column())
        {
        case Id:
            return QString::fromStdString(rec->id);
        case Name:
            return QString::fromStdString(rec->name);
        }
    }
    else if (role == Qt::TextAlignmentRole)
    {
        return (int)(Qt::AlignLeft|Qt::AlignVCenter);
    }
    return QVariant();
}

bool KevaNamespaceModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    return true;
}

QVariant KevaNamespaceModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation == Qt::Horizontal)
    {
        if(role == Qt::DisplayRole && section < columns.size())
        {
            return columns[section];
        }
    }
    return QVariant();
}


QModelIndex KevaNamespaceModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(parent);

    return createIndex(row, column);
}

bool KevaNamespaceModel::removeRows(int row, int count, const QModelIndex &parent)
{
    Q_UNUSED(parent);

    if(count > 0 && row >= 0 && (row+count) <= list.size())
    {
        beginRemoveRows(parent, row, row + count - 1);
        list.erase(list.begin() + row, list.begin() + row + count);
        endRemoveRows();
        return true;
    } else {
        return false;
    }
}

Qt::ItemFlags KevaNamespaceModel::flags(const QModelIndex &index) const
{
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
}

// actually add to table in GUI
void KevaNamespaceModel::setNamespace(std::vector<NamespaceEntry> vNamespaceEntries)
{
    // Remove the old ones.
    removeRows(0, list.size());
    list.clear();

    for (auto it = vNamespaceEntries.begin(); it != vNamespaceEntries.end(); it++) {
        beginInsertRows(QModelIndex(), 0, 0);
        list.prepend(*it);
        endInsertRows();
    }
}

void KevaNamespaceModel::sort(int column, Qt::SortOrder order)
{
    qSort(list.begin(), list.end(), NamespaceEntryLessThan(column, order));
    Q_EMIT dataChanged(index(0, 0, QModelIndex()), index(list.size() - 1, NUMBER_OF_COLUMNS - 1, QModelIndex()));
}


bool NamespaceEntryLessThan::operator()(NamespaceEntry &left, NamespaceEntry &right) const
{
    NamespaceEntry *pLeft = &left;
    NamespaceEntry *pRight = &right;
    if (order == Qt::DescendingOrder)
        std::swap(pLeft, pRight);

    switch(column)
    {
    case KevaNamespaceModel::Id:
        return pLeft->id < pRight->id;
    case KevaNamespaceModel::Name:
        return pLeft->name < pRight->name;
    default:
        return pLeft->id < pRight->id;
    }
}
