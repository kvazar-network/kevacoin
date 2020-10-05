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
 * @param height The key's last update height.
 * @return A JSON object to return.
 */
UniValue
getKevaInfo(const valtype& key, const valtype& value, const COutPoint& outp,
             int height, const valtype& nameSpace)
{
  UniValue obj(UniValue::VOBJ);
  obj.pushKV("key", ValtypeToString(key));
  obj.pushKV("value", ValtypeToString(value));
  obj.pushKV("txid", outp.hash.GetHex());
  obj.pushKV("vout", static_cast<int>(outp.n));
  obj.pushKV("height", height);
  if (nameSpace.size() > 0) {
    obj.pushKV("namespace", EncodeBase58Check(nameSpace));
  }

  return obj;
}

/**
 * Return keva info object for a CKevaData object.
 * @param key The key.
 * @param data The key's data.
 * @return A JSON object to return.
 */
UniValue
getKevaInfo(const valtype& key, const CKevaData& data, const valtype& nameSpace=valtype())
{
  return getKevaInfo(key, data.getValue(), data.getUpdateOutpoint(),
                      data.getHeight(), nameSpace);
}

UniValue keva_get(const JSONRPCRequest& request)
{
  if (request.fHelp || request.params.size() != 2) {
    throw std::runtime_error (
        "keva_get \"namespace\" \"key\"\n"
        "\nGet value of the given key.\n"
        "\nArguments:\n"
        "1. \"namespace\"            (string, required) the namespace to get the value of the key \n"
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

enum InitiatorType : int
{
    INITIATOR_TYPE_ALL,
    INITIATOR_TYPE_SELF,
    INITIATOR_TYPE_OTHER
};


// Check if the key has the format _g:NamespaceId. If yes,
// return the namespace.
bool isNamespaceGroup(const valtype& key, valtype& targetNS)
{
  std::string keyStr = ValtypeToString(key);
  if (keyStr.rfind(CKevaData::ASSOCIATE_PREFIX, 0) != 0) {
    return false;
  }
  keyStr.erase(0, CKevaData::ASSOCIATE_PREFIX.length());
  if (!DecodeKevaNamespace(keyStr, Params(), targetNS)) {
    return false;
  }
  return true;
}

void getNamespaceGroup(const valtype& nameSpace, std::set<valtype>& namespaces, const InitiatorType type)
{
  CKevaData data;
  // Find the namespace connection initialized by others.
  if (type == INITIATOR_TYPE_ALL || type == INITIATOR_TYPE_OTHER) {
    valtype ns;
    std::unique_ptr<CKevaIterator> iter(pcoinsTip->IterateAssociatedNamespaces(nameSpace));
    while (iter->next(ns, data)) {
      namespaces.insert(ns);
    }
  }

  if (type == INITIATOR_TYPE_OTHER) {
    return;
  }

  // Find the namespace connection initialized by us, and not confirmed yet.
  {
    LOCK (mempool.cs);
    std::vector<std::tuple<valtype, valtype, valtype, uint256>> unconfirmedKeyValueList;
    mempool.getUnconfirmedKeyValueList(unconfirmedKeyValueList, nameSpace);
    std::set<valtype> nsList;
    valtype targetNS;
    for (auto entry: unconfirmedKeyValueList) {
      const valtype ns = std::get<0>(entry);
      if (ns != nameSpace) {
        continue;
      }
      const valtype key = std::get<1>(entry);
      // Find the value with the format _g:NamespaceId
      if (!isNamespaceGroup(key, targetNS)) {
        continue;
      }
      if (nsList.find(targetNS) != nsList.end()) {
        continue;
      }
      valtype val;
      if (mempool.getUnconfirmedKeyValue(nameSpace, key, val) && val.size() > 0) {
        namespaces.insert(targetNS);
        nsList.insert(targetNS);
      }
    }
  }

  // Find the namespace connection initialized by us.
  std::unique_ptr<CKevaIterator> iterKeys(pcoinsTip->IterateKeys(nameSpace));
  valtype key;
  while (iterKeys->next(key, data)) {
    valtype targetNS;
    // Find the value with the format _g:NamespaceId
    if (isNamespaceGroup(key, targetNS)) {
      // If it has been removed but not yet confirmed, skip it anyway.
      {
        LOCK (mempool.cs);
        valtype val;
        if (mempool.getUnconfirmedKeyValue(nameSpace, key, val) && val.size() == 0) {
          continue;
        }
      }
      namespaces.insert(targetNS);
    }
  }

}

UniValue keva_group_get(const JSONRPCRequest& request)
{
  if (request.fHelp || request.params.size() < 2) {
    throw std::runtime_error (
        "keva_group_get \"namespace\" \"key\" \"initiator\"\n"
        "\nGet value of the given key from the namespace and other namespaces in the same group.\n"
        "\nArguments:\n"
        "1. \"namespace\"            (string, required) the namespace to get the value of the key\n"
        "2. \"key\"                  (string, required) value for the key\n"
        "3. \"initiator\"            (string, optional) Options are \"all\", \"self\" and \"other\", default is \"all\". \"all\": all the namespaces, whose participation in the group is initiated by this namespace or other namespaces. \"self\": only the namespace whose participation is initiated by this namespace. \"other\": only the namespace whose participation is initiated by other namespaces.\n"
        "\nResult:\n"
        "\"value\"             (string) the value associated with the key\n"
        "\nExamples:\n"
        + HelpExampleCli ("keva_get", "\"namespace_id\", \"key\"")
        + HelpExampleCli ("keva_get", "\"namespace_id\", \"key\", \"self\"")
      );
  }

  RPCTypeCheck(request.params, {UniValue::VSTR, UniValue::VSTR, UniValue::VSTR});

  InitiatorType initiatorType = INITIATOR_TYPE_ALL;
  if (request.params.size() == 3) {
    const std::string initiator = request.params[2].get_str();
    if (initiator == "all") {
      initiatorType = INITIATOR_TYPE_ALL;
    } else if (initiator == "self") {
      initiatorType = INITIATOR_TYPE_SELF;
    } else if (initiator == "other") {
      initiatorType = INITIATOR_TYPE_SELF;
    } else {
      throw JSONRPCError (RPC_INVALID_PARAMETER, "invalid initiator type");
    }
  }

  ObserveSafeMode();

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

  std::set<valtype> namespaces;
  {
    LOCK(cs_main);
    namespaces.insert(nameSpace);
    getNamespaceGroup(nameSpace, namespaces, initiatorType);
  }

  // If there is unconfirmed one, return its value.
  {
    LOCK(mempool.cs);
    valtype val;
    for (auto iter = namespaces.begin(); iter != namespaces.end(); ++iter) {
      if (mempool.getUnconfirmedKeyValue(*iter, key, val)) {
        // Return the first unconfirmed one.
        UniValue obj(UniValue::VOBJ);
        obj.pushKV("key", keyStr);
        obj.pushKV("value", ValtypeToString(val));
        obj.pushKV("height", -1);
        obj.pushKV("namespace", EncodeBase58Check(*iter));
        return obj;
      }
    }
  }

  // Otherwise, return the confirmed value.
  {
    LOCK(cs_main);
    unsigned currentHeight = 0;
    CKevaData data;
    CKevaData currentData;
    valtype ns;
    for (auto iter = namespaces.begin(); iter != namespaces.end(); ++iter) {
      if (pcoinsTip->GetName(*iter, key, currentData)) {
        if (currentData.getHeight() > currentHeight) {
          currentHeight = currentData.getHeight();
          data = currentData;
          ns = *iter;
        }
      }
    }
    if (currentHeight > 0) {
      return getKevaInfo(key, data, ns);
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
  res << indent << "  \"height\": xxxxx,         "
      << "(numeric) the key's last update height" << std::endl;
  res << indent << "}" << trailing << std::endl;

  return res.str ();
}

UniValue keva_group_filter(const JSONRPCRequest& request)
{
  if (request.fHelp || request.params.size() > 6 || request.params.size() == 0)
    throw std::runtime_error(
        "keva_group_filter (\"namespaceId\" (\"initiator\" \"regexp\" (\"from\" (\"nb\" (\"stat\")))))\n"
        "\nScan and list keys matching a regular expression.\n"
        "\nArguments:\n"
        "1. \"namespace\"   (string) namespace Id\n"
        "2. \"initiator\"   (string, optional) Options are \"all\", \"self\" and \"other\", default is \"all\". \"all\": all the namespaces, whose participation in the group is initiated by this namespace or other namespaces. \"self\": only the namespace whose participation is initiated by this namespace. \"other\": only the namespace whose participation is initiated by other namespaces.\n"
        "3. \"regexp\"      (string, optional) filter keys with this regexp\n"
        "4. \"maxage\"      (numeric, optional, default=96000) only consider names updated in the last \"maxage\" blocks; 0 means all names\n"
        "5. \"from\"        (numeric, optional, default=0) return from this position onward; index starts at 0\n"
        "6. \"nb\"          (numeric, optional, default=0) return only \"nb\" entries; 0 means all\n"
        "7. \"stat\"        (string, optional) if set to the string \"stat\", print statistics instead of returning the names\n"
        "\nResult:\n"
        "[\n"
        + getKevaInfoHelp ("  ", ",") +
        "  ...\n"
        "]\n"
        "\nExamples:\n"
        + HelpExampleCli ("keva_group_filter", "\"namespaceId\" \"all\"")
        + HelpExampleCli ("keva_group_filter", "\"namespaceId\" \"self\" 96000 0 0 \"stat\"")
        + HelpExampleRpc ("keva_group_filter", "\"namespaceId\"")
      );

  RPCTypeCheck(request.params, {
                  UniValue::VSTR, UniValue::VSTR, UniValue::VSTR, UniValue::VNUM,
                  UniValue::VNUM, UniValue::VNUM, UniValue::VSTR
               });

  if (IsInitialBlockDownload()) {
    throw JSONRPCError(RPC_CLIENT_IN_INITIAL_DOWNLOAD,
                       "Kevacoin is downloading blocks...");
  }

  ObserveSafeMode();

  bool haveRegexp(false);
  boost::xpressive::sregex regexp;

  valtype nameSpace;
  int maxage(96000), from(0), nb(0);
  bool stats(false);
  InitiatorType initiatorType = INITIATOR_TYPE_ALL;

  if (request.params.size() >= 1) {
    const std::string namespaceStr = request.params[0].get_str();
    if (!DecodeKevaNamespace(namespaceStr, Params(), nameSpace)) {
      throw JSONRPCError (RPC_INVALID_PARAMETER, "invalid namespace id");
    }
  }

  if (request.params.size() >= 2) {
    const std::string initiator = request.params[1].get_str();
    if (initiator == "all") {
      initiatorType = INITIATOR_TYPE_ALL;
    } else if (initiator == "self") {
      initiatorType = INITIATOR_TYPE_SELF;
    } else if (initiator == "other") {
      initiatorType = INITIATOR_TYPE_OTHER;
    } else {
      throw JSONRPCError (RPC_INVALID_PARAMETER, "invalid initiator");
    }
  }

  if (request.params.size() >= 3) {
    haveRegexp = true;
    regexp = boost::xpressive::sregex::compile (request.params[2].get_str());
  }

  if (request.params.size() >= 4)
    maxage = request.params[3].get_int();
  if (maxage < 0)
    throw JSONRPCError(RPC_INVALID_PARAMETER,
                        "'maxage' should be non-negative");

  if (request.params.size() >= 5)
    from = request.params[4].get_int ();

  if (from < 0)
    throw JSONRPCError (RPC_INVALID_PARAMETER, "'from' should be non-negative");

  if (request.params.size() >= 6)
    nb = request.params[5].get_int ();

  if (nb < 0)
    throw JSONRPCError (RPC_INVALID_PARAMETER, "'nb' should be non-negative");

  if (request.params.size() >= 7) {
    if (request.params[6].get_str() != "stat")
      throw JSONRPCError (RPC_INVALID_PARAMETER,
                          "fifth argument must be the literal string 'stat'");
    stats = true;
  }

  UniValue uniKeys(UniValue::VARR);
  unsigned count(0);
  std::map<valtype, std::tuple<CKevaData, valtype>> keys;

  LOCK (cs_main);
  std::set<valtype> namespaces;
  namespaces.insert(nameSpace);
  getNamespaceGroup(nameSpace, namespaces, initiatorType);

  valtype key;
  CKevaData data;
  valtype displayKey = ValtypeFromString(CKevaScript::KEVA_DISPLAY_NAME_KEY);
  for (auto iterNS = namespaces.begin(); iterNS != namespaces.end(); ++iterNS) {
    std::unique_ptr<CKevaIterator> iter(pcoinsTip->IterateKeys(*iterNS));
    while (iter->next(key, data)) {
      if (key == displayKey) {
        continue;
      }
      const int age = chainActive.Height() - data.getHeight();
      assert(age >= 0);
      if (maxage != 0 && age >= maxage) {
        continue;
      }

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

      if (stats) {
        ++count;
      } else {
        auto it = keys.find(key);
        if (it == keys.end()) {
          keys.insert(std::make_pair(key, std::make_tuple(data, *iterNS)));
        } else if (data.getHeight() > std::get<0>(it->second).getHeight()) {
          it->second = std::make_tuple(data, *iterNS);
        }
      }

      if (nb > 0) {
        --nb;
        if (nb == 0)
          break;
      }
    }
  }

  if (stats) {
    UniValue res(UniValue::VOBJ);
    res.pushKV("blocks", chainActive.Height());
    res.pushKV("count", static_cast<int>(count));
    return res;
  }

  for (auto e = keys.begin(); e != keys.end(); ++e) {
    uniKeys.push_back(getKevaInfo(e->first, std::get<0>(e->second), std::get<1>(e->second)));
  }

  return uniKeys;
}


UniValue keva_filter(const JSONRPCRequest& request)
{
  if (request.fHelp || request.params.size() > 6 || request.params.size() == 0)
    throw std::runtime_error(
        "keva_filter (\"namespaceId\" (\"regexp\" (\"from\" (\"nb\" (\"stat\")))))\n"
        "\nScan and list keys matching a regular expression.\n"
        "\nArguments:\n"
        "1. \"namespace\"   (string) namespace Id\n"
        "2. \"regexp\"      (string, optional) filter keys with this regexp, use Perl regexp syntax\n"
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
 * @param height The height at which the namespace joins the group.
 * @param initiator If true, the namespace connection is initiated by this namespace.
 * @return A JSON object to return.
 */
UniValue
getNamespaceInfo(const valtype& namespaceId, const valtype& name, const COutPoint& outp,
             int height, bool initiator)
{
  UniValue obj(UniValue::VOBJ);
  obj.pushKV("namespaceId", EncodeBase58Check(namespaceId));
  obj.pushKV("display_name", ValtypeToString(name));
  obj.pushKV("txid", outp.hash.GetHex());
  obj.pushKV("height", height);
  obj.pushKV("initiator", initiator);

  return obj;
}

UniValue keva_group_show(const JSONRPCRequest& request)
{
  if (request.fHelp || request.params.size() > 6 || request.params.size() == 0)
    throw std::runtime_error(
        "keva_group_show (\"namespaceId\" (\"regexp\" (\"from\" (\"nb\" (\"stat\")))))\n"
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
        + HelpExampleCli ("keva_group_show", "NamespaceId")
        + HelpExampleCli ("keva_group_show", "NamespaceId 96000 0 0 \"stat\"")
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
                      data.getHeight(), true));
    }

    if (nb > 0) {
      --nb;
      if (nb == 0)
        break;
    }
  }

  // Find the namespace connection initialized by us, and not confirmed yet.
  {
    LOCK (mempool.cs);
    std::vector<std::tuple<valtype, valtype, valtype, uint256>> unconfirmedKeyValueList;
    mempool.getUnconfirmedKeyValueList(unconfirmedKeyValueList, nameSpace);
    std::set<valtype> nsList;
    valtype targetNS;
    for (auto entry: unconfirmedKeyValueList) {
      const valtype ns = std::get<0>(entry);
      if (ns != nameSpace) {
        continue;
      }
      const valtype key = std::get<1>(entry);
      // Find the value with the format _g:NamespaceId
      if (!isNamespaceGroup(key, targetNS)) {
        continue;
      }
      if (nsList.find(targetNS) != nsList.end()) {
        continue;
      }
      valtype val;
      if (mempool.getUnconfirmedKeyValue(nameSpace, key, val) && val.size() > 0) {
        CKevaData nsData;
        valtype nsName;
        if (pcoinsTip->GetName(targetNS, nsDisplayKey, nsData)) {
          nsName = nsData.getValue();
        }
        UniValue obj(UniValue::VOBJ);
        obj.pushKV("namespaceId", EncodeBase58Check(targetNS));
        obj.pushKV("display_name", ValtypeToString(nsName));
        obj.pushKV("height", -1);
        obj.pushKV("initiator", false);
        namespaces.push_back(obj);
        nsList.insert(targetNS);
      }
    }
  }

  // Find the namespace connection initialized by us and confirmed.
  std::unique_ptr<CKevaIterator> iterKeys(pcoinsTip->IterateKeys(nameSpace));
  valtype targetNS;
  valtype key;
  while (iterKeys->next(key, data)) {

    // Find the value with the format _g:NamespaceId
    if (!isNamespaceGroup(key, targetNS)) {
      continue;
    }

    // If it has been removed but not yet confirmed, skip it anyway.
    {
      LOCK (mempool.cs);
      valtype val;
      if (mempool.getUnconfirmedKeyValue(nameSpace, key, val) && val.size() == 0) {
        continue;
      }
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
                      data.getHeight(), false));
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
    { "kevacoin",           "keva_group_show",       &keva_group_show,       {"namespace", "from", "nb", "stat"} },
    { "kevacoin",           "keva_group_get",        &keva_group_get,        {"namespace", "key", "initiator"} },
    { "kevacoin",           "keva_group_filter",     &keva_group_filter,     {"namespace", "initiator", "regexp", "from", "nb", "stat"} }
};

void RegisterKevaRPCCommands(CRPCTable &t)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
