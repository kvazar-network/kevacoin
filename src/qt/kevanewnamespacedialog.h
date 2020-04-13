// Copyright (c) 2011-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_KEVANEWNMAESPACEDIALOG_H
#define BITCOIN_QT_KEVANEWNMAESPACEDIALOG_H

#include <QObject>
#include <QString>

#include <QDialog>

namespace Ui {
    class KevaNewNamespaceDialog;
}


/** Dialog showing namepsace creation. */
class KevaNewNamespaceDialog : public QDialog
{
    Q_OBJECT

public:
    explicit KevaNewNamespaceDialog(QWidget *parent = 0);
    ~KevaNewNamespaceDialog();

public Q_SLOTS:
    void create();
    void close();
    void onNamespaceChanged(const QString & ns);

private:
    Ui::KevaNewNamespaceDialog *ui;
};

#endif // BITCOIN_QT_KEVANEWNMAESPACEDIALOG_H
