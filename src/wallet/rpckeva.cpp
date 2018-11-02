// Copyright (c) 2018 Jianping Wu
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "base58.h"
#include "coins.h"
#include "init.h"
#include "keva/common.h"
#include "keva/main.h"
#include "primitives/transaction.h"
#include "random.h"
#include "rpc/mining.h"
#include "rpc/safemode.h"
#include "rpc/server.h"
#include "script/keva.h"
#include "txmempool.h"
#include "util.h"
#include "validation.h"
#include "wallet/coincontrol.h"
#include "wallet/wallet.h"

#include <univalue.h>


/* ************************************************************************** */

UniValue keva_namespace(const JSONRPCRequest& request)
{
  CWallet* const pwallet = GetWalletForJSONRPCRequest(request);
  if (!EnsureWalletIsAvailable (pwallet, request.fHelp))
    return NullUniValue;

  if (request.fHelp || request.params.size () != 1)
    throw std::runtime_error (
        "keva_namespace \"display_name\"\n"
        "\nRegister a namespace with the given display name.\n"
        + HelpRequiringPassphrase (pwallet) +
        "\nArguments:\n"
        "1. \"display_name\"          (string, required) the display name of the namespace\n"
        "\nResult:\n"
        "[\n"
        "  xxxxx,   (string) the txid, required for keva_put\n"
        "  xxxxx,   (string) the unique namespace id, required for keva_put\n"
        "]\n"
        "\nExamples:\n"
        + HelpExampleCli ("keva_namespace", "\"display name\"")
      );

  RPCTypeCheck (request.params, {UniValue::VSTR});

  ObserveSafeMode ();

  const std::string displayNameStr = request.params[0].get_str();
  const valtype displayName = ValtypeFromString (displayNameStr);
  if (displayName.size () > MAX_NAMESPACE_LENGTH) {
    throw JSONRPCError (RPC_INVALID_PARAMETER, "the name is too long");
  }

  /* No explicit locking should be necessary.  CReserveKey takes care
     of locking the wallet, and CommitTransaction (called when sending
     the tx) locks cs_main as necessary.  */

  EnsureWalletIsUnlocked (pwallet);

  CReserveKey keyName(pwallet);
  CPubKey pubKey;
  const bool ok = keyName.GetReservedKey(pubKey, true);
  assert (ok);

  CKeyID keyId = pubKey.GetID();

  // The namespace name is: Hash160(Hash160(keyId) || displayName)
  valtype toHash = ToByteVector(Hash160(ToByteVector(keyId)));
  toHash.insert(toHash.end(), displayName.begin(), displayName.end());
  const uint160 namespaceHashVal = Hash160(toHash);
  const std::string namespaceHash = EncodeBase64(namespaceHashVal.begin(), namespaceHashVal.size());

  const CScript addrName = GetScriptForDestination(keyId);
  const CScript newScript = CKevaScript::buildKevaNamespace(addrName, ValtypeFromString(namespaceHash), displayName);

  CCoinControl coinControl;
  CWalletTx wtx;
  SendMoneyToScript(pwallet, newScript, nullptr,
                     KEVA_LOCKED_AMOUNT, false, wtx, coinControl);

  keyName.KeepKey();

  const std::string txid = wtx.GetHash().GetHex();
  LogPrintf("keva_namespace: namespace=%s, displayName=%s, tx=%s\n",
             namespaceHash.c_str(), displayNameStr.c_str(), txid.c_str());

  UniValue res(UniValue::VARR);
  res.push_back(txid);
  res.push_back(namespaceHash);

  return res;
}

UniValue keva_list_namespaces(const JSONRPCRequest& request)
{
  CWallet* const pwallet = GetWalletForJSONRPCRequest(request);
  if (!EnsureWalletIsAvailable (pwallet, request.fHelp))
    return NullUniValue;

  if (request.fHelp)
    throw std::runtime_error (
        "keva_list_namespace\n"
        "\nList all namespaces.\n"
        + HelpRequiringPassphrase(pwallet) +
        "\nArguments:\n"
        "\nResult:\n"
        "[\n"
        "  xxxxx: display_name   (string) namespace id : (string) display name\n"
        "  ...\n"
        "]\n"
        "\nExamples:\n"
        + HelpExampleCli("keva_list_namespace", "")
      );

  RPCTypeCheck (request.params, {UniValue::VSTR});

  ObserveSafeMode ();

  std::map<valtype, std::string> mapObjects;
  {
  LOCK2 (cs_main, pwallet->cs_wallet);
  for (const auto& item : pwallet->mapWallet)
    {
      const CWalletTx& tx = item.second;
      if (!tx.tx->IsKevacoin ())
        continue;

      CKevaScript kevaOp;
      int nOut = -1;
      for (unsigned i = 0; i < tx.tx->vout.size (); ++i)
        {
          const CKevaScript cur(tx.tx->vout[i].scriptPubKey);
          if (cur.isKevaOp ())
            {
              if (nOut != -1) {
                LogPrintf ("ERROR: wallet contains tx with multiple name outputs");
              } else {
                kevaOp = cur;
                nOut = i;
              }
            }
        }

      if (nOut == -1) {
        continue;
      }

      if (!kevaOp.isNamespaceRegistration() && !kevaOp.isAnyUpdate()) {
        continue;
      }

      const valtype& nameSpace = kevaOp.getOpNamespace();
      const CBlockIndex* pindex;
      const int depth = tx.GetDepthInMainChain(pindex);
      if (depth <= 0) {
        continue;
      }

      const bool mine = IsMine(*pwallet, kevaOp.getAddress ());
      CKevaData data;
      if (mine && pcoinsTip->GetNamespace(nameSpace, data)) {
        std::string displayName = ValtypeToString(data.getValue());
        mapObjects[nameSpace] = displayName;
      }
    }
  }

  UniValue res(UniValue::VARR);
  for (const auto& item : mapObjects) {
    res.push_back(ValtypeToString(item.first) + " : " + item.second);
  }

  return res;
}

UniValue keva_put(const JSONRPCRequest& request)
{
  CWallet* const pwallet = GetWalletForJSONRPCRequest(request);
  if (!EnsureWalletIsAvailable (pwallet, request.fHelp)) {
    return NullUniValue;
  }

  if (request.fHelp || request.params.size() != 3) {
    throw std::runtime_error (
        "keva_put \"namespace\" \"key\" \"value\" (\"create_namespace\")\n"
        "\nUpdate a name and possibly transfer it.\n"
        + HelpRequiringPassphrase (pwallet) +
        "\nArguments:\n"
        "1. \"namespace\"            (string, required) the namespace to insert the key to\n"
        "2. \"key\"                  (string, required) value for the key\n"
        "3. \"value\"                (string, required) value for the name\n"
        "\nResult:\n"
        "\"txid\"             (string) the keva_put's txid\n"
        "\nExamples:\n"
        + HelpExampleCli ("keva_put", "\"mynamespace\", \"new-key\", \"new-value\"")
      );
  }

  RPCTypeCheck(request.params, {UniValue::VSTR, UniValue::VSTR, UniValue::VSTR});
  RPCTypeCheckArgument(request.params[0], UniValue::VSTR);
  RPCTypeCheckArgument(request.params[1], UniValue::VSTR);
  RPCTypeCheckArgument(request.params[2], UniValue::VSTR);

  ObserveSafeMode ();

  const std::string namespaceStr = request.params[0].get_str ();
  const valtype nameSpace = ValtypeFromString (namespaceStr);
  if (nameSpace.size () > MAX_NAMESPACE_LENGTH)
    throw JSONRPCError (RPC_INVALID_PARAMETER, "the namespace is too long");

  const std::string keyStr = request.params[1].get_str ();
  const valtype key = ValtypeFromString (keyStr);
  if (key.size () > MAX_KEY_LENGTH)
    throw JSONRPCError (RPC_INVALID_PARAMETER, "the key is too long");

  const std::string valueStr = request.params[2].get_str ();
  const valtype value = ValtypeFromString (valueStr);
  if (value.size () > MAX_VALUE_LENGTH)
    throw JSONRPCError (RPC_INVALID_PARAMETER, "the value is too long");

  /* Reject updates to a name for which the mempool already has
     a pending update.  This is not a hard rule enforced by network
     rules, but it is necessary with the current mempool implementation.  */
  {
    LOCK (mempool.cs);
    if (mempool.updatesKey(nameSpace, key)) {
      throw JSONRPCError (RPC_TRANSACTION_ERROR,
                          "there is already a pending update for this name");
    }
  }

  CKevaData oldData;
  {
    LOCK (cs_main);
    if (!pcoinsTip->GetNamespace(nameSpace, oldData)) {
      throw JSONRPCError (RPC_TRANSACTION_ERROR,
                                "this name can not be updated");
    }
  }

  const COutPoint outp = oldData.getUpdateOutpoint();
  const CTxIn txIn(outp);

  /* No more locking required, similarly to name_new.  */

  EnsureWalletIsUnlocked(pwallet);

  CReserveKey keyName(pwallet);
  CPubKey pubKeyReserve;
  const bool ok = keyName.GetReservedKey(pubKeyReserve, true);
  assert(ok);
  bool usedKey = false;

  CScript addrName;
  usedKey = true;
  addrName = GetScriptForDestination(pubKeyReserve.GetID());

  const CScript kevaScript = CKevaScript::buildKevaPut(addrName, nameSpace, key, value);

  CCoinControl coinControl;
  CWalletTx wtx;
  SendMoneyToScript(pwallet, kevaScript, &txIn,
                     KEVA_LOCKED_AMOUNT, false, wtx, coinControl);

  if (usedKey) {
    keyName.KeepKey ();
  }

  return wtx.GetHash().GetHex();
}

UniValue keva_get(const JSONRPCRequest& request)
{
  CWallet* const pwallet = GetWalletForJSONRPCRequest(request);
  if (!EnsureWalletIsAvailable (pwallet, request.fHelp)) {
    return NullUniValue;
  }

  if (request.fHelp || request.params.size() != 2) {
    throw std::runtime_error (
        "keva_get \"namespace\" \"key\"\n"
        "\nGet value of the given key.\n"
        + HelpRequiringPassphrase (pwallet) +
        "\nArguments:\n"
        "1. \"namespace\"            (string, required) the namespace to insert the key to\n"
        "2. \"key\"                  (string, required) value for the key\n"
        "\nResult:\n"
        "\"value\"             (string) the value associated with the key\n"
        "\nExamples:\n"
        + HelpExampleCli ("keva_get", "\"namespace_id\", \"key\"")
      );
  }

  RPCTypeCheck(request.params, {UniValue::VSTR, UniValue::VSTR});
  RPCTypeCheckArgument(request.params[0], UniValue::VSTR);
  RPCTypeCheckArgument(request.params[1], UniValue::VSTR);

  ObserveSafeMode ();

  const std::string namespaceStr = request.params[0].get_str ();
  const valtype nameSpace = ValtypeFromString (namespaceStr);
  if (nameSpace.size () > MAX_NAMESPACE_LENGTH)
    throw JSONRPCError (RPC_INVALID_PARAMETER, "the namespace is too long");

  const std::string keyStr = request.params[1].get_str ();
  const valtype key = ValtypeFromString (keyStr);
  if (key.size () > MAX_KEY_LENGTH)
    throw JSONRPCError (RPC_INVALID_PARAMETER, "the key is too long");

  CKevaData data;
  {
    LOCK (cs_main);
    if (!pcoinsTip->GetName(nameSpace, key, data)) {
      throw JSONRPCError (RPC_TRANSACTION_ERROR, "key not found");
    }
  }

  return ValtypeToString(data.getValue());
}
