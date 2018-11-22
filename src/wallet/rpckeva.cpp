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

const int NAMESPACE_LENGTH           =  21;
const std::string DUMMY_NAMESPACE    =  "___DUMMY_NAMESPACE___";


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
  SendMoneyToScript(pwallet, newScript, nullptr, kevaNamespace,
                     KEVA_LOCKED_AMOUNT, false, wtx, coinControl);
  keyName.KeepKey();

  std::string kevaNamespaceBase58 = EncodeBase58Check(kevaNamespace);
  const std::string txid = wtx.GetHash().GetHex();
  LogPrintf("keva_namespace: namespace=%s, displayName=%s, tx=%s\n",
             kevaNamespaceBase58.c_str(), displayNameStr.c_str(), txid.c_str());

  UniValue res(UniValue::VARR);
  res.push_back(txid);
  res.push_back(kevaNamespaceBase58);
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

  std::map<std::string, std::string> mapObjects;
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

      const valtype nameSpace = kevaOp.getOpNamespace();
      const std::string nameSpaceStr = EncodeBase58Check(nameSpace);
      const CBlockIndex* pindex;
      const int depth = tx.GetDepthInMainChain(pindex);
      if (depth <= 0) {
        continue;
      }

      const bool mine = IsMine(*pwallet, kevaOp.getAddress ());
      CKevaData data;
      if (mine && pcoinsTip->GetNamespace(nameSpace, data)) {
        std::string displayName = ValtypeToString(data.getValue());
        mapObjects[nameSpaceStr] = displayName;
      }
    }
  }

  UniValue res(UniValue::VARR);
  for (const auto& item : mapObjects) {
    UniValue obj(UniValue::VOBJ);
    obj.pushKV(item.first, item.second);
    res.push_back(obj);
  }

  {
    LOCK (mempool.cs);
    // Also get the unconfirmed ones from mempool.
    std::vector<std::tuple<valtype, valtype, uint256>> unconfirmedNamespaces;
    mempool.getUnconfirmedNamespaceList(unconfirmedNamespaces);
    for (auto entry : unconfirmedNamespaces) {
      UniValue obj(UniValue::VOBJ);
      obj.pushKV(EncodeBase58Check(std::get<0>(entry)), ValtypeToString(std::get<1>(entry)));
      res.push_back(obj);
    }
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

  const std::string namespaceStr = request.params[0].get_str();
  valtype nameSpace;
  if (!DecodeKevaNamespace(namespaceStr, Params(), nameSpace)) {
    throw JSONRPCError (RPC_INVALID_PARAMETER, "invalid namespace id");
  }
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

  EnsureWalletIsUnlocked(pwallet);

  COutput output;
  std::string kevaNamespce = namespaceStr;
  if (!pwallet->FindKevaCoin(output, kevaNamespce)) {
    throw JSONRPCError (RPC_TRANSACTION_ERROR, "this name can not be updated");
  }
  const COutPoint outp(output.tx->GetHash(), output.i);
  const CTxIn txIn(outp);

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
  valtype empty;
  SendMoneyToScript(pwallet, kevaScript, &txIn, empty,
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
  valtype nameSpace;
  if (!DecodeKevaNamespace(namespaceStr, Params(), nameSpace)) {
    throw JSONRPCError (RPC_INVALID_PARAMETER, "invalid namespace id");
  }
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
      //throw JSONRPCError (RPC_TRANSACTION_ERROR, "key not found");
    }
  }

  auto value = data.getValue();
  // Also get the unconfirmed ones.
  {
    LOCK (mempool.cs);
    valtype val;
    if (mempool.getUnconfirmedKeyValue(nameSpace, key, val)) {
      value = val;
    }
  }

  return ValtypeToString(value);
}

UniValue keva_pending(const JSONRPCRequest& request)
{
  if (request.fHelp || request.params.size () > 1)
    throw std::runtime_error (
        "keva_pending (\"namespace\")\n"
        "\nList unconfirmed keva operations in the mempool.\n"
        "\nIf a namespace is given, only check for operations on this namespace.\n"
        "\nArguments:\n"
        "1. \"namespace\"        (string, optional) only look for this namespace\n"
        "\nResult:\n"
        "[\n"
        "  {\n"
        "    \"op\": xxxx       (string) the operation being performed\n"
        "    \"name\": xxxx     (string) the namespace operated on\n"
        "    \"value\": xxxx    (string) the namespace's new value\n"
        "    \"txid\": xxxx     (string) the txid corresponding to the operation\n"
        "    \"ismine\": xxxx   (boolean) whether the name is owned by the wallet\n"
        "  },\n"
        "  ...\n"
        "]\n"
        + HelpExampleCli ("keva_pending", "")
        + HelpExampleCli ("keva_pending", "\"NJdZVkeerpgxqFM2oAzitVU8xsdj7\"")
        + HelpExampleRpc ("keva_pending", "")
      );

  ObserveSafeMode ();

  std::string namespaceStr;
  valtype nameSpace;

  if (request.params.size() == 1) {
    RPCTypeCheckArgument(request.params[0], UniValue::VSTR);
    namespaceStr = request.params[0].get_str();
    if (!DecodeKevaNamespace(namespaceStr, Params(), nameSpace)) {
      throw JSONRPCError (RPC_INVALID_PARAMETER, "invalid namespace id");
    }
  }

  LOCK (mempool.cs);

  std::vector<std::tuple<valtype, valtype, uint256>> unconfirmedNamespaces;
  mempool.getUnconfirmedNamespaceList(unconfirmedNamespaces);

  std::vector<std::tuple<valtype, valtype, valtype, uint256>> unconfirmedKeyValueList;
  mempool.getUnconfirmedKeyValueList(unconfirmedKeyValueList, nameSpace);

  UniValue arr(UniValue::VARR);
  const std::string opKevaNamepsace = "keva_namespace";
  const std::string opKevaPut       = "keva_put";

  for (auto entry: unconfirmedNamespaces) {
    UniValue obj(UniValue::VOBJ);
    obj.pushKV("op", opKevaNamepsace);
    obj.pushKV("namespace", EncodeBase58Check(std::get<0>(entry)));
    obj.pushKV("display name", ValtypeToString(std::get<1>(entry)));
    obj.pushKV("txid", std::get<2>(entry).ToString());
    arr.push_back(obj);
  }

  for (auto entry: unconfirmedKeyValueList) {
    UniValue obj(UniValue::VOBJ);
    obj.pushKV("op", opKevaPut);
    obj.pushKV("namespace", EncodeBase58Check(std::get<0>(entry)));
    obj.pushKV("key", ValtypeToString(std::get<1>(entry)));
    obj.pushKV("value", ValtypeToString(std::get<2>(entry)));
    obj.pushKV("txid", std::get<3>(entry).ToString());
    arr.push_back(obj);
  }

  return arr;
}
