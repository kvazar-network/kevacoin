// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/walletmodel.h>

#include <qt/addresstablemodel.h>
#include <consensus/validation.h>
#include <qt/guiconstants.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>
#include <qt/paymentserver.h>
#include <qt/recentrequeststablemodel.h>
#include <qt/kevatablemodel.h>
#include <qt/kevanamespacemodel.h>
#include <qt/kevabookmarksmodel.h>
#include <qt/sendcoinsdialog.h>
#include <qt/transactiontablemodel.h>

#include <base58.h>
#include <chain.h>
#include <univalue.h>
#include <rpc/protocol.h>
#include <keystore.h>
#include <keva/common.h>
#include <keva/main.h>
#include <validation.h>
#include <net.h> // for g_connman
#include <policy/fees.h>
#include <policy/rbf.h>
#include <sync.h>
#include <ui_interface.h>
#include <util.h> // for GetBoolArg
#include <wallet/coincontrol.h>
#include <wallet/feebumper.h>
#include <wallet/wallet.h>
#include <wallet/walletdb.h> // for BackupWallet

#include <stdint.h>

#include <QDebug>
#include <QMessageBox>
#include <QSet>
#include <QTimer>

const int NAMESPACE_LENGTH           =  21;
const std::string DUMMY_NAMESPACE    =  "___DUMMY_NAMESPACE___";

WalletModel::WalletModel(const PlatformStyle *platformStyle, CWallet *_wallet, OptionsModel *_optionsModel, QObject *parent) :
    QObject(parent), wallet(_wallet), optionsModel(_optionsModel), addressTableModel(0),
    transactionTableModel(0),
    recentRequestsTableModel(0),
    kevaTableModel(0),
    kevaNamespaceModel(0),
    kevaBookmarksModel(0),
    cachedBalance(0), cachedUnconfirmedBalance(0), cachedImmatureBalance(0),
    cachedEncryptionStatus(Unencrypted),
    cachedNumBlocks(0)
{
    fHaveWatchOnly = wallet->HaveWatchOnly();
    fForceCheckBalanceChanged = false;

    addressTableModel = new AddressTableModel(wallet, this);
    transactionTableModel = new TransactionTableModel(platformStyle, wallet, this);
    recentRequestsTableModel = new RecentRequestsTableModel(wallet, this);
    kevaTableModel = new KevaTableModel(wallet, this);
    kevaNamespaceModel = new KevaNamespaceModel(wallet, this);
    kevaBookmarksModel = new KevaBookmarksModel(wallet, this);

    // This timer will be fired repeatedly to update the balance
    pollTimer = new QTimer(this);
    connect(pollTimer, SIGNAL(timeout()), this, SLOT(pollBalanceChanged()));
    pollTimer->start(MODEL_UPDATE_DELAY);

    subscribeToCoreSignals();
}

WalletModel::~WalletModel()
{
    unsubscribeFromCoreSignals();
}

CAmount WalletModel::getBalance(const CCoinControl *coinControl) const
{
    if (coinControl)
    {
        return wallet->GetAvailableBalance(coinControl);
    }

    return wallet->GetBalance();
}

CAmount WalletModel::getUnconfirmedBalance() const
{
    return wallet->GetUnconfirmedBalance();
}

CAmount WalletModel::getImmatureBalance() const
{
    return wallet->GetImmatureBalance();
}

bool WalletModel::haveWatchOnly() const
{
    return fHaveWatchOnly;
}

CAmount WalletModel::getWatchBalance() const
{
    return wallet->GetWatchOnlyBalance();
}

CAmount WalletModel::getWatchUnconfirmedBalance() const
{
    return wallet->GetUnconfirmedWatchOnlyBalance();
}

CAmount WalletModel::getWatchImmatureBalance() const
{
    return wallet->GetImmatureWatchOnlyBalance();
}

void WalletModel::updateStatus()
{
    EncryptionStatus newEncryptionStatus = getEncryptionStatus();

    if(cachedEncryptionStatus != newEncryptionStatus)
        Q_EMIT encryptionStatusChanged(newEncryptionStatus);
}

void WalletModel::pollBalanceChanged()
{
    // Get required locks upfront. This avoids the GUI from getting stuck on
    // periodical polls if the core is holding the locks for a longer time -
    // for example, during a wallet rescan.
    TRY_LOCK(cs_main, lockMain);
    if(!lockMain)
        return;
    TRY_LOCK(wallet->cs_wallet, lockWallet);
    if(!lockWallet)
        return;

    if(fForceCheckBalanceChanged || chainActive.Height() != cachedNumBlocks)
    {
        fForceCheckBalanceChanged = false;

        // Balance and number of transactions might have changed
        cachedNumBlocks = chainActive.Height();

        checkBalanceChanged();
        if(transactionTableModel)
            transactionTableModel->updateConfirmations();
    }
}

void WalletModel::checkBalanceChanged()
{
    CAmount newBalance = getBalance();
    CAmount newUnconfirmedBalance = getUnconfirmedBalance();
    CAmount newImmatureBalance = getImmatureBalance();
    CAmount newWatchOnlyBalance = 0;
    CAmount newWatchUnconfBalance = 0;
    CAmount newWatchImmatureBalance = 0;
    if (haveWatchOnly())
    {
        newWatchOnlyBalance = getWatchBalance();
        newWatchUnconfBalance = getWatchUnconfirmedBalance();
        newWatchImmatureBalance = getWatchImmatureBalance();
    }

    if(cachedBalance != newBalance || cachedUnconfirmedBalance != newUnconfirmedBalance || cachedImmatureBalance != newImmatureBalance ||
        cachedWatchOnlyBalance != newWatchOnlyBalance || cachedWatchUnconfBalance != newWatchUnconfBalance || cachedWatchImmatureBalance != newWatchImmatureBalance)
    {
        cachedBalance = newBalance;
        cachedUnconfirmedBalance = newUnconfirmedBalance;
        cachedImmatureBalance = newImmatureBalance;
        cachedWatchOnlyBalance = newWatchOnlyBalance;
        cachedWatchUnconfBalance = newWatchUnconfBalance;
        cachedWatchImmatureBalance = newWatchImmatureBalance;
        Q_EMIT balanceChanged(newBalance, newUnconfirmedBalance, newImmatureBalance,
                            newWatchOnlyBalance, newWatchUnconfBalance, newWatchImmatureBalance);
    }
}

void WalletModel::updateTransaction()
{
    // Balance and number of transactions might have changed
    fForceCheckBalanceChanged = true;
}

void WalletModel::updateAddressBook(const QString &address, const QString &label,
        bool isMine, const QString &purpose, int status)
{
    if(addressTableModel)
        addressTableModel->updateEntry(address, label, isMine, purpose, status);
}

void WalletModel::updateWatchOnlyFlag(bool fHaveWatchonly)
{
    fHaveWatchOnly = fHaveWatchonly;
    Q_EMIT notifyWatchonlyChanged(fHaveWatchonly);
}

bool WalletModel::validateAddress(const QString &address)
{
    return IsValidDestinationString(address.toStdString());
}

WalletModel::SendCoinsReturn WalletModel::prepareTransaction(WalletModelTransaction &transaction, const CCoinControl& coinControl)
{
    CAmount total = 0;
    bool fSubtractFeeFromAmount = false;
    QList<SendCoinsRecipient> recipients = transaction.getRecipients();
    std::vector<CRecipient> vecSend;

    if(recipients.empty())
    {
        return OK;
    }

    QSet<QString> setAddress; // Used to detect duplicates
    int nAddresses = 0;

    // Pre-check input data for validity
    for (const SendCoinsRecipient &rcp : recipients)
    {
        if (rcp.fSubtractFeeFromAmount)
            fSubtractFeeFromAmount = true;

        if (rcp.paymentRequest.IsInitialized())
        {   // PaymentRequest...
            CAmount subtotal = 0;
            const payments::PaymentDetails& details = rcp.paymentRequest.getDetails();
            for (int i = 0; i < details.outputs_size(); i++)
            {
                const payments::Output& out = details.outputs(i);
                if (out.amount() <= 0) continue;
                subtotal += out.amount();
                const unsigned char* scriptStr = (const unsigned char*)out.script().data();
                CScript scriptPubKey(scriptStr, scriptStr+out.script().size());
                CAmount nAmount = out.amount();
                CRecipient recipient = {scriptPubKey, nAmount, rcp.fSubtractFeeFromAmount};
                vecSend.push_back(recipient);
            }
            if (subtotal <= 0)
            {
                return InvalidAmount;
            }
            total += subtotal;
        }
        else
        {   // User-entered bitcoin address / amount:
            if(!validateAddress(rcp.address))
            {
                return InvalidAddress;
            }
            if(rcp.amount <= 0)
            {
                return InvalidAmount;
            }
            setAddress.insert(rcp.address);
            ++nAddresses;

            CScript scriptPubKey = GetScriptForDestination(DecodeDestination(rcp.address.toStdString()));
            CRecipient recipient = {scriptPubKey, rcp.amount, rcp.fSubtractFeeFromAmount};
            vecSend.push_back(recipient);

            total += rcp.amount;
        }
    }
    if(setAddress.size() != nAddresses)
    {
        return DuplicateAddress;
    }

    CAmount nBalance = getBalance(&coinControl);

    if(total > nBalance)
    {
        return AmountExceedsBalance;
    }

    {
        LOCK2(cs_main, wallet->cs_wallet);

        transaction.newPossibleKeyChange(wallet);

        CAmount nFeeRequired = 0;
        int nChangePosRet = -1;
        std::string strFailReason;

        CWalletTx *newTx = transaction.getTransaction();
        CReserveKey *keyChange = transaction.getPossibleKeyChange();
        std::vector<unsigned char> empty;
        bool fCreated = wallet->CreateTransaction(vecSend, nullptr, empty, *newTx, *keyChange, nFeeRequired, nChangePosRet, strFailReason, coinControl);
        transaction.setTransactionFee(nFeeRequired);
        if (fSubtractFeeFromAmount && fCreated)
            transaction.reassignAmounts(nChangePosRet);

        if(!fCreated)
        {
            if(!fSubtractFeeFromAmount && (total + nFeeRequired) > nBalance)
            {
                return SendCoinsReturn(AmountWithFeeExceedsBalance);
            }
            Q_EMIT message(tr("Send Coins"), QString::fromStdString(strFailReason),
                         CClientUIInterface::MSG_ERROR);
            return TransactionCreationFailed;
        }

        // reject absurdly high fee. (This can never happen because the
        // wallet caps the fee at maxTxFee. This merely serves as a
        // belt-and-suspenders check)
        if (nFeeRequired > maxTxFee)
            return AbsurdFee;
    }

    return SendCoinsReturn(OK);
}

WalletModel::SendCoinsReturn WalletModel::sendCoins(WalletModelTransaction &transaction)
{
    QByteArray transaction_array; /* store serialized transaction */

    {
        LOCK2(cs_main, wallet->cs_wallet);
        CWalletTx *newTx = transaction.getTransaction();

        for (const SendCoinsRecipient &rcp : transaction.getRecipients())
        {
            if (rcp.paymentRequest.IsInitialized())
            {
                // Make sure any payment requests involved are still valid.
                if (PaymentServer::verifyExpired(rcp.paymentRequest.getDetails())) {
                    return PaymentRequestExpired;
                }

                // Store PaymentRequests in wtx.vOrderForm in wallet.
                std::string key("PaymentRequest");
                std::string value;
                rcp.paymentRequest.SerializeToString(&value);
                newTx->vOrderForm.push_back(make_pair(key, value));
            }
            else if (!rcp.message.isEmpty()) // Message from normal bitcoin:URI (bitcoin:123...?message=example)
                newTx->vOrderForm.push_back(make_pair("Message", rcp.message.toStdString()));
        }

        CReserveKey *keyChange = transaction.getPossibleKeyChange();
        CValidationState state;
        if(!wallet->CommitTransaction(*newTx, *keyChange, g_connman.get(), state))
            return SendCoinsReturn(TransactionCommitFailed, QString::fromStdString(state.GetRejectReason()));

        CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
        ssTx << *newTx->tx;
        transaction_array.append(&(ssTx[0]), ssTx.size());
    }

    // Add addresses / update labels that we've sent to the address book,
    // and emit coinsSent signal for each recipient
    for (const SendCoinsRecipient &rcp : transaction.getRecipients())
    {
        // Don't touch the address book when we have a payment request
        if (!rcp.paymentRequest.IsInitialized())
        {
            std::string strAddress = rcp.address.toStdString();
            CTxDestination dest = DecodeDestination(strAddress);
            std::string strLabel = rcp.label.toStdString();
            {
                LOCK(wallet->cs_wallet);

                std::map<CTxDestination, CAddressBookData>::iterator mi = wallet->mapAddressBook.find(dest);

                // Check if we have a new address or an updated label
                if (mi == wallet->mapAddressBook.end())
                {
                    wallet->SetAddressBook(dest, strLabel, "send");
                }
                else if (mi->second.name != strLabel)
                {
                    wallet->SetAddressBook(dest, strLabel, ""); // "" means don't change purpose
                }
            }
        }
        Q_EMIT coinsSent(wallet, rcp, transaction_array);
    }
    checkBalanceChanged(); // update balance immediately, otherwise there could be a short noticeable delay until pollBalanceChanged hits

    return SendCoinsReturn(OK);
}

OptionsModel *WalletModel::getOptionsModel()
{
    return optionsModel;
}

AddressTableModel *WalletModel::getAddressTableModel()
{
    return addressTableModel;
}

TransactionTableModel *WalletModel::getTransactionTableModel()
{
    return transactionTableModel;
}

RecentRequestsTableModel *WalletModel::getRecentRequestsTableModel()
{
    return recentRequestsTableModel;
}

KevaTableModel *WalletModel::getKevaTableModel()
{
    return kevaTableModel;
}

KevaNamespaceModel *WalletModel::getKevaNamespaceModel()
{
    return kevaNamespaceModel;
}

KevaBookmarksModel *WalletModel::getKevaBookmarksModel()
{
    return kevaBookmarksModel;
}

WalletModel::EncryptionStatus WalletModel::getEncryptionStatus() const
{
    if(!wallet->IsCrypted())
    {
        return Unencrypted;
    }
    else if(wallet->IsLocked())
    {
        return Locked;
    }
    else
    {
        return Unlocked;
    }
}

bool WalletModel::setWalletEncrypted(bool encrypted, const SecureString &passphrase)
{
    if(encrypted)
    {
        // Encrypt
        return wallet->EncryptWallet(passphrase);
    }
    else
    {
        // Decrypt -- TODO; not supported yet
        return false;
    }
}

bool WalletModel::setWalletLocked(bool locked, const SecureString &passPhrase)
{
    if(locked)
    {
        // Lock
        return wallet->Lock();
    }
    else
    {
        // Unlock
        return wallet->Unlock(passPhrase);
    }
}

bool WalletModel::changePassphrase(const SecureString &oldPass, const SecureString &newPass)
{
    bool retval;
    {
        LOCK(wallet->cs_wallet);
        wallet->Lock(); // Make sure wallet is locked before attempting pass change
        retval = wallet->ChangeWalletPassphrase(oldPass, newPass);
    }
    return retval;
}

bool WalletModel::backupWallet(const QString &filename)
{
    return wallet->BackupWallet(filename.toLocal8Bit().data());
}

// Handlers for core signals
static void NotifyKeyStoreStatusChanged(WalletModel *walletmodel, CCryptoKeyStore *wallet)
{
    qDebug() << "NotifyKeyStoreStatusChanged";
    QMetaObject::invokeMethod(walletmodel, "updateStatus", Qt::QueuedConnection);
}

static void NotifyAddressBookChanged(WalletModel *walletmodel, CWallet *wallet,
        const CTxDestination &address, const std::string &label, bool isMine,
        const std::string &purpose, ChangeType status)
{
    QString strAddress = QString::fromStdString(EncodeDestination(address));
    QString strLabel = QString::fromStdString(label);
    QString strPurpose = QString::fromStdString(purpose);

    qDebug() << "NotifyAddressBookChanged: " + strAddress + " " + strLabel + " isMine=" + QString::number(isMine) + " purpose=" + strPurpose + " status=" + QString::number(status);
    QMetaObject::invokeMethod(walletmodel, "updateAddressBook", Qt::QueuedConnection,
                              Q_ARG(QString, strAddress),
                              Q_ARG(QString, strLabel),
                              Q_ARG(bool, isMine),
                              Q_ARG(QString, strPurpose),
                              Q_ARG(int, status));
}

static void NotifyTransactionChanged(WalletModel *walletmodel, CWallet *wallet, const uint256 &hash, ChangeType status)
{
    Q_UNUSED(wallet);
    Q_UNUSED(hash);
    Q_UNUSED(status);
    QMetaObject::invokeMethod(walletmodel, "updateTransaction", Qt::QueuedConnection);
}

static void ShowProgress(WalletModel *walletmodel, const std::string &title, int nProgress)
{
    // emits signal "showProgress"
    QMetaObject::invokeMethod(walletmodel, "showProgress", Qt::QueuedConnection,
                              Q_ARG(QString, QString::fromStdString(title)),
                              Q_ARG(int, nProgress));
}

static void NotifyWatchonlyChanged(WalletModel *walletmodel, bool fHaveWatchonly)
{
    QMetaObject::invokeMethod(walletmodel, "updateWatchOnlyFlag", Qt::QueuedConnection,
                              Q_ARG(bool, fHaveWatchonly));
}

void WalletModel::subscribeToCoreSignals()
{
    // Connect signals to wallet
    wallet->NotifyStatusChanged.connect(boost::bind(&NotifyKeyStoreStatusChanged, this, boost::placeholders::_1));
    wallet->NotifyAddressBookChanged.connect(boost::bind(NotifyAddressBookChanged, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3, boost::placeholders::_4, boost::placeholders::_5, boost::placeholders::_6));
    wallet->NotifyTransactionChanged.connect(boost::bind(NotifyTransactionChanged, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3));
    wallet->ShowProgress.connect(boost::bind(ShowProgress, this, boost::placeholders::_1, boost::placeholders::_2));
    wallet->NotifyWatchonlyChanged.connect(boost::bind(NotifyWatchonlyChanged, this, boost::placeholders::_1));
}

void WalletModel::unsubscribeFromCoreSignals()
{
    // Disconnect signals from wallet
    wallet->NotifyStatusChanged.disconnect(boost::bind(&NotifyKeyStoreStatusChanged, this, boost::placeholders::_1));
    wallet->NotifyAddressBookChanged.disconnect(boost::bind(NotifyAddressBookChanged, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3, boost::placeholders::_4, boost::placeholders::_5, boost::placeholders::_6));
    wallet->NotifyTransactionChanged.disconnect(boost::bind(NotifyTransactionChanged, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3));
    wallet->ShowProgress.disconnect(boost::bind(ShowProgress, this, boost::placeholders::_1, boost::placeholders::_2));
    wallet->NotifyWatchonlyChanged.disconnect(boost::bind(NotifyWatchonlyChanged, this, boost::placeholders::_1));
}

// WalletModel::UnlockContext implementation
WalletModel::UnlockContext WalletModel::requestUnlock()
{
    bool was_locked = getEncryptionStatus() == Locked;
    if(was_locked)
    {
        // Request UI to unlock wallet
        Q_EMIT requireUnlock();
    }
    // If wallet is still locked, unlock was failed or cancelled, mark context as invalid
    bool valid = getEncryptionStatus() != Locked;

    return UnlockContext(this, valid, was_locked);
}

WalletModel::UnlockContext::UnlockContext(WalletModel *_wallet, bool _valid, bool _relock):
        wallet(_wallet),
        valid(_valid),
        relock(_relock)
{
}

WalletModel::UnlockContext::~UnlockContext()
{
    if(valid && relock)
    {
        wallet->setWalletLocked(true);
    }
}

void WalletModel::UnlockContext::CopyFrom(const UnlockContext& rhs)
{
    // Transfer context; old object no longer relocks wallet
    *this = rhs;
    rhs.relock = false;
}

bool WalletModel::getPubKey(const CKeyID &address, CPubKey& vchPubKeyOut) const
{
    return wallet->GetPubKey(address, vchPubKeyOut);
}

bool WalletModel::IsSpendable(const CTxDestination& dest) const
{
    return IsMine(*wallet, dest) & ISMINE_SPENDABLE;
}

bool WalletModel::getPrivKey(const CKeyID &address, CKey& vchPrivKeyOut) const
{
    return wallet->GetKey(address, vchPrivKeyOut);
}

// returns a list of COutputs from COutPoints
void WalletModel::getOutputs(const std::vector<COutPoint>& vOutpoints, std::vector<COutput>& vOutputs)
{
    LOCK2(cs_main, wallet->cs_wallet);
    for (const COutPoint& outpoint : vOutpoints)
    {
        auto it = wallet->mapWallet.find(outpoint.hash);
        if (it == wallet->mapWallet.end()) continue;
        int nDepth = it->second.GetDepthInMainChain();
        if (nDepth < 0) continue;
        COutput out(&it->second, outpoint.n, nDepth, true /* spendable */, true /* solvable */, true /* safe */);
        vOutputs.push_back(out);
    }
}

bool WalletModel::isSpent(const COutPoint& outpoint) const
{
    LOCK2(cs_main, wallet->cs_wallet);
    return wallet->IsSpent(outpoint.hash, outpoint.n);
}

// AvailableCoins + LockedCoins grouped by wallet address (put change in one group with wallet address)
void WalletModel::listCoins(std::map<QString, std::vector<COutput> >& mapCoins) const
{
    for (auto& group : wallet->ListCoins()) {
        auto& resultGroup = mapCoins[QString::fromStdString(EncodeDestination(group.first))];
        for (auto& coin : group.second) {
            resultGroup.emplace_back(std::move(coin));
        }
    }
}

bool WalletModel::isLockedCoin(uint256 hash, unsigned int n) const
{
    LOCK2(cs_main, wallet->cs_wallet);
    return wallet->IsLockedCoin(hash, n);
}

void WalletModel::lockCoin(COutPoint& output)
{
    LOCK2(cs_main, wallet->cs_wallet);
    wallet->LockCoin(output);
}

void WalletModel::unlockCoin(COutPoint& output)
{
    LOCK2(cs_main, wallet->cs_wallet);
    wallet->UnlockCoin(output);
}

void WalletModel::listLockedCoins(std::vector<COutPoint>& vOutpts)
{
    LOCK2(cs_main, wallet->cs_wallet);
    wallet->ListLockedCoins(vOutpts);
}

void WalletModel::loadReceiveRequests(std::vector<std::string>& vReceiveRequests)
{
    vReceiveRequests = wallet->GetDestValues("rr"); // receive request
}

bool WalletModel::saveReceiveRequest(const std::string &sAddress, const int64_t nId, const std::string &sRequest)
{
    CTxDestination dest = DecodeDestination(sAddress);

    std::stringstream ss;
    ss << nId;
    std::string key = "rr" + ss.str(); // "rr" prefix = "receive request" in destdata

    LOCK(wallet->cs_wallet);
    if (sRequest.empty())
        return wallet->EraseDestData(dest, key);
    else
        return wallet->AddDestData(dest, key, sRequest);
}

bool WalletModel::transactionCanBeAbandoned(uint256 hash) const
{
    return wallet->TransactionCanBeAbandoned(hash);
}

bool WalletModel::abandonTransaction(uint256 hash) const
{
    LOCK2(cs_main, wallet->cs_wallet);
    return wallet->AbandonTransaction(hash);
}

bool WalletModel::transactionCanBeBumped(uint256 hash) const
{
    return feebumper::TransactionCanBeBumped(wallet, hash);
}

bool WalletModel::bumpFee(uint256 hash)
{
    CCoinControl coin_control;
    coin_control.signalRbf = true;
    std::vector<std::string> errors;
    CAmount old_fee;
    CAmount new_fee;
    CMutableTransaction mtx;
    if (feebumper::CreateTransaction(wallet, hash, coin_control, 0 /* totalFee */, errors, old_fee, new_fee, mtx) != feebumper::Result::OK) {
        QMessageBox::critical(0, tr("Fee bump error"), tr("Increasing transaction fee failed") + "<br />(" +
            (errors.size() ? QString::fromStdString(errors[0]) : "") +")");
         return false;
    }

    // allow a user based fee verification
    QString questionString = tr("Do you want to increase the fee?");
    questionString.append("<br />");
    questionString.append("<table style=\"text-align: left;\">");
    questionString.append("<tr><td>");
    questionString.append(tr("Current fee:"));
    questionString.append("</td><td>");
    questionString.append(BitcoinUnits::formatHtmlWithUnit(getOptionsModel()->getDisplayUnit(), old_fee));
    questionString.append("</td></tr><tr><td>");
    questionString.append(tr("Increase:"));
    questionString.append("</td><td>");
    questionString.append(BitcoinUnits::formatHtmlWithUnit(getOptionsModel()->getDisplayUnit(), new_fee - old_fee));
    questionString.append("</td></tr><tr><td>");
    questionString.append(tr("New fee:"));
    questionString.append("</td><td>");
    questionString.append(BitcoinUnits::formatHtmlWithUnit(getOptionsModel()->getDisplayUnit(), new_fee));
    questionString.append("</td></tr></table>");
    SendConfirmationDialog confirmationDialog(tr("Confirm fee bump"), questionString);
    confirmationDialog.exec();
    QMessageBox::StandardButton retval = (QMessageBox::StandardButton)confirmationDialog.result();

    // cancel sign&broadcast if users doesn't want to bump the fee
    if (retval != QMessageBox::Yes) {
        return false;
    }

    WalletModel::UnlockContext ctx(requestUnlock());
    if(!ctx.isValid())
    {
        return false;
    }

    // sign bumped transaction
    if (!feebumper::SignTransaction(wallet, mtx)) {
        QMessageBox::critical(0, tr("Fee bump error"), tr("Can't sign transaction."));
        return false;
    }
    // commit the bumped transaction
    uint256 txid;
    if (feebumper::CommitTransaction(wallet, hash, std::move(mtx), errors, txid) != feebumper::Result::OK) {
        QMessageBox::critical(0, tr("Fee bump error"), tr("Could not commit transaction") + "<br />(" +
            QString::fromStdString(errors[0])+")");
         return false;
    }
    return true;
}

bool WalletModel::isWalletEnabled()
{
   return !gArgs.GetBoolArg("-disablewallet", DEFAULT_DISABLE_WALLET);
}

bool WalletModel::hdEnabled() const
{
    return wallet->IsHDEnabled();
}

OutputType WalletModel::getDefaultAddressType() const
{
    return g_address_type;
}

int WalletModel::getDefaultConfirmTarget() const
{
    return nTxConfirmTarget;
}

void WalletModel::getKevaEntries(std::vector<KevaEntry>& vKevaEntries, std::string nameSpace)
{
    valtype nameSpaceVal = ValtypeFromString(nameSpace);

    {
        // Get the unconfirmed namespaces and list them at the beginning.
        LOCK (mempool.cs);

        std::vector<std::tuple<valtype, valtype, valtype, uint256>> unconfirmedKeyValueList;
        mempool.getUnconfirmedKeyValueList(unconfirmedKeyValueList, nameSpaceVal);

        for (auto e : unconfirmedKeyValueList) {
            KevaEntry entry;
            entry.key = ValtypeToString(std::get<1>(e));
            entry.value = ValtypeToString(std::get<2>(e));
            entry.block = -1; // Unconfirmed.
            entry.date = QDateTime::currentDateTime();
            vKevaEntries.push_back(std::move(entry));
        }
    }

    LOCK(cs_main);

    valtype key;
    CKevaData data;
    std::unique_ptr<CKevaIterator> iter(pcoinsTip->IterateKeys(nameSpaceVal));
    while (iter->next(key, data)) {
        KevaEntry entry;
        entry.key = ValtypeToString(key);
        entry.value = ValtypeToString(data.getValue());
        entry.block = data.getHeight();
        entry.transactionID = data.getUpdateOutpoint().hash.ToString();

        CBlockIndex* pblockindex = chainActive[entry.block];
        if (pblockindex) {
            entry.date.setTime_t(pblockindex->nTime);
        }
        vKevaEntries.push_back(std::move(entry));
    }

}

void WalletModel::getNamespaceEntries(std::vector<NamespaceEntry>& vNamespaceEntries)
{
    std::map<std::string, std::string> mapObjects;
    {
        LOCK2(cs_main, wallet->cs_wallet);
        std::set<CTxDestination> destinations;
        std::vector<COutput> vecOutputs;
        bool include_unsafe = true;
        CAmount nMinimumAmount = 0;
        CAmount nMaximumAmount = MAX_MONEY;
        CAmount nMinimumSumAmount = MAX_MONEY;
        uint64_t nMaximumCount = 0;
        int nMinDepth = 1;
        int nMaxDepth = 99999999;
        wallet->AvailableCoins(vecOutputs, !include_unsafe, nullptr, nMinimumAmount, nMaximumAmount, nMinimumSumAmount, nMaximumCount, nMinDepth, nMaxDepth);
        for (const COutput& out : vecOutputs) {
            CTxDestination address;
            const CScript& scriptPubKey = out.tx->tx->vout[out.i].scriptPubKey;
            const CKevaScript kevaOp(scriptPubKey);
            if (!kevaOp.isKevaOp()) {
                continue;
            }
            if (!kevaOp.isNamespaceRegistration() && !kevaOp.isAnyUpdate()) {
                continue;
            }
            const valtype nameSpace = kevaOp.getOpNamespace();
            const std::string nameSpaceStr = EncodeBase58Check(nameSpace);
            CKevaData data;
            if (pcoinsTip->GetNamespace(nameSpace, data)) {
                std::string displayName = ValtypeToString(data.getValue());
                mapObjects[nameSpaceStr] = displayName;
            }
        }
    }
    {
        // Also get the unconfirmed namespaces and list them at the beginning.
        LOCK (mempool.cs);

        std::vector<std::tuple<valtype, valtype, uint256>> unconfirmedNamespaces;
        mempool.getUnconfirmedNamespaceList(unconfirmedNamespaces);

        for (auto ns: unconfirmedNamespaces) {
            NamespaceEntry entry;
            entry.id = EncodeBase58Check(std::get<0>(ns));
            entry.name = ValtypeToString(std::get<1>(ns));
            entry.confirmed = false;
            vNamespaceEntries.push_back(std::move(entry));
        }
    }

    // The confirmed namespaces.
    std::map<std::string, std::string>::iterator it = mapObjects.begin();
	while (it != mapObjects.end()) {
        NamespaceEntry entry;
        entry.id = it->first;
        entry.name = it->second;
        vNamespaceEntries.push_back(std::move(entry));
		it++;
	}
}

int WalletModel::createNamespace(std::string displayNameStr, std::string& namespaceId)
{
    const valtype displayName = ValtypeFromString (displayNameStr);
    if (displayName.size() > MAX_NAMESPACE_LENGTH) {
        return NamespaceTooLong;
    }

    WalletModel::UnlockContext ctx(requestUnlock());
    if(!ctx.isValid()) {
        return WalletLocked;
    }

    CReserveKey keyName(wallet);
    CPubKey pubKey;
    const bool ok = keyName.GetReservedKey(pubKey, true);
    assert(ok);

    CKeyID keyId = pubKey.GetID();

    // The namespace name is: Hash160("first txin")
    // For now we don't know the first txin, so use dummy name here.
    // It will be replaced later in CreateTransaction.
    valtype namespaceDummy = ToByteVector(std::string(DUMMY_NAMESPACE));
    assert(namespaceDummy.size() == NAMESPACE_LENGTH);

    CScript redeemScript = GetScriptForDestination(WitnessV0KeyHash(keyId));
    CScriptID scriptHash = CScriptID(redeemScript);
    CScript addrName = GetScriptForDestination(scriptHash);
    const CScript newScript = CKevaScript::buildKevaNamespace(addrName, namespaceDummy, displayName);

    CCoinControl coinControl;
    CWalletTx wtx;
    valtype kevaNamespace;
    try {
        SendMoneyToScript(wallet, newScript, nullptr, kevaNamespace,
            KEVA_LOCKED_AMOUNT, false, wtx, coinControl);
    } catch (const UniValue& objError) {
        UniValue code = objError["code"];
        if (code.get_int() == RPC_WALLET_INSUFFICIENT_FUNDS) {
            return WalletModel::InsufficientFund;
        }
        return WalletModel::CannotUpdate;
    }

    keyName.KeepKey();
    namespaceId = EncodeBase58Check(kevaNamespace);
    return 0;
}


int WalletModel::deleteKevaEntry(std::string namespaceStr, std::string keyStr)
{
    valtype nameSpace;
    if (!DecodeKevaNamespace(namespaceStr, Params(), nameSpace)) {
        return InvalidNamespace;
    }

    const valtype key = ValtypeFromString(keyStr);
    if (key.size() > MAX_KEY_LENGTH) {
        return KeyTooLong;
    }

    WalletModel::UnlockContext ctx(requestUnlock());
    if(!ctx.isValid()) {
        return WalletLocked;
    }

    bool hasKey = false;
    CKevaData data;
    {
        LOCK2(cs_main, mempool.cs);
        std::vector<std::tuple<valtype, valtype, valtype, uint256>> unconfirmedKeyValueList;
        valtype val;
        if (mempool.getUnconfirmedKeyValue(nameSpace, key, val)) {
            if (val.size() > 0) {
                hasKey = true;
            }
        } else if (pcoinsTip->GetName(nameSpace, key, data)) {
            hasKey = true;
        }
    }

    if (!hasKey) {
        return KeyNotFound;
    }

    COutput output;
    std::string kevaNamespce = namespaceStr;
    if (!wallet->FindKevaCoin(output, kevaNamespce)) {
        // TODO: This namespace can not be updated
        return 0;
    }
    const COutPoint outp(output.tx->GetHash(), output.i);
    const CTxIn txIn(outp);

    CReserveKey keyName(wallet);
    CPubKey pubKeyReserve;
    const bool ok = keyName.GetReservedKey(pubKeyReserve, true);
    assert(ok);

    CScript redeemScript = GetScriptForDestination(WitnessV0KeyHash(pubKeyReserve.GetID()));
    CScriptID scriptHash = CScriptID(redeemScript);
    CScript addrName = GetScriptForDestination(scriptHash);

    const CScript kevaScript = CKevaScript::buildKevaDelete(addrName, nameSpace, key);

    CCoinControl coinControl;
    CWalletTx wtx;
    valtype empty;
    SendMoneyToScript(wallet, kevaScript, &txIn, empty,
        KEVA_LOCKED_AMOUNT, false, wtx, coinControl);

    keyName.KeepKey();
    return 0;
}

int WalletModel::addKeyValue(std::string& namespaceStr, std::string& keyStr, std::string& valueStr)
{
    valtype nameSpace;
    if (!DecodeKevaNamespace(namespaceStr, Params(), nameSpace)) {
        return InvalidNamespace;
    }

    const valtype key = ValtypeFromString(keyStr);
    if (keyStr.size() > MAX_KEY_LENGTH) {
        return KeyTooLong;
    }

    const valtype value = ValtypeFromString(valueStr);
    if (value.size() > MAX_VALUE_LENGTH) {
        return ValueTooLong;
    }

    WalletModel::UnlockContext ctx(requestUnlock());
    if(!ctx.isValid()) {
        return WalletLocked;
    }

    COutput output;
    if (!wallet->FindKevaCoin(output, namespaceStr)) {
        return CannotUpdate;
    }
    const COutPoint outp(output.tx->GetHash(), output.i);
    const CTxIn txIn(outp);

    CReserveKey keyName(wallet);
    CPubKey pubKeyReserve;
    const bool ok = keyName.GetReservedKey(pubKeyReserve, true);
    assert(ok);
    CKeyID keyId = pubKeyReserve.GetID();
    CScript redeemScript = GetScriptForDestination(WitnessV0KeyHash(keyId));
    CScriptID scriptHash = CScriptID(redeemScript);
    CScript addrName = GetScriptForDestination(scriptHash);

    const CScript kevaScript = CKevaScript::buildKevaPut(addrName, nameSpace, key, value);

    CCoinControl coinControl;
    CWalletTx wtx;
    valtype empty;
    SendMoneyToScript(wallet, kevaScript, &txIn, empty,
        KEVA_LOCKED_AMOUNT, false, wtx, coinControl);

    keyName.KeepKey();
    return 0;
}
