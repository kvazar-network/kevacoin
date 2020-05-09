// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/kevatablemodel.h>

#include <qt/bitcoinunits.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>

#include <clientversion.h>
#include <streams.h>


KevaTableModel::KevaTableModel(CWallet *wallet, WalletModel *parent) :
    QAbstractTableModel(parent), walletModel(parent)
{
    Q_UNUSED(wallet)

    /* These columns must match the indices in the ColumnIndex enumeration */
    columns << tr("Date") << tr("Key") << tr("Value") << tr("Block");

    // TODO: display new keva entry when it arrives.
    // connect(walletModel->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));
}

KevaTableModel::~KevaTableModel()
{
    /* Intentionally left empty */
}

int KevaTableModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);

    return list.length();
}

int KevaTableModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);

    return columns.length();
}

QVariant KevaTableModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid() || index.row() >= list.length())
        return QVariant();

    if (role == Qt::TextColorRole)
    {
        const KevaEntry *rec = &list[index.row()];
        if (rec->block < 0) {
            return QVariant(QBrush (QColor(Qt::gray)));
        }
        return QVariant();
    }
    else if(role == Qt::DisplayRole || role == Qt::EditRole)
    {
        const KevaEntry *rec = &list[index.row()];
        switch(index.column())
        {
        case Date:
            return GUIUtil::dateTimeStr(rec->date);
        case Key:
            return QString::fromStdString(rec->key);
        case Value:
            return QString::fromStdString(rec->value);
        case Block:
            return QString::number(rec->block);
        }
    }
    else if (role == Qt::TextAlignmentRole)
    {
        if (index.column() == Block)
            return (int)(Qt::AlignRight|Qt::AlignVCenter);
        else if (index.column() == Value) {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 12, 0))
            return (int)(Qt::AlignLeft|Qt::AlignTop);
#endif
        }
    }
    return QVariant();
}

bool KevaTableModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    return true;
}

QVariant KevaTableModel::headerData(int section, Qt::Orientation orientation, int role) const
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

/** Gets title for amount column including current display unit if optionsModel reference available. */
QString KevaTableModel::getAmountTitle()
{
    return (this->walletModel->getOptionsModel() != nullptr) ? tr("Requested") + " ("+BitcoinUnits::shortName(this->walletModel->getOptionsModel()->getDisplayUnit()) + ")" : "";
}

QModelIndex KevaTableModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(parent);

    return createIndex(row, column);
}

bool KevaTableModel::removeRows(int row, int count, const QModelIndex &parent)
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

Qt::ItemFlags KevaTableModel::flags(const QModelIndex &index) const
{
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
}


// actually add to table in GUI
void KevaTableModel::setKeva(std::vector<KevaEntry> vKevaEntries)
{
    // Remove the old ones.
    removeRows(0, list.size());
    list.clear();

    for (auto it = vKevaEntries.begin(); it != vKevaEntries.end(); it++) {
        beginInsertRows(QModelIndex(), 0, 0);
        list.prepend(*it);
        endInsertRows();
    }
}

void KevaTableModel::sort(int column, Qt::SortOrder order)
{
    qSort(list.begin(), list.end(), KevaEntryLessThan(column, order));
    Q_EMIT dataChanged(index(0, 0, QModelIndex()), index(list.size() - 1, NUMBER_OF_COLUMNS - 1, QModelIndex()));
}

void KevaTableModel::updateDisplayUnit()
{
}

bool KevaEntryLessThan::operator()(KevaEntry &left, KevaEntry &right) const
{
    KevaEntry *pLeft = &left;
    KevaEntry *pRight = &right;
    if (order == Qt::DescendingOrder)
        std::swap(pLeft, pRight);

    switch(column)
    {
    case KevaTableModel::Date:
        return pLeft->date.toTime_t() < pRight->date.toTime_t();
    case KevaTableModel::Block:
        return pLeft->block < pRight->block;
    case KevaTableModel::Key:
        return pLeft->key < pRight->key;
    case KevaTableModel::Value:
        return pLeft->value < pRight->value;
    default:
        return pLeft->block < pRight->block;
    }
}
