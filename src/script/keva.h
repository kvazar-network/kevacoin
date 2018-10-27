// Copyright (c) 2018 Jianping Wu
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef H_BITCOIN_SCRIPT_KEVA
#define H_BITCOIN_SCRIPT_KEVA

#include <script/script.h>

class uint160;

/**
 * A script parsed for keva operations.  This can be initialised
 * from a "standard" script, and will then determine if this is
 * a name operation and which parts it consists of.
 */
class CKevaScript
{

private:

  /** The type of operation.  OP_NOP if no (valid) name op.  */
  opcodetype op;

  /** The non-name part, i. e., the address.  */
  CScript address;

  /** The operation arguments.  */
  std::vector<valtype> args;

public:

  /**
   * Default constructor.  This enables us to declare a variable
   * and initialise it later via assignment.
   */
  inline CKevaScript()
    : op(OP_NOP)
  {}

  /**
   * The key pointing to the internal display name. This is the first
   * key created when a namespace is registered.
   */
  static std::string KEVA_DISPLAY_NAME_KEY;

  /**
   * Parse a script and determine whether it is a valid name script.  Sets
   * the member variables representing the "picked apart" name script.
   * @param script The ordinary script to parse.
   */
  explicit CKevaScript(const CScript& script);

  /**
   * Return whether this is a (valid) name script.
   * @return True iff this is a name operation.
   */
  inline bool isKevaOp() const
  {
    switch (op) {
      case OP_KEVA_PUT:
        return true;

      case OP_KEVA_NAMESPACE:
        return true;

      case OP_NOP:
        return false;

      default:
        assert(false);
    }
  }

  /**
   * Return the non-name script.
   * @return The address part.
   */
  inline const CScript& getAddress() const
  {
    return address;
  }

  /**
   * Return the name operation.  This returns OP_NAME_NEW, OP_NAME_FIRSTUPDATE
   * or OP_NAME_UPDATE.  Do not call if this is not a keva script.
   * @return The name operation opcode.
   */
  inline opcodetype getKevaOp() const
  {
    switch (op) {
      case OP_KEVA_PUT:
        return op;

      case OP_KEVA_NAMESPACE:
        return op;

      default:
        assert (false);
    }
  }

  /**
   * Return whether this is a namespace registration.
   * @return True iff this is OP_KEVA_NAMESPACE.
   */
  inline bool isNamespaceRegistration() const
  {
    switch (op) {
      case OP_KEVA_NAMESPACE:
        return true;

      default:
        assert(false);
    }
  }

  /**
   * Return whether this is a key/value update.
   * @return True iff this is OP_KEVA_PUT.
   */
  inline bool isAnyUpdate() const
  {
    switch (op) {
      case OP_KEVA_PUT:
        return true;

      default:
        assert(false);
    }
  }

  /**
   * Return the name operation name.  This call is only valid for
   * OP_KEVA_NAMESPACE or OP_KEVA_PUT.
   * @return The keva operation's namespace.
   */
  inline const valtype& getOpNamespace() const
  {
    switch (op) {
      case OP_KEVA_PUT:
        return args[0];

      case OP_KEVA_NAMESPACE:
        return args[0];

      default:
        assert(false);
    }
  }

  /**
   * Return namespace's display name.  This call is only valid for
   * OP_KEVA_NAMESPACE.
   * @return The namespace's display name.
   */
  inline const valtype& getOpNamespaceDisplayName() const
  {
    switch (op) {
      case OP_KEVA_NAMESPACE:
        return args[1];

      default:
        assert (false);
    }
  }

  /**
   * Return the keva operation key.  This call is only valid for
   * OP_KEVA_PUT.
   * @return The keva operation's value.
   */
  inline const valtype& getOpKey() const
  {
    switch (op) {
      case OP_KEVA_PUT:
        return args[1];

      default:
        assert(false);
    }
  }

  /**
   * Return the keva operation value.  This call is only valid for
   * OP_KEVA_PUT.
   * @return The keva operation's value.
   */
  inline const valtype& getOpValue() const
  {
    switch (op) {
      case OP_KEVA_PUT:
        // args[0] is namespace, args[1] is key
        return args[2];

      default:
        assert (false);
    }
  }

  /**
   * Check if the given script is a keva script.  This is a utility method.
   * @param script The script to parse.
   * @return True iff it is a name script.
   */
  static inline bool
  isKevaScript (const CScript& script)
  {
    const CKevaScript op(script);
    return op.isKevaOp ();
  }

 /**
   * Build a KEVA_NAMESPACE transaction.
   * @param addr The address script to append.
   * @param hash The hash to use.
   * @return The full KEVA_NAMESPACE script.
   */
  static CScript buildKevaNamespace(const CScript& addr, const uint160& nameSpace,
                                const valtype& displayName);

  /**
   * Build a KEVA_PUT transaction.
   * @param addr The address script to append.
   * @param hash The hash to use.
   * @return The full KEVA_PUT script.
   */
  static CScript buildKevaPut(const CScript& addr, const valtype& nameSpace,
                                const valtype& key, const valtype& value);

};

#endif // H_BITCOIN_SCRIPT_KEVA
