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
#include "script/kava.h"
#include "txmempool.h"
#include "util.h"
#include "validation.h"
#include "wallet/coincontrol.h"
#include "wallet/wallet.h"

#include <univalue.h>


/* ************************************************************************** */

UniValue
keva_namespace (const JSONRPCRequest& request)
{
  CWallet* const pwallet = GetWalletForJSONRPCRequest(request);
  if (!EnsureWalletIsAvailable (pwallet, request.fHelp))
    return NullUniValue;

  if (request.fHelp || request.params.size () != 1)
    throw std::runtime_error (
        "keva_namespace \"display_name\"\n"
        "\nStart creation of the given namespace.\n"
        + HelpRequiringPassphrase (pwallet) +
        "\nArguments:\n"
        "1. \"display_name\"          (string, required) the display name of the namespace\n"
        "\nResult:\n"
        "[\n"
        "  xxxxx,   (string) the txid, required for keva_put\n"
        "]\n"
        "\nExamples:\n"
        + HelpExampleCli ("keva_namespace", "\"display name\"")
      );

  RPCTypeCheck (request.params, {UniValue::VSTR});

  ObserveSafeMode ();

  const std::string displayNameStr = request.params[0].get_str ();
  const valtype displayName = ValtypeFromString (displayNameStr);
  if (displayName.size () > MAX_NAME_LENGTH)
    throw JSONRPCError (RPC_INVALID_PARAMETER, "the name is too long");

  /* No explicit locking should be necessary.  CReserveKey takes care
     of locking the wallet, and CommitTransaction (called when sending
     the tx) locks cs_main as necessary.  */

  EnsureWalletIsUnlocked (pwallet);

  CReserveKey keyName(pwallet);
  CPubKey pubKey;
  const bool ok = keyName.GetReservedKey(pubKey, true);
  assert (ok);

  CKeyID keyId = pubKey.GetID()

  // The namespace is: Hash160(Hash160(keyId) || displayName)
  valtype toHash = ToByteVector(Hash160(ToByteVector(keyId)));
  toHash.insert(toHash.end(), displayName.begin(), displayName.end());
  const uint160 namespaceHash = Hash160(toHash);

  const CScript addrName = GetScriptForDestination(keyId);
  const CScript newScript = CKevaScript::buildKevaNamespace(addrName, namespaceHash, displayName);

  CCoinControl coinControl;
  CWalletTx wtx;
  SendMoneyToScript(pwallet, newScript, nullptr,
                     NAME_LOCKED_AMOUNT, false, wtx, coinControl);

  keyName.KeepKey();

  const std::string randStr = HexStr(rand);
  const std::string txid = wtx.GetHash().GetHex();
  LogPrintf ("name_new: name=%s, rand=%s, tx=%s\n",
             nameStr.c_str(), randStr.c_str(), txid.c_str());

  UniValue res(UniValue::VARR);
  res.push_back(txid);
  res.push_back(randStr);

  return res;
}

/* ************************************************************************** */

UniValue keva_namespace(const JSONRPCRequest& request)
{
  CWallet* const pwallet = GetWalletForJSONRPCRequest(request);
  if (!EnsureWalletIsAvailable(pwallet, request.fHelp))
    return NullUniValue;

  if (request.fHelp
      || (request.params.size() != 3))
    throw std::runtime_error (
        "keva_namespace \"display_name\" (\"keva_namespace\")\n"
        "\nCreate a namespace.\n"
        + HelpRequiringPassphrase(pwallet) +
        "\nArguments:\n"
        "1. \"display_name\"            (string, required) the display name of the namespace\n"
        "\nResult:\n"
        "\"txid\"                       (string) the keva_namespace's txid\n"
        "\"internal_id\"                (string) the internal id of the namespace\n"
        "\nExamples:\n"
        + HelpExampleCli("keva_namespace", "\"display_name\"")
      );

  RPCTypeCheck (request.params,
    {UniValue::VSTR});

  ObserveSafeMode ();

  const std::string displayName = request.params[0].get_str();
  const valtype displayNameVal = ValtypeFromString(displayName);
  if (displayNameVal.size() > MAX_NAMESPACE_LENGTH)
    throw JSONRPCError (RPC_INVALID_PARAMETER, "the display name of the namespace is too long");

  /* Reject updates to a name for which the mempool already has
     a pending update.  This is not a hard rule enforced by network
     rules, but it is necessary with the current mempool implementation.  */
  {
    LOCK (mempool.cs);
    if (mempool.updatesName(name))
      throw JSONRPCError (RPC_TRANSACTION_ERROR,
                          "there is already a pending update for this name");
  }

  /* No more locking required, similarly to name_new.  */
  EnsureWalletIsUnlocked (pwallet);

  CReserveKey keyName(pwallet);
  CPubKey pubKeyReserve;
  const bool ok = keyName.GetReservedKey(pubKeyReserve, true);
  assert (ok);
  bool usedKey = false;

  CScript addrName;
  if (request.params.size () == 3)
    {
      keyName.ReturnKey ();
      const CTxDestination dest
        = DecodeDestination (request.params[2].get_str ());
      if (!IsValidDestination (dest))
        throw JSONRPCError (RPC_INVALID_ADDRESS_OR_KEY, "invalid address");

      addrName = GetScriptForDestination (dest);
    }
  else
    {
      usedKey = true;
      addrName = GetScriptForDestination (pubKeyReserve.GetID ());
    }

  const CScript nameScript
    = CNameScript::buildNameUpdate (addrName, name, value);

  CCoinControl coinControl;
  CWalletTx wtx;
  SendMoneyToScript (pwallet, nameScript, &txIn,
                     NAME_LOCKED_AMOUNT, false, wtx, coinControl);

  if (usedKey)
    keyName.KeepKey ();

  return wtx.GetHash ().GetHex ();
}


UniValue keva_put(const JSONRPCRequest& request)
{
  CWallet* const pwallet = GetWalletForJSONRPCRequest(request);
  if (!EnsureWalletIsAvailable (pwallet, request.fHelp))
    return NullUniValue;

  if (request.fHelp
      || (request.params.size () != 3))
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

  RPCTypeCheck (request.params,
    {UniValue::VSTR, UniValue::VSTR, UniValue::VSTR, UniValue::VSTR});

  ObserveSafeMode ();

  const std::string namespaceStr = request.params[0].get_str ();
  const valtype namespaceVal = ValtypeFromString (namespaceStr);
  if (namespaceVal.size () > MAX_NAMESPACE_LENGTH)
    throw JSONRPCError (RPC_INVALID_PARAMETER, "the namespace is too long");

  const std::string keyStr = request.params[1].get_str ();
  const valtype key = ValtypeFromString (keyStr);
  if (key.size () > MAX_KEY_LENGTH)
    throw JSONRPCError (RPC_INVALID_PARAMETER, "the key is too long");

  const std::string valueStr = request.params[2].get_str ();
  const valtype value = ValtypeFromString (valueStr);
  if (value.size () > MAX_VALUE_LENGTH)
    throw JSONRPCError (RPC_INVALID_PARAMETER, "the value is too long");

  bool createNamespace = false;
  if (!request.params[3].isNull()) {
    createNamespace = request.params[3].get_bool();
  }

  /* Reject updates to a name for which the mempool already has
     a pending update.  This is not a hard rule enforced by network
     rules, but it is necessary with the current mempool implementation.  */
  {
    LOCK (mempool.cs);
    if (mempool.updatesName (name))
      throw JSONRPCError (RPC_TRANSACTION_ERROR,
                          "there is already a pending update for this name");
  }

  CNameData oldData;
  {
    LOCK (cs_main);
    if (!pcoinsTip->GetName (name, oldData) || oldData.isExpired ())
      throw JSONRPCError (RPC_TRANSACTION_ERROR,
                          "this name can not be updated");
  }

  const COutPoint outp = oldData.getUpdateOutpoint ();
  const CTxIn txIn(outp);

  /* No more locking required, similarly to name_new.  */

  EnsureWalletIsUnlocked (pwallet);

  CReserveKey keyName(pwallet);
  CPubKey pubKeyReserve;
  const bool ok = keyName.GetReservedKey (pubKeyReserve, true);
  assert (ok);
  bool usedKey = false;

  CScript addrName;
  if (request.params.size () == 3)
    {
      keyName.ReturnKey ();
      const CTxDestination dest
        = DecodeDestination (request.params[2].get_str ());
      if (!IsValidDestination (dest))
        throw JSONRPCError (RPC_INVALID_ADDRESS_OR_KEY, "invalid address");

      addrName = GetScriptForDestination (dest);
    }
  else
    {
      usedKey = true;
      addrName = GetScriptForDestination (pubKeyReserve.GetID ());
    }

  const CScript nameScript
    = CNameScript::buildNameUpdate (addrName, name, value);

  CCoinControl coinControl;
  CWalletTx wtx;
  SendMoneyToScript (pwallet, nameScript, &txIn,
                     NAME_LOCKED_AMOUNT, false, wtx, coinControl);

  if (usedKey)
    keyName.KeepKey ();

  return wtx.GetHash ().GetHex ();
}
