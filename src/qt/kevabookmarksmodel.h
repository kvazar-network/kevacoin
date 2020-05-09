// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_KEVABOOKMARKSMODEL_H
#define BITCOIN_QT_KEVABOOKMARKSMODEL_H

#include <qt/walletmodel.h>

#include <QAbstractTableModel>
#include <QStringList>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>

#define BOOKMARK_FILE "bookmarks.dat"

class CWallet;

class BookmarkEntry
{
public:
    BookmarkEntry() { }

    std::string id;
    std::string name;
};

class BookmarkEntryLessThan
{
public:
    BookmarkEntryLessThan(int nColumn, Qt::SortOrder fOrder):
        column(nColumn), order(fOrder) {}
    bool operator()(BookmarkEntry &left, BookmarkEntry &right) const;

private:
    int column;
    Qt::SortOrder order;
};

/** Model for list of recently generated payment requests / bitcoin: URIs.
 * Part of wallet model.
 */
class KevaBookmarksModel: public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit KevaBookmarksModel(CWallet *wallet, WalletModel *parent);
    ~KevaBookmarksModel();

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

    const BookmarkEntry &entry(int row) const { return list[row]; }
    void setBookmarks(std::vector<BookmarkEntry> vBookmarkEntries);
    void setBookmarks(QJsonArray &json);

    int loadBookmarks();
    int loadBookmarks(QJsonArray &json);
    int saveBookmarks(QJsonArray &json);
    bool isBookmarked(std::string& ns);

public Q_SLOTS:
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder);

private:
    WalletModel *walletModel;
    QStringList columns;
    QList<BookmarkEntry> list;
    int64_t nReceiveRequestsMaxId;

    QString getBookmarkFile();
};

#endif // BITCOIN_QT_KEVABOOKMARKSMODEL_H
