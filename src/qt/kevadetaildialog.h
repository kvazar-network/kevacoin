// Copyright (c) 2011-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_KEVADETAILDIALOG_H
#define BITCOIN_QT_KEVADETAILDIALOG_H

#include <QObject>
#include <QString>

#include <QDialog>

namespace Ui {
    class KevaDetailDialog;
}

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

/** Dialog showing transaction details. */
class KevaDetailDialog : public QDialog
{
    Q_OBJECT

public:
    explicit KevaDetailDialog(const QModelIndex &idx, QWidget *parent, const QString &nameSpace);
    ~KevaDetailDialog();

public Q_SLOTS:
    void onValueChanged();
    void onSave();

private:
    Ui::KevaDetailDialog *ui;
    QString nameSpace;
    QString key;
};

#endif // BITCOIN_QT_KEVADETAILDIALOG_H
