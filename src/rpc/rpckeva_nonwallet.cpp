// Copyright (c) 2014-2017 Daniel Kraft
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// Copyright (c) 2018-2020 the Kevacoin Core Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "base58.h"
#include "coins.h"
#include "init.h"
#include "keva/common.h"
#include "keva/main.h"
#include "script/keva.h"
#include "primitives/transaction.h"
#include "random.h"
#include "rpc/mining.h"
#include "rpc/safemode.h"
#include "rpc/server.h"
#include "script/keva.h"
#include "txmempool.h"
#include "util.h"
#include "validation.h"
#include "utilstrencodings.h"

#include <univalue.h>
#include <boost/xpressive/xpressive_dynamic.hpp>

/**
 * Utility routine to construct a "keva info" object to return.  This is used
 * for keva_filter.
 * @param key The key.
 * @param value The key's value.
 * @param outp The last update's outpoint.
 * @param addr The key's address script.
 * @param height The key's last update height.
 * @return A JSON object to return.
 */
UniValue
getKevaInfo(const valtype& key, const valtype& value, const COutPoint& outp,
             const CScript& addr, int height)
{
  UniValue obj(UniValue::VOBJ);
  obj.pushKV("key", ValtypeToString(key));
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
  obj.pushKV("height", height);

  return obj;
}

/**
 * Return keva info object for a CKevaData object.
 * @param key The key.
 * @param data The key's data.
 * @return A JSON object to return.
 */
UniValue
getKevaInfo(const valtype& key, const CKevaData& data)
{
  return getKevaInfo(key, data.getValue(), data.getUpdateOutpoint(),
                      data.getAddress(), data.getHeight());
}

UniValue keva_get(const JSONRPCRequest& request)
{
  if (request.fHelp || request.params.size() != 2) {
    throw std::runtime_error (
        "keva_get \"namespace\" \"key\"\n"
        "\nGet value of the given key.\n"
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

  const std::string keyStr = request.params[1].get_str();
  const valtype key = ValtypeFromString(keyStr);
  if (key.size() > MAX_KEY_LENGTH)
    throw JSONRPCError(RPC_INVALID_PARAMETER, "the key is too long");

  // If there is unconfirmed one, return its value.
  {
    LOCK (mempool.cs);
    valtype val;
    if (mempool.getUnconfirmedKeyValue(nameSpace, key, val)) {
      UniValue obj(UniValue::VOBJ);
      obj.pushKV("key", keyStr);
      obj.pushKV("value", ValtypeToString(val));
      obj.pushKV("height", -1);
      return obj;
    }
  }

  // Otherwise, return the confirmed value.
  {
    LOCK(cs_main);
    CKevaData data;
    if (pcoinsTip->GetName(nameSpace, key, data)) {
      return getKevaInfo(key, data);
    }
  }

  // Empty value
  UniValue obj(UniValue::VOBJ);
  obj.pushKV("key", keyStr);
  obj.pushKV("value", "");
  return obj;
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
  res << indent << "  \"key\": xxxxx,           "
      << "(string) the requested key" << std::endl;
  res << indent << "  \"value\": xxxxx,          "
      << "(string) the key's current value" << std::endl;
  res << indent << "  \"txid\": xxxxx,           "
      << "(string) the key's last update tx" << std::endl;
  res << indent << "  \"address\": xxxxx,        "
      << "(string) the address holding the key" << std::endl;
  res << indent << "  \"height\": xxxxx,         "
      << "(numeric) the key's last update height" << std::endl;
  res << indent << "}" << trailing << std::endl;

  return res.str ();
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
        "3. \"maxage\"      (numeric, optional, default=96000) only consider names updated in the last \"maxage\" blocks; 0 means all names\n"
        "4. \"from\"        (numeric, optional, default=0) return from this position onward; index starts at 0\n"
        "5. \"nb\"          (numeric, optional, default=0) return only \"nb\" entries; 0 means all\n"
        "6. \"stat\"        (string, optional) if set to the string \"stat\", print statistics instead of returning the names\n"
        "\nResult:\n"
        "[\n"
        + getKevaInfoHelp ("  ", ",") +
        "  ...\n"
        "]\n"
        "\nExamples:\n"
        + HelpExampleCli ("keva_filter", "\"^id/\"")
        + HelpExampleCli ("keva_filter", "\"^id/\" 96000 0 0 \"stat\"")
        + HelpExampleRpc ("keva_filter", "\"^d/\"")
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
  int maxage(96000), from(0), nb(0);
  bool stats(false);

  if (request.params.size() >= 1) {
    const std::string namespaceStr = request.params[0].get_str();
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

/**
 * Utility routine to construct a "namespace info" object to return.  This is used
 * for keva_group.
 * @param namespaceId The namespace Id.
 * @param name The display name of the namespace.
 * @param outp The last update's outpoint.
 * @param addr The namespace's address script.
 * @param height The height at which the namespace joins the group.
 * @param initiator If true, the namespace connection is initiated by this namespace.
 * @return A JSON object to return.
 */
UniValue
getNamespaceInfo(const valtype& namespaceId, const valtype& name, const COutPoint& outp,
             const CScript& addr, int height, bool initiator)
{
  UniValue obj(UniValue::VOBJ);
  obj.pushKV("namespaceId", EncodeBase58Check(namespaceId));
  obj.pushKV("display_name", ValtypeToString(name));
  obj.pushKV("txid", outp.hash.GetHex());

  // Try to extract the address. May fail if we can't parse the script as a "standard" script.
  CTxDestination dest;
  std::string addrStr;
  if (ExtractDestination(addr, dest)) {
    addrStr = EncodeDestination(dest);
  } else {
    addrStr = "<nonstandard>";
  }
  obj.pushKV("address", addrStr);
  obj.pushKV("height", height);
  obj.pushKV("initiator", initiator);

  return obj;
}

UniValue keva_show_group(const JSONRPCRequest& request)
{
  if (request.fHelp || request.params.size() > 6 || request.params.size() == 0)
    throw std::runtime_error(
        "keva_show_group (\"namespaceId\" (\"regexp\" (\"from\" (\"nb\" (\"stat\")))))\n"
        "\nList namespaces that are in the same group as the given namespace.\n"
        "\nArguments:\n"
        "1. \"namespace\"   (string) namespace Id\n"
        "2. \"maxage\"      (numeric, optional, default=96000) only consider namespaces updated in the last \"maxage\" blocks; 0 means all namespaces\n"
        "3. \"from\"        (numeric, optional, default=0) return from this position onward; index starts at 0\n"
        "4. \"nb\"          (numeric, optional, default=0) return only \"nb\" entries; 0 means all\n"
        "5. \"stat\"        (string, optional) if set to the string \"stat\", print statistics instead of returning the names\n"
        "\nResult:\n"
        "[\n"
        + getKevaInfoHelp ("  ", ",") +
        "  ...\n"
        "]\n"
        "\nExamples:\n"
        + HelpExampleCli ("keva_show_group", "NamespaceId")
        + HelpExampleCli ("keva_show_group", "NamespaceId 96000 0 0 \"stat\"")
      );

  RPCTypeCheck(request.params, {
                  UniValue::VSTR, UniValue::VNUM,
                  UniValue::VNUM, UniValue::VNUM, UniValue::VSTR
               });

  if (IsInitialBlockDownload()) {
    throw JSONRPCError(RPC_CLIENT_IN_INITIAL_DOWNLOAD,
                       "Kevacoin is downloading blocks...");
  }

  ObserveSafeMode();

  // Interpret parameters.
  valtype nameSpace;
  int maxage(96000), from(0), nb(0);
  bool stats(false);

  if (request.params.size() >= 1) {
    const std::string namespaceStr = request.params[0].get_str();
    if (!DecodeKevaNamespace(namespaceStr, Params(), nameSpace)) {
      throw JSONRPCError (RPC_INVALID_PARAMETER, "invalid namespace id");
    }
  }

  if (request.params.size() >= 2)
    maxage = request.params[1].get_int();
  if (maxage < 0)
    throw JSONRPCError(RPC_INVALID_PARAMETER,
                        "'maxage' should be non-negative");

  if (request.params.size() >= 3)
    from = request.params[3].get_int ();

  if (from < 0)
    throw JSONRPCError (RPC_INVALID_PARAMETER, "'from' should be non-negative");

  if (request.params.size() >= 4)
    nb = request.params[3].get_int ();

  if (nb < 0)
    throw JSONRPCError (RPC_INVALID_PARAMETER, "'nb' should be non-negative");

  if (request.params.size() >= 5) {
    if (request.params[4].get_str() != "stat")
      throw JSONRPCError (RPC_INVALID_PARAMETER,
                          "fifth argument must be the literal string 'stat'");
    stats = true;
  }

  // Iterate over names to build up the result.
  UniValue namespaces(UniValue::VARR);
  unsigned count(0);

  LOCK (cs_main);

  valtype ns;
  CKevaData data;
  valtype nsDisplayKey = ValtypeFromString(CKevaScript::KEVA_DISPLAY_NAME_KEY);
  std::unique_ptr<CKevaIterator> iter(pcoinsTip->IterateAssociatedNamespaces(nameSpace));

  // Find the namespace connection initialized by others.
  while (iter->next(ns, data)) {
    const int age = chainActive.Height() - data.getHeight();
    assert(age >= 0);
    if (maxage != 0 && age >= maxage) {
      continue;
    }

    if (from > 0) {
      --from;
      continue;
    }
    assert(from == 0);

    if (stats) {
      ++count;
    } else {
      CKevaData nsData;
      valtype nsName;
      if (pcoinsTip->GetName(ns, nsDisplayKey, nsData)) {
        nsName = nsData.getValue();
      }
      namespaces.push_back(getNamespaceInfo(ns, nsName, data.getUpdateOutpoint(),
                      data.getAddress(), data.getHeight(), true));
    }

    if (nb > 0) {
      --nb;
      if (nb == 0)
        break;
    }
  }

  // Find the namespace connection initialized by us.
  std::unique_ptr<CKevaIterator> iterKeys(pcoinsTip->IterateKeys(nameSpace));
  valtype targetNS;
  valtype key;
  while (iterKeys->next(key, data)) {

    // Find the value with the format _g:NamespaceId
    std::string keyStr = ValtypeToString(key);
    if (keyStr.rfind(CKevaData::ASSOCIATE_PREFIX, 0) != 0) {
      continue;
    }
    keyStr.erase(0, CKevaData::ASSOCIATE_PREFIX.length());
    if (!DecodeKevaNamespace(keyStr, Params(), targetNS)) {
      continue;
    }

    const int age = chainActive.Height() - data.getHeight();
    assert(age >= 0);
    if (maxage != 0 && age >= maxage) {
      continue;
    }

    if (from > 0) {
      --from;
      continue;
    }
    assert(from == 0);

    if (stats) {
      ++count;
    }
    else {
      CKevaData nsData;
      valtype nsName;
      if (pcoinsTip->GetName(targetNS, nsDisplayKey, nsData)) {
        nsName = nsData.getValue();
      }
      namespaces.push_back(getNamespaceInfo(targetNS, nsName, data.getUpdateOutpoint(),
                      data.getAddress(), data.getHeight(), false));
    }

    if (nb > 0) {
      --nb;
      if (nb == 0)
        break;
    }
  }


  if (stats) {
    UniValue res(UniValue::VOBJ);
    res.pushKV("blocks", chainActive.Height());
    res.pushKV("count", static_cast<int>(count));
    return res;
  }

  return namespaces;
}

static const CRPCCommand commands[] =
{ //  category              name                      actor (function)         argNames
  //  --------------------- ------------------------  -----------------------  ----------
    { "kevacoin",           "keva_get",              &keva_get,              {"namespace", "key"} },
    { "kevacoin",           "keva_filter",           &keva_filter,           {"namespace", "regexp", "from", "nb", "stat"} },
    { "kevacoin",           "keva_show_group",       &keva_show_group,       {"namespace", "from", "nb", "stat"} }
};

void RegisterKevaRPCCommands(CRPCTable &t)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
