// Copyright (c) 2014-2017 Daniel Kraft
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// Copyright (c) 2018 the Kevacoin Core Developers
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
#include <boost/xpressive/xpressive_dynamic.hpp>

const int NAMESPACE_LENGTH           =  21;
const std::string DUMMY_NAMESPACE    =  "___DUMMY_NAMESPACE___";


/* ************************************************************************** */

UniValue keva_namespace(const JSONRPCRequest& request)
{
  CWallet* const pwallet = GetWalletForJSONRPCRequest(request);
  if (!EnsureWalletIsAvailable (pwallet, request.fHelp))
    return NullUniValue;

  if (request.fHelp || request.params.size() != 1)
    throw std::runtime_error (
        "keva_namespace \"display_name\"\n"
        "\nRegister a namespace with the given display name.\n"
        + HelpRequiringPassphrase (pwallet) +
        "\nArguments:\n"
        "1. \"display_name\"          (string, required) the display name of the namespace\n"
        "\nResult:\n"
        "[\n"
        "  xxxxx,   (string) the txid\n"
        "  xxxxx,   (string) the unique namespace id\n"
        "]\n"
        "\nExamples:\n"
        + HelpExampleCli ("keva_namespace", "\"display name\"")
      );

  RPCTypeCheck (request.params, {UniValue::VSTR});

  ObserveSafeMode ();

  const std::string displayNameStr = request.params[0].get_str();
  const valtype displayName = ValtypeFromString (displayNameStr);
  if (displayName.size() > MAX_NAMESPACE_LENGTH) {
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
  UniValue obj(UniValue::VOBJ);
  obj.pushKV("txid", txid);
  obj.pushKV("namespaceId", kevaNamespaceBase58);
  res.push_back(obj);

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
      for (unsigned i = 0; i < tx.tx->vout.size(); ++i)
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
        "keva_put \"namespace\" \"key\" \"value\"\n"
        "\nInsert or update a key value pair in the given namespace.\n"
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
  if (nameSpace.size() > MAX_NAMESPACE_LENGTH)
    throw JSONRPCError (RPC_INVALID_PARAMETER, "the namespace is too long");

  const std::string keyStr = request.params[1].get_str ();
  const valtype key = ValtypeFromString (keyStr);
  if (key.size() > MAX_KEY_LENGTH)
    throw JSONRPCError (RPC_INVALID_PARAMETER, "the key is too long");

  const std::string valueStr = request.params[2].get_str ();
  const valtype value = ValtypeFromString (valueStr);
  if (value.size() > MAX_VALUE_LENGTH)
    throw JSONRPCError (RPC_INVALID_PARAMETER, "the value is too long");

  EnsureWalletIsUnlocked(pwallet);

  COutput output;
  std::string kevaNamespce = namespaceStr;
  if (!pwallet->FindKevaCoin(output, kevaNamespce)) {
    throw JSONRPCError (RPC_TRANSACTION_ERROR, "this namespace can not be updated");
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

UniValue keva_delete(const JSONRPCRequest& request)
{
  CWallet* const pwallet = GetWalletForJSONRPCRequest(request);
  if (!EnsureWalletIsAvailable (pwallet, request.fHelp)) {
    return NullUniValue;
  }

  if (request.fHelp || request.params.size() != 2) {
    throw std::runtime_error (
        "keva_delete \"namespace\" \"key\"\n"
        "\nRemove the specified key from the given namespace.\n"
        + HelpRequiringPassphrase (pwallet) +
        "\nArguments:\n"
        "1. \"namespace\"            (string, required) the namespace to insert the key to\n"
        "2. \"key\"                  (string, required) value for the key\n"
        "\nResult:\n"
        "\"txid\"             (string) the keva_delete's txid\n"
        "\nExamples:\n"
        + HelpExampleCli ("keva_delete", "\"mynamespace\", \"key\"")
      );
  }

  RPCTypeCheck(request.params, {UniValue::VSTR, UniValue::VSTR});
  RPCTypeCheckArgument(request.params[0], UniValue::VSTR);
  RPCTypeCheckArgument(request.params[1], UniValue::VSTR);

  ObserveSafeMode ();

  const std::string namespaceStr = request.params[0].get_str();
  valtype nameSpace;
  if (!DecodeKevaNamespace(namespaceStr, Params(), nameSpace)) {
    throw JSONRPCError (RPC_INVALID_PARAMETER, "invalid namespace id");
  }
  if (nameSpace.size() > MAX_NAMESPACE_LENGTH) {
    throw JSONRPCError (RPC_INVALID_PARAMETER, "the namespace is too long");
  }

  const std::string keyStr = request.params[1].get_str ();
  const valtype key = ValtypeFromString (keyStr);
  if (key.size() > MAX_KEY_LENGTH) {
    throw JSONRPCError (RPC_INVALID_PARAMETER, "the key is too long");
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
    throw JSONRPCError (RPC_TRANSACTION_ERROR, "key not found");
  }

  EnsureWalletIsUnlocked(pwallet);

  COutput output;
  std::string kevaNamespce = namespaceStr;
  if (!pwallet->FindKevaCoin(output, kevaNamespce)) {
    throw JSONRPCError (RPC_TRANSACTION_ERROR, "this namespace can not be updated");
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

  const CScript kevaScript = CKevaScript::buildKevaDelete(addrName, nameSpace, key);

  CCoinControl coinControl;
  CWalletTx wtx;
  valtype empty;
  SendMoneyToScript(pwallet, kevaScript, &txIn, empty,
                     KEVA_LOCKED_AMOUNT, false, wtx, coinControl);

  if (usedKey) {
    keyName.KeepKey();
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
  if (nameSpace.size() > MAX_NAMESPACE_LENGTH)
    throw JSONRPCError (RPC_INVALID_PARAMETER, "the namespace is too long");

  const std::string keyStr = request.params[1].get_str ();
  const valtype key = ValtypeFromString (keyStr);
  if (key.size() > MAX_KEY_LENGTH)
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
  if (request.fHelp || request.params.size() > 1)
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
  const std::string opKevaDelete    = "keva_delete";

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
    const valtype val = std::get<2>(entry);
    if (val.size() > 0) {
      obj.pushKV("op", opKevaPut);
      obj.pushKV("namespace", EncodeBase58Check(std::get<0>(entry)));
      obj.pushKV("key", ValtypeToString(std::get<1>(entry)));
      obj.pushKV("value", ValtypeToString(std::get<2>(entry)));
      obj.pushKV("txid", std::get<3>(entry).ToString());
    } else {
      obj.pushKV("op", opKevaDelete);
      obj.pushKV("namespace", EncodeBase58Check(std::get<0>(entry)));
      obj.pushKV("key", ValtypeToString(std::get<1>(entry)));
      obj.pushKV("txid", std::get<3>(entry).ToString());
    }
    arr.push_back(obj);
  }

  return arr;
}

/**
 * Return the help string description to use for keva info objects.
 * @param indent Indentation at the line starts.
 * @param trailing Trailing string (e. g., comma for an array of these objects).
 * @return The description string.
 */
std::string getKevaInfoHelp (const std::string& indent, const std::string& trailing)
{
  std::ostringstream res;

  res << indent << "{" << std::endl;
  res << indent << "  \"name\": xxxxx,           "
      << "(string) the requested name" << std::endl;
  res << indent << "  \"value\": xxxxx,          "
      << "(string) the name's current value" << std::endl;
  res << indent << "  \"txid\": xxxxx,           "
      << "(string) the name's last update tx" << std::endl;
  res << indent << "  \"address\": xxxxx,        "
      << "(string) the address holding the name" << std::endl;
  res << indent << "  \"height\": xxxxx,         "
      << "(numeric) the name's last update height" << std::endl;
  res << indent << "}" << trailing << std::endl;

  return res.str ();
}

/**
 * Utility routine to construct a "name info" object to return.  This is used
 * for name_show and also name_list.
 * @param name The name.
 * @param value The name's value.
 * @param outp The last update's outpoint.
 * @param addr The name's address script.
 * @param height The name's last update height.
 * @return A JSON object to return.
 */
UniValue
getKevaInfo (const valtype& name, const valtype& value, const COutPoint& outp,
             const CScript& addr, int height)
{
  UniValue obj(UniValue::VOBJ);
  obj.pushKV("name", ValtypeToString(name));
  obj.pushKV("value", ValtypeToString(value));
  obj.pushKV("txid", outp.hash.GetHex());
  obj.pushKV("vout", static_cast<int>(outp.n));

  /* Try to extract the address.  May fail if we can't parse the script
     as a "standard" script.  */
  CTxDestination dest;
  std::string addrStr;
  if (ExtractDestination(addr, dest))
    addrStr = EncodeDestination(dest);
  else
    addrStr = "<nonstandard>";
  obj.pushKV("address", addrStr);

  /* Calculate expiration data.  */
  const int curHeight = chainActive.Height ();
  const Consensus::Params& params = Params ().GetConsensus ();
  obj.pushKV("height", height);

  return obj;
}

/**
 * Return name info object for a CNameData object.
 * @param name The name.
 * @param data The name's data.
 * @return A JSON object to return.
 */
UniValue
getKevaInfo (const valtype& name, const CKevaData& data)
{
  return getKevaInfo(name, data.getValue (), data.getUpdateOutpoint (),
                      data.getAddress (), data.getHeight ());
}


UniValue keva_filter(const JSONRPCRequest& request)
{
  if (request.fHelp || request.params.size() > 6 || request.params.size() == 0)
    throw std::runtime_error(
        "keva_filter (\"namespaceId\" (\"regexp\" (\"from\" (\"nb\" (\"stat\")))))\n"
        "\nScan and list keys matching a regular expression.\n"
        "\nArguments:\n"
        "1. \"namespace\"   (string) namespace Id\n"
        "2. \"regexp\"      (string, optional) filter keys with this regexp\n"
        "3. \"maxage\"      (numeric, optional, default=36000) only consider names updated in the last \"maxage\" blocks; 0 means all names\n"
        "4. \"from\"        (numeric, optional, default=0) return from this position onward; index starts at 0\n"
        "5. \"nb\"          (numeric, optional, default=0) return only \"nb\" entries; 0 means all\n"
        "6. \"stat\"        (string, optional) if set to the string \"stat\", print statistics instead of returning the names\n"
        "\nResult:\n"
        "[\n"
        + getKevaInfoHelp ("  ", ",") +
        "  ...\n"
        "]\n"
        "\nExamples:\n"
        + HelpExampleCli ("keva_filter", "\"\" 5")
        + HelpExampleCli ("keva_filter", "\"^id/\"")
        + HelpExampleCli ("keva_filter", "\"^id/\" 36000 0 0 \"stat\"")
        + HelpExampleRpc ("keva_scan", "\"^d/\"")
      );

  RPCTypeCheck(request.params, {
                  UniValue::VSTR, UniValue::VSTR, UniValue::VNUM,
                  UniValue::VNUM, UniValue::VNUM, UniValue::VSTR
               });

  if (IsInitialBlockDownload()) {
    throw JSONRPCError(RPC_CLIENT_IN_INITIAL_DOWNLOAD,
                       "Kevacoin is downloading blocks...");
  }

  ObserveSafeMode();

  /* ********************** */
  /* Interpret parameters.  */

  bool haveRegexp(false);
  boost::xpressive::sregex regexp;

  valtype nameSpace;
  int maxage(36000), from(0), nb(0);
  bool stats(false);

  if (request.params.size() >= 1) {
    const std::string namespaceStr = request.params[0].get_str();
    valtype nameSpace;
    if (!DecodeKevaNamespace(namespaceStr, Params(), nameSpace)) {
      throw JSONRPCError (RPC_INVALID_PARAMETER, "invalid namespace id");
    }
  }

  if (request.params.size() >= 2) {
    haveRegexp = true;
    regexp = boost::xpressive::sregex::compile (request.params[1].get_str());
  }

  if (request.params.size() >= 3)
    maxage = request.params[2].get_int();
  if (maxage < 0)
    throw JSONRPCError(RPC_INVALID_PARAMETER,
                        "'maxage' should be non-negative");

  if (request.params.size() >= 4)
    from = request.params[3].get_int ();

  if (from < 0)
    throw JSONRPCError (RPC_INVALID_PARAMETER, "'from' should be non-negative");

  if (request.params.size() >= 5)
    nb = request.params[4].get_int ();

  if (nb < 0)
    throw JSONRPCError (RPC_INVALID_PARAMETER, "'nb' should be non-negative");

  if (request.params.size() >= 6) {
    if (request.params[5].get_str() != "stat")
      throw JSONRPCError (RPC_INVALID_PARAMETER,
                          "fifth argument must be the literal string 'stat'");
    stats = true;
  }

  /* ******************************************* */
  /* Iterate over names to build up the result.  */

  UniValue keys(UniValue::VARR);
  unsigned count(0);

  LOCK (cs_main);

  valtype key;
  CKevaData data;
  std::unique_ptr<CKevaIterator> iter(pcoinsTip->IterateKeys(nameSpace));
  while (iter->next(key, data)) {
    const int age = chainActive.Height() - data.getHeight();
    assert(age >= 0);
    if (maxage != 0 && age >= maxage)
      continue;

    if (haveRegexp) {
      const std::string keyStr = ValtypeToString(key);
      boost::xpressive::smatch matches;
      if (!boost::xpressive::regex_search(keyStr, matches, regexp))
        continue;
    }

    if (from > 0) {
      --from;
      continue;
    }
    assert(from == 0);

    if (stats)
      ++count;
    else
      keys.push_back(getKevaInfo(key, data));

    if (nb > 0) {
      --nb;
      if (nb == 0)
        break;
    }
  }

  /* ********************************************************** */
  /* Return the correct result (take stats mode into account).  */

  if (stats) {
    UniValue res(UniValue::VOBJ);
    res.pushKV("blocks", chainActive.Height());
    res.pushKV("count", static_cast<int>(count));
    return res;
  }

  return keys;
}
