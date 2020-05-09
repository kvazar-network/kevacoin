// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/kevabookmarksmodel.h>

#include <qt/bitcoinunits.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>

#include <util.h>

#include <clientversion.h>
#include <streams.h>

#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDir>


KevaBookmarksModel::KevaBookmarksModel(CWallet *wallet, WalletModel *parent) :
    QAbstractTableModel(parent), walletModel(parent)
{
    Q_UNUSED(wallet)

    /* These columns must match the indices in the ColumnIndex enumeration */
    columns << tr("Id") << tr("Name");
}

KevaBookmarksModel::~KevaBookmarksModel()
{
    /* Intentionally left empty */
}

int KevaBookmarksModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);

    return list.length();
}

int KevaBookmarksModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);

    return columns.length();
}

QVariant KevaBookmarksModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid() || index.row() >= list.length())
        return QVariant();

    if (role == Qt::TextColorRole)
    {
        return QVariant();
    }
    else if(role == Qt::DisplayRole || role == Qt::EditRole)
    {
        const BookmarkEntry *rec = &list[index.row()];
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

bool KevaBookmarksModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    return true;
}

QVariant KevaBookmarksModel::headerData(int section, Qt::Orientation orientation, int role) const
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


QModelIndex KevaBookmarksModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(parent);

    return createIndex(row, column);
}

bool KevaBookmarksModel::removeRows(int row, int count, const QModelIndex &parent)
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

Qt::ItemFlags KevaBookmarksModel::flags(const QModelIndex &index) const
{
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
}

// actually add to table in GUI
void KevaBookmarksModel::setBookmarks(std::vector<BookmarkEntry> vBookmarEntries)
{
    // Remove the old ones.
    removeRows(0, list.size());
    list.clear();

    for (auto it = vBookmarEntries.begin(); it != vBookmarEntries.end(); it++) {
        beginInsertRows(QModelIndex(), 0, 0);
        list.prepend(*it);
        endInsertRows();
    }
}

void KevaBookmarksModel::setBookmarks(QJsonArray &array)
{
    std::vector<BookmarkEntry> vBookmarEntries;
    for (int i = 0; i < array.size(); ++i) {
        QJsonObject obj = array[i].toObject();
        BookmarkEntry entry;
        entry.id = obj["id"].toString().toStdString();
        entry.name = obj["name"].toString().toStdString();
        vBookmarEntries.push_back(entry);
    }
    setBookmarks(std::move(vBookmarEntries));
}

bool KevaBookmarksModel::isBookmarked(std::string& ns)
{
    for (int i = 0; i < list.size(); ++i) {
        if (list.at(i).id == ns) {
            return true;
        }
    }
    return false;
}

void KevaBookmarksModel::sort(int column, Qt::SortOrder order)
{
    qSort(list.begin(), list.end(), BookmarkEntryLessThan(column, order));
    Q_EMIT dataChanged(index(0, 0, QModelIndex()), index(list.size() - 1, NUMBER_OF_COLUMNS - 1, QModelIndex()));
}


QString KevaBookmarksModel::getBookmarkFile()
{
    QString dataDir = GUIUtil::boostPathToQString(GetDataDir());
    return dataDir + QDir::separator() + QStringLiteral(BOOKMARK_FILE);
}

int KevaBookmarksModel::loadBookmarks()
{
    QJsonArray json;
    if (loadBookmarks(json)) {
        setBookmarks(json);
        return 1;
    }

    // No bookmark file. Save and load the default ones.
    QJsonObject entry0;
    entry0["id"] = "NgKBKkBAJMtzsuit85TpTpo5Xj6UQUg1wr";
    entry0["name"] = "Kevacoin Official Blog";

    QJsonObject entry1;
    entry1["id"] = "NV9GkLpCLMh4Nd6nZRkch8iNbuV8w9khTm";
    entry1["name"] = "Kevacoin官方博客";

    QJsonObject entry2;
    entry2["id"] = "NfFPgVqzk3H9afHjX8FDoyhkwtwGNanjyG";
    entry2["name"] = "Официальный блог Kevacoin";

    QJsonArray array;
    array.append(entry0);
    array.append(entry1);
    array.append(entry2);

    if (!saveBookmarks(array)) {
        return 0;
    }

    // Load the bookmarks again.
    if (loadBookmarks(json)) {
        setBookmarks(json);
        return 1;
    }
    return 0;
}


int KevaBookmarksModel::loadBookmarks(QJsonArray &json)
{
    QFile loadFile(getBookmarkFile());
    if (!loadFile.open(QIODevice::ReadOnly)) {
        return 0;
    }
    QByteArray saveData = loadFile.readAll();
    QJsonDocument loadDoc(QJsonDocument::fromBinaryData(saveData));
    json = loadDoc.array();
    return 1;
}

int KevaBookmarksModel::saveBookmarks(QJsonArray &json)
{
    QFile saveFile(getBookmarkFile());
    if (!saveFile.open(QIODevice::WriteOnly)) {
        return 0;
    }

    QJsonDocument saveDoc(json);
    saveFile.write(saveDoc.toBinaryData());
    return 1;
}


bool BookmarkEntryLessThan::operator()(BookmarkEntry &left, BookmarkEntry &right) const
{
    BookmarkEntry *pLeft = &left;
    BookmarkEntry *pRight = &right;
    if (order == Qt::DescendingOrder)
        std::swap(pLeft, pRight);

    switch(column)
    {
    case KevaBookmarksModel::Id:
        return pLeft->id < pRight->id;
    case KevaBookmarksModel::Name:
        return pLeft->name < pRight->name;
    default:
        return pLeft->id < pRight->id;
    }
}
