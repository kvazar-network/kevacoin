// Copyright (c) 2014-2017 Daniel Kraft
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// Copyright (c) 2018 Jianping Wu
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <keva/main.h>

#include <chainparams.h>
#include <coins.h>
#include <consensus/validation.h>
#include <hash.h>
#include <dbwrapper.h>
#include <script/interpreter.h>
#include <script/keva.h>
#include <txmempool.h>
#include <undo.h>
#include <util.h>
#include <utilstrencodings.h>
#include <validation.h>


/* ************************************************************************** */
/* CKevaTxUndo.  */

void
CKevaTxUndo::fromOldState(const valtype& nameSpace, const valtype& key, const CCoinsView& view)
{
  this->nameSpace = nameSpace;
  this->key = key;
  isNew = !view.GetName(nameSpace, key, oldData);
}

void
CKevaTxUndo::apply(CCoinsViewCache& view) const
{
  if (isNew)
    view.DeleteName(nameSpace, key);
  else
    view.SetName(nameSpace, key, oldData, true);
}

/* ************************************************************************** */
/* CKevaMemPool.  */

uint256
CKevaMemPool::getTxForNamespace(const valtype& nameSpace) const
{
#if 0
  // JWU TODO: implement this!
  NamespaceTxMap::const_iterator mi;

  mi = mapNamespaceRegs.find(nameSpace);
  if (mi != mapNamespaceRegs.end()) {
    assert(mapNamespaceKeyUpdates.count(name) == 0);
    return mi->second;
  }

  mi = mapNamespaceKeyUpdates.find(name);
  if (mi != mapNameUpdates.end ()) {
    assert (mapNameRegs.count (name) == 0);
    return mi->second;
  }
#endif
  return uint256();
}

void
CKevaMemPool::addUnchecked (const uint256& hash, const CTxMemPoolEntry& entry)
{
  AssertLockHeld (pool.cs);

  if (entry.isNamespaceRegistration()) {
    const valtype& nameSpace = entry.getNamespace();
    assert(mapNamespaceRegs.count(nameSpace) == 0);
    mapNamespaceRegs.insert(std::make_pair(nameSpace, hash));
  }

  if (entry.isNamespaceKeyUpdate ()) {
    const valtype& nameSpace = entry.getNamespace();
    const valtype& key = entry.getKey();
    NamespaceKeyTuple tuple(nameSpace, key);
    assert(mapNamespaceKeyUpdates.count(tuple) == 0);
    mapNamespaceKeyUpdates.insert (std::make_pair(tuple, hash));
  }
}

void
CKevaMemPool::remove (const CTxMemPoolEntry& entry)
{
  AssertLockHeld (pool.cs);

  if (entry.isNamespaceRegistration ()) {
    const NamespaceTxMap::iterator mit = mapNamespaceRegs.find (entry.getNamespace());
    assert (mit != mapNamespaceRegs.end());
    mapNamespaceRegs.erase (mit);
  }

  if (entry.isNamespaceKeyUpdate ()) {
    NamespaceKeyTuple tuple(entry.getNamespace(), entry.getKey());
    const NamespaceKeyTxMap::iterator mit = mapNamespaceKeyUpdates.find(tuple);
    assert (mit != mapNamespaceKeyUpdates.end());
    mapNamespaceKeyUpdates.erase (mit);
  }
}

void
CKevaMemPool::removeConflicts (const CTransaction& tx)
{
  AssertLockHeld (pool.cs);

  if (!tx.IsKevacoin ())
    return;

  for (const auto& txout : tx.vout)
    {
      const CKevaScript nameOp(txout.scriptPubKey);
      if (nameOp.isKevaOp () && nameOp.getKevaOp () == OP_KEVA_PUT) {
        const valtype& nameSpace = nameOp.getOpNamespace();
        const NamespaceTxMap::const_iterator mit = mapNamespaceRegs.find(nameSpace);
        if (mit != mapNamespaceRegs.end ()) {
          const CTxMemPool::txiter mit2 = pool.mapTx.find (mit->second);
          assert (mit2 != pool.mapTx.end ());
          pool.removeRecursive (mit2->GetTx (),
                                MemPoolRemovalReason::KEVA_CONFLICT);
        }
      }
    }
}

void
CKevaMemPool::check(const CCoinsView& coins) const
{
  AssertLockHeld (pool.cs);

  const uint256 blockHash = coins.GetBestBlock ();
  int nHeight;
  if (blockHash.IsNull())
    nHeight = 0;
  else
    nHeight = mapBlockIndex.find (blockHash)->second->nHeight;

  std::set<valtype> nameRegs;
  std::set<std::tuple<valtype, valtype>> namespaceKeyUpdates;
  for (const auto& entry : pool.mapTx) {
    const uint256 txHash = entry.GetTx ().GetHash ();
    if (entry.isNamespaceRegistration()) {
      const valtype& nameSpace = entry.getNamespace();

      const NamespaceTxMap::const_iterator mit = mapNamespaceRegs.find(nameSpace);
      assert (mit != mapNamespaceRegs.end());
      assert (mit->second == txHash);

      assert (nameRegs.count(nameSpace) == 0);
      nameRegs.insert(nameSpace);
#if 0
      /* The old name should be expired already.  Note that we use
          nHeight+1 for the check, because that's the height at which
          the mempool tx will actually be mined.  */
      CKevaData data;
      if (coins.GetName(name, data))
        assert (data.isExpired (nHeight + 1));
#endif
    }

    if (entry.isNamespaceKeyUpdate()) {
      const valtype& nameSpace = entry.getNamespace();
      const valtype& key = entry.getKey();
      std::tuple<valtype, valtype> tuple(nameSpace, key);
      const NamespaceKeyTxMap::const_iterator mit = mapNamespaceKeyUpdates.find(tuple);
      assert (mit != mapNamespaceKeyUpdates.end ());
      assert (mit->second == txHash);

      assert (namespaceKeyUpdates.count(tuple) == 0);
      namespaceKeyUpdates.insert(tuple);

#if 0
      /* As above, use nHeight+1 for the expiration check.  */
      CKevaData data;
      if (!coins.GetName(name, data)) {
        assert (false);
      }
      assert (!data.isExpired (nHeight + 1));
#endif
    }
  }

  assert(nameRegs.size() == mapNamespaceRegs.size());
  assert(namespaceKeyUpdates.size() == mapNamespaceKeyUpdates.size());

  /* Check that nameRegs and nameUpdates are disjoint.  They must be since
     a name can only be in either category, depending on whether it exists
     at the moment or not.  */
#if 0
  // Is this neccesary?
  for (const auto& nameSpace : nameRegs) {
    assert(namespaceKeyUpdates.count(name) == 0);
  }
#endif

  for (const auto& namespaceKey : namespaceKeyUpdates) {
    assert(nameRegs.count(std::get<0>(namespaceKey)) == 0);
  }
}

bool
CKevaMemPool::checkTx (const CTransaction& tx) const
{
  AssertLockHeld (pool.cs);

  if (!tx.IsKevacoin ())
    return true;

  /* In principle, multiple name_updates could be performed within the
     mempool at once (building upon each other).  This is disallowed, though,
     since the current mempool implementation does not like it.  (We keep
     track of only a single update tx for each name.)  */

  for (const auto& txout : tx.vout) {
    const CKevaScript nameOp(txout.scriptPubKey);
    if (!nameOp.isKevaOp ())
      continue;

    switch (nameOp.getKevaOp ()) {
      case OP_KEVA_NAMESPACE:
      {
        const valtype& nameSpace = nameOp.getOpNamespace();
        std::map<valtype, uint256>::const_iterator mi;
        mi = mapNamespaceRegs.find(nameSpace);
        if (mi != mapNamespaceRegs.end ())
          return false;
        break;
      }

      case OP_KEVA_PUT:
      {
        const valtype& nameSpace = nameOp.getOpNamespace();
        const valtype& key = nameOp.getOpKey();
        if (updatesKey(nameSpace, key))
          return false;
        break;
      }

      default:
        assert (false);
    }
  }

  return true;
}

/* ************************************************************************** */
/* CNameConflictTracker.  */

namespace
{

void
ConflictTrackerNotifyEntryRemoved (CNameConflictTracker* tracker,
                                   CTransactionRef txRemoved,
                                   MemPoolRemovalReason reason)
{
  if (reason == MemPoolRemovalReason::KEVA_CONFLICT)
    tracker->AddConflictedEntry (txRemoved);
}

} // anonymous namespace

CNameConflictTracker::CNameConflictTracker (CTxMemPool &p)
  : txNameConflicts(std::make_shared<std::vector<CTransactionRef>>()), pool(p)
{
  pool.NotifyEntryRemoved.connect (
    boost::bind (&ConflictTrackerNotifyEntryRemoved, this, _1, _2));
}

CNameConflictTracker::~CNameConflictTracker ()
{
  pool.NotifyEntryRemoved.disconnect (
    boost::bind (&ConflictTrackerNotifyEntryRemoved, this, _1, _2));
}

void
CNameConflictTracker::AddConflictedEntry (CTransactionRef txRemoved)
{
  txNameConflicts->emplace_back (std::move (txRemoved));
}

/* ************************************************************************** */

bool
CheckKevaTransaction (const CTransaction& tx, unsigned nHeight,
                      const CCoinsView& view,
                      CValidationState& state, unsigned flags)
{
  const std::string strTxid = tx.GetHash ().GetHex ();
  const char* txid = strTxid.c_str ();
  const bool fMempool = (flags & SCRIPT_VERIFY_KEVA_MEMPOOL);

#if 0
  /* Ignore historic bugs.  */
  CChainParams::BugType type;
  if (Params ().IsHistoricBug (tx.GetHash (), nHeight, type))
    return true;
#endif

  /* As a first step, try to locate inputs and outputs of the transaction
     that are keva scripts.  At most one input and output should be
     a keva operation.  */

  int nameIn = -1;
  CKevaScript nameOpIn;
  Coin coinIn;
  for (unsigned i = 0; i < tx.vin.size (); ++i) {
    const COutPoint& prevout = tx.vin[i].prevout;
    Coin coin;
    if (!view.GetCoin (prevout, coin)) {
      return error ("%s: failed to fetch input coin for %s", __func__, txid);
    }

    const CKevaScript op(coin.out.scriptPubKey);
    if (op.isKevaOp()) {
      if (nameIn != -1) {
        return state.Invalid (error ("%s: multiple name inputs into transaction %s", __func__, txid));
      }
      nameIn = i;
      nameOpIn = op;
      coinIn = coin;
    }
  }

  int nameOut = -1;
  CKevaScript nameOpOut;
  for (unsigned i = 0; i < tx.vout.size (); ++i) {
    const CKevaScript op(tx.vout[i].scriptPubKey);
    if (op.isKevaOp()) {
      if (nameOut != -1) {
        return state.Invalid (error ("%s: multiple name outputs from"
                                      " transaction %s", __func__, txid));
      }
      nameOut = i;
      nameOpOut = op;
    }
  }

  /* Check that no name inputs/outputs are present for a non-Namecoin tx.
     If that's the case, all is fine.  For a Namecoin tx instead, there
     should be at least an output (for NAME_NEW, no inputs are expected).  */

  if (!tx.IsKevacoin ()) {
    if (nameIn != -1)
      return state.Invalid (error ("%s: non-Kevacoin tx %s has keva inputs",
                                    __func__, txid));
    if (nameOut != -1)
      return state.Invalid (error ("%s: non-Kevacoin tx %s at height %u"
                                    " has keva outputs",
                                    __func__, txid, nHeight));

    return true;
  }

  assert(tx.IsKevacoin ());
  if (nameOut == -1)
    return state.Invalid (error ("%s: Kevacoin tx %s has no keva outputs",
                                 __func__, txid));

  /* Reject "greedy names".  */
#if 0
  const Consensus::Params& params = Params ().GetConsensus ();
  if (tx.vout[nameOut].nValue < params.rules->MinNameCoinAmount(nHeight)) {
    return state.Invalid (error ("%s: greedy name", __func__));
  }
#else
  if (tx.vout[nameOut].nValue < KEVA_LOCKED_AMOUNT) {
    return state.Invalid (error ("%s: greedy name", __func__));
  }
#endif

#if 0
  /* Handle NAME_NEW now, since this is easy and different from the other
     operations.  */

  if (nameOpOut.getNameOp () == OP_NAME_NEW)
    {
      if (nameIn != -1)
        return state.Invalid (error ("CheckKevaTransaction: NAME_NEW with"
                                     " previous name input"));

      if (nameOpOut.getOpHash ().size () != 20)
        return state.Invalid (error ("CheckKevaTransaction: NAME_NEW's hash"
                                     " has wrong size"));

      return true;
    }

  /* Now that we have ruled out NAME_NEW, check that we have a previous
     name input that is being updated.  */
#endif

  if (nameOpOut.isNamespaceRegistration()) {
    if (nameOpOut.getOpNamespaceDisplayName().size () > MAX_VALUE_LENGTH) {
      return state.Invalid (error ("CheckKevaTransaction: display name value too long"));
    }
    return true;
  }

  assert (nameOpOut.isAnyUpdate());
  if (nameIn == -1) {
    return state.Invalid(error("CheckKevaTransaction: update without previous keva input"));
  }

  const valtype& key = nameOpOut.getOpKey();
  if (key.size() > MAX_KEY_LENGTH) {
    return state.Invalid (error ("CheckKevaTransaction: key too long"));
  }

  if (nameOpOut.getOpValue().size () > MAX_VALUE_LENGTH) {
    return state.Invalid (error ("CheckKevaTransaction: value too long"));
  }

  /* Process KEVA_PUT next.  */
  const valtype& nameSpace = nameOpOut.getOpNamespace();
  if (nameOpOut.getKevaOp() == OP_KEVA_PUT) {
    if (!nameOpIn.isAnyUpdate() && !nameOpIn.isNamespaceRegistration()) {
      return state.Invalid(error("CheckKevaTransaction: KEVA_PUT with prev input that is no update"));
    }

    if (nameSpace != nameOpIn.getOpNamespace()) {
      return state.Invalid (error ("%s: KEVA_PUT namespace mismatch to prev tx"
                                          " found in %s", __func__, txid));
    }

    CKevaData oldName;
    const valtype& namespaceIn = nameOpIn.getOpNamespace();
    if (!view.GetNamespace(namespaceIn, oldName)) {
        return state.Invalid (error ("%s: Namespace not found", __func__));
    }
    assert (tx.vin[nameIn].prevout == oldName.getUpdateOutpoint());

#if 0
    /* This is actually redundant, since expired names are removed
        from the UTXO set and thus not available to be spent anyway.
        But it does not hurt to enforce this here, too.  It is also
        exercised by the unit tests.  */
    CKevaData oldName;
    const valtype& namespaceIn = nameOpIn.getOpNamespace();
    const valtype& keyIn = nameOpIn.getOpKey();
    if (!view.GetName(namespaceIn, keyIn, oldName)) {
      return state.Invalid (error ("%s: KEVA_PUT name does not exist",
                                          __func__));
    }
    if (oldName.isExpired (nHeight))
      return state.Invalid (error ("%s: trying to update expired name",
                                    __func__));
    /* This is an internal consistency check.  If everything is fine,
        the input coins from the UTXO database should match the
        name database.  */
    assert (static_cast<unsigned> (coinIn.nHeight) == oldName.getHeight());
    assert (tx.vin[nameIn].prevout == oldName.getUpdateOutpoint());
#endif
    return true;
  }

#if 0
  /* Finally, NAME_FIRSTUPDATE.  */

  assert(nameOpOut.getNameOp () == OP_NAME_FIRSTUPDATE);
  if (nameOpIn.getNameOp () != OP_NAME_NEW)
    return state.Invalid (error ("CheckKevaTransaction: NAME_FIRSTUPDATE"
                                 " with non-NAME_NEW prev tx"));

  /* Maturity of NAME_NEW is checked only if we're not adding
     to the mempool.  */
  if (!fMempool)
    {
      assert (static_cast<unsigned> (coinIn.nHeight) != MEMPOOL_HEIGHT);
      if (coinIn.nHeight + MIN_FIRSTUPDATE_DEPTH > nHeight)
        return state.Invalid (error ("CheckKevaTransaction: NAME_NEW"
                                     " is not mature for FIRST_UPDATE"));
    }

  if (nameOpOut.getOpRand ().size () > 20)
    return state.Invalid (error ("CheckKevaTransaction: NAME_FIRSTUPDATE"
                                 " rand too large, %d bytes",
                                 nameOpOut.getOpRand ().size ()));

  {
    valtype toHash(nameOpOut.getOpRand ());
    toHash.insert (toHash.end (), name.begin (), name.end ());
    const uint160 hash = Hash160 (toHash);
    if (hash != uint160 (nameOpIn.getOpHash ()))
      return state.Invalid (error ("CheckKevaTransaction: NAME_FIRSTUPDATE"
                                   " hash mismatch"));
  }

  CNameData oldName;
  if (view.GetName (name, oldName) && !oldName.isExpired (nHeight))
    return state.Invalid (error ("CheckKevaTransaction: NAME_FIRSTUPDATE"
                                 " on an unexpired name"));

  /* We don't have to specifically check that miners don't create blocks with
     conflicting NAME_FIRSTUPDATE's, since the mining's CCoinsViewCache
     takes care of this with the check above already.  */
#endif
  return true;
}

void ApplyNameTransaction(const CTransaction& tx, unsigned nHeight,
                        CCoinsViewCache& view, CBlockUndo& undo)
{
  assert (nHeight != MEMPOOL_HEIGHT);
  if (!tx.IsKevacoin ())
    return;

  /* Changes are encoded in the outputs.  We don't have to do any checks,
     so simply apply all these.  */

  for (unsigned i = 0; i < tx.vout.size(); ++i) {
    const CKevaScript op(tx.vout[i].scriptPubKey);
    if (!op.isKevaOp()) {
      continue;
    }

    if (op.isNamespaceRegistration()) {
      const valtype& nameSpace = op.getOpNamespace();
      const valtype& displayName = op.getOpNamespaceDisplayName();
      LogPrint (BCLog::KEVA, "Register name at height %d: %s, display name: %s\n",
                nHeight, ValtypeToString(nameSpace).c_str(),
                ValtypeToString(displayName).c_str());

      const valtype& key = ValtypeFromString(CKevaScript::KEVA_DISPLAY_NAME_KEY);
      CKevaTxUndo opUndo;
      opUndo.fromOldState(nameSpace, key, view);
      undo.vkevaundo.push_back(opUndo);

      CKevaData data;
      data.fromScript(nHeight, COutPoint(tx.GetHash(), i), op);
      view.SetName(nameSpace, key, data, false);
    } else if (op.isAnyUpdate()) {
      const valtype& nameSpace = op.getOpNamespace();
      const valtype& key = op.getOpKey();
      LogPrint (BCLog::KEVA, "Updating name at height %d: %s\n",
                nHeight, ValtypeToString (nameSpace).c_str ());

      CKevaTxUndo opUndo;
      opUndo.fromOldState(nameSpace, key, view);
      undo.vkevaundo.push_back(opUndo);

      CKevaData data;
      data.fromScript(nHeight, COutPoint(tx.GetHash(), i), op);
      view.SetName(nameSpace, key, data, false);
    }
  }
}

#if 0
bool
ExpireNames (unsigned nHeight, CCoinsViewCache& view, CBlockUndo& undo,
             std::set<valtype>& names)
{
  names.clear ();

  /* The genesis block contains no name expirations.  */
  if (nHeight == 0)
    return true;

  /* Otherwise, find out at which update heights names have expired
     since the last block.  If the expiration depth changes, this could
     be multiple heights at once.  */

  const Consensus::Params& params = Params ().GetConsensus ();
  const unsigned expDepthOld = params.rules->NameExpirationDepth (nHeight - 1);
  const unsigned expDepthNow = params.rules->NameExpirationDepth (nHeight);

  if (expDepthNow > nHeight)
    return true;

  /* Both are inclusive!  The last expireTo was nHeight - 1 - expDepthOld,
     now we start at this value + 1.  */
  const unsigned expireFrom = nHeight - expDepthOld;
  const unsigned expireTo = nHeight - expDepthNow;

  /* It is possible that expireFrom = expireTo + 1, in case that the
     expiration period is raised together with the block height.  In this
     case, no names expire in the current step.  This case means that
     the absolute expiration height "n - expirationDepth(n)" is
     flat -- which is fine.  */
  assert (expireFrom <= expireTo + 1);

  /* Find all names that expire at those depths.  Note that GetNamesForHeight
     clears the output set, to we union all sets here.  */
  for (unsigned h = expireFrom; h <= expireTo; ++h)
    {
      std::set<valtype> newNames;
      view.GetNamesForHeight (h, newNames);
      names.insert (newNames.begin (), newNames.end ());
    }

  /* Expire all those names.  */
  for (std::set<valtype>::const_iterator i = names.begin ();
       i != names.end (); ++i)
    {
      const std::string nameStr = ValtypeToString (*i);

      CNameData data;
      if (!view.GetName (*i, data))
        return error ("%s : name '%s' not found in the database",
                      __func__, nameStr.c_str ());
      if (!data.isExpired (nHeight))
        return error ("%s : name '%s' is not actually expired",
                      __func__, nameStr.c_str ());

      /* Special rule:  When d/postmortem expires (the name used by
         libcoin in the name-stealing demonstration), it's coin
         is already spent.  Ignore.  */
      if (nHeight == 175868 && nameStr == "d/postmortem")
        continue;

      const COutPoint& out = data.getUpdateOutpoint ();
      Coin coin;
      if (!view.GetCoin(out, coin))
        return error ("%s : name coin for '%s' is not available",
                      __func__, nameStr.c_str ());
      const CNameScript nameOp(coin.out.scriptPubKey);
      if (!nameOp.isNameOp () || !nameOp.isAnyUpdate ()
          || nameOp.getOpName () != *i)
        return error ("%s : name coin to be expired is wrong script", __func__);

      if (!view.SpendCoin (out, &coin))
        return error ("%s : spending name coin failed", __func__);
      undo.vexpired.push_back (coin);
    }

  return true;
}

bool
UnexpireNames (unsigned nHeight, CBlockUndo& undo, CCoinsViewCache& view,
               std::set<valtype>& names)
{
  names.clear ();

  /* The genesis block contains no name expirations.  */
  if (nHeight == 0)
    return true;

  std::vector<Coin>::reverse_iterator i;
  for (i = undo.vexpired.rbegin (); i != undo.vexpired.rend (); ++i)
    {
      const CNameScript nameOp(i->out.scriptPubKey);
      if (!nameOp.isNameOp () || !nameOp.isAnyUpdate ())
        return error ("%s : wrong script to be unexpired", __func__);

      const valtype& name = nameOp.getOpName ();
      if (names.count (name) > 0)
        return error ("%s : name '%s' unexpired twice",
                      __func__, ValtypeToString (name).c_str ());
      names.insert (name);

      CNameData data;
      if (!view.GetName (nameOp.getOpName (), data))
        return error ("%s : no data for name '%s' to be unexpired",
                      __func__, ValtypeToString (name).c_str ());
      if (!data.isExpired (nHeight) || data.isExpired (nHeight - 1))
        return error ("%s : name '%s' to be unexpired is not expired in the DB"
                      " or it was already expired before the current height",
                      __func__, ValtypeToString (name).c_str ());

      if (ApplyTxInUndo (std::move(*i), view,
                         data.getUpdateOutpoint ()) != DISCONNECT_OK)
        return error ("%s : failed to undo name coin spending", __func__);
    }

  return true;
}
#endif

void
CheckNameDB (bool disconnect)
{
  const int option
    = gArgs.GetArg ("-checknamedb", Params().DefaultCheckNameDB ());

  if (option == -1)
    return;

  assert (option >= 0);
  if (option != 0)
    {
      if (disconnect || chainActive.Height () % option != 0)
        return;
    }

  pcoinsTip->Flush ();
  const bool ok = pcoinsTip->ValidateNameDB();

  /* The DB is inconsistent (mismatch between UTXO set and names DB) between
     (roughly) blocks 139,000 and 180,000.  This is caused by libcoin's
     "name stealing" bug.  For instance, d/postmortem is removed from
     the UTXO set shortly after registration (when it is used to steal
     names), but it remains in the name DB until it expires.  */
  if (!ok)
    {
      const unsigned nHeight = chainActive.Height ();
      LogPrintf ("ERROR: %s : name database is inconsistent\n", __func__);
      if (nHeight >= 139000 && nHeight <= 180000)
        LogPrintf ("This is expected due to 'name stealing'.\n");
      else
        assert (false);
    }
}
