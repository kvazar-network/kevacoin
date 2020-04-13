// Copyright (c) 2011-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_KEVAADDKEYDIALOG_H
#define BITCOIN_QT_KEVAADDKEYDIALOG_H

#include <QObject>
#include <QString>

#include <QDialog>

namespace Ui {
    class KevaAddKeyDialog;
}

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

/** Dialog add new key-value pair. */
class KevaAddKeyDialog : public QDialog
{
    Q_OBJECT

public:
    explicit KevaAddKeyDialog(QWidget *parent, QString &nameSpace);
    ~KevaAddKeyDialog();

private:
    Ui::KevaAddKeyDialog *ui;
    QString nameSpace;

public Q_SLOTS:
    void create();
    void cancel();
    void onKeyChanged(const QString& key);
    void onValueChanged();
};

#endif // BITCOIN_QT_KEVAADDKEYDIALOG_H
