// Copyright (c) 2014-2017 Daniel Kraft
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// Copyright (c) 2018
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#ifndef H_BITCOIN_NAMES_COMMON
#define H_BITCOIN_NAMES_COMMON

#include <compat/endian.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <serialize.h>

#include <map>
#include <set>

class CKevaScript;
class CDBBatch;

typedef std::vector<unsigned char> valtype;

/** Whether or not name history is enabled.  */
extern bool fNameHistory;

/**
 * Construct a valtype (e. g., name) from a string.
 * @param str The string input.
 * @return The corresponding valtype.
 */
inline valtype
ValtypeFromString (const std::string& str)
{
  return valtype (str.begin (), str.end ());
}

/**
 * Convert a valtype to a string.
 * @param val The valtype value.
 * @return Corresponding string.
 */
inline std::string
ValtypeToString (const valtype& val)
{
  return std::string (val.begin (), val.end ());
}

/* ************************************************************************** */
/* CKevaData.  */

/**
 * Information stored for a name in the database.
 */
class CKevaData
{

private:

  /** The name's value.  */
  valtype value;

  /** The transaction's height.  Used for expiry.  */
  unsigned nHeight;

  /** The name's last update outpoint.  */
  COutPoint prevout;

  /**
   * The name's address (as script).  This is kept here also, because
   * that information is useful to extract on demand (e. g., in name_show).
   */
  CScript addr;

public:

  ADD_SERIALIZE_METHODS;

  template<typename Stream, typename Operation>
    inline void SerializationOp (Stream& s, Operation ser_action)
  {
    READWRITE (value);
    READWRITE (nHeight);
    READWRITE (prevout);
    READWRITE (*(CScriptBase*)(&addr));
  }

  /* Compare for equality.  */
  friend inline bool
  operator== (const CKevaData& a, const CKevaData& b)
  {
    return a.value == b.value && a.nHeight == b.nHeight
            && a.prevout == b.prevout && a.addr == b.addr;
  }
  friend inline bool
  operator!= (const CKevaData& a, const CKevaData& b)
  {
    return !(a == b);
  }

  /**
   * Get the height.
   * @return The name's update height.
   */
  inline unsigned
  getHeight () const
  {
    return nHeight;
  }

  /**
   * Get the value.
   * @return The name's value.
   */
  inline const valtype&
  getValue () const
  {
    return value;
  }

  /**
   * Get the name's update outpoint.
   * @return The update outpoint.
   */
  inline const COutPoint&
  getUpdateOutpoint () const
  {
    return prevout;
  }

  /**
   * Get the namespace's updated outpoint.
   */
  inline void
  setUpdateOutpoint (const COutPoint& out)
  {
    prevout = out;
  }

  /**
   * Get the address.
   * @return The name's address.
   */
  inline const CScript&
  getAddress () const
  {
    return addr;
  }

  /**
   * Check if the name is expired at the current chain height.
   * @return True iff the name is expired.
   */
  bool isExpired () const;

  /**
   * Check if the name is expired at the given height.
   * @param h The height at which to check.
   * @return True iff the name is expired at height h.
   */
  bool isExpired (unsigned h) const;

  /**
   * Set from a name update operation.
   * @param h The height (not available from script).
   * @param out The update outpoint.
   * @param script The name script.  Should be a name (first) update.
   */
  void fromScript (unsigned h, const COutPoint& out, const CKevaScript& script);

};

/* ************************************************************************** */
/* CNameHistory.  */

/**
 * Keep track of a name's history.  This is a stack of old CKevaData
 * objects that have been obsoleted.
 */
class CNameHistory
{

private:

  /** The actual data.  */
  std::vector<CKevaData> data;

public:

  ADD_SERIALIZE_METHODS;

  template<typename Stream, typename Operation>
    inline void SerializationOp (Stream& s, Operation ser_action)
  {
    READWRITE (data);
  }

  /**
   * Check if the stack is empty.  This is used to decide when to fully
   * delete an entry in the database.
   * @return True iff the data stack is empty.
   */
  inline bool
  empty () const
  {
    return data.empty ();
  }

  /**
   * Access the data in a read-only way.
   * @return The data stack.
   */
  inline const std::vector<CKevaData>&
  getData () const
  {
    return data;
  }

  /**
   * Push a new entry onto the data stack.  The new entry's height should
   * be at least as high as the stack top entry's.  If not, fail.
   * @param entry The new entry to push onto the stack.
   */
  inline void
  push (const CKevaData& entry)
  {
    assert (data.empty () || data.back ().getHeight () <= entry.getHeight ());
    data.push_back (entry);
  }

  /**
   * Pop the top entry off the stack.  This is used when undoing name
   * changes.  The name's new value is passed as argument and should
   * match the removed entry.  If not, fail.
   * @param entry The name's value after undoing.
   */
  inline void
  pop (const CKevaData& entry)
  {
    assert (!data.empty () && data.back () == entry);
    data.pop_back ();
  }

};

/* ************************************************************************** */
/* CKevaIterator.  */

/**
 * Interface for iterators over the name database.
 */
class CKevaIterator
{
protected:
  const valtype& nameSpace;

public:

  CKevaIterator(const valtype& ns) :nameSpace(ns)
  {}

  // Virtual destructor in case subclasses need them.
  virtual ~CKevaIterator();

  const valtype& getNamespace() {
    return nameSpace;
  }

  /**
   * Seek to a given lower bound.
   * @param start The key to seek to.
   */
  virtual void seek(const valtype& start) = 0;

  /**
   * Get the next key.  Returns false if no more keys are available.
   * @param nameSpace Put the namespace here.
   * @param key Put the key here.
   * @param data Put the key's data here.
   * @return True if successful, false if no more keys.
   */
  virtual bool next(valtype& key, CKevaData& data) = 0;

};

/* ************************************************************************** */
/* CKevaCache.  */

/**
 * Cache / record of updates to the name database.  In addition to
 * new names (or updates to them), this also keeps track of deleted names
 * (when rolling back changes).
 */
class CKevaCache
{

private:

  /**
   * Special comparator class for names that compares by length first.
   * This is used to sort the cache entry map in the same way as the
   * database is sorted.
   */
  class KeyComparator
  {
  public:
    inline bool operator() (const std::tuple<valtype, valtype> a,
                            const std::tuple<valtype, valtype> b) const
    {
      unsigned int aSize = std::get<0>(a).size() + std::get<1>(a).size();
      unsigned int bSize = std::get<0>(b).size() + std::get<1>(b).size();
      if (aSize != bSize) {
        return aSize < bSize;
      }
      return a < b;
    }
  };

public:

  /**
   * Type of name entry map.  This is public because it is also used
   * by the unit tests.
   */
  typedef std::map<std::tuple<valtype, valtype>, CKevaData, KeyComparator> EntryMap;

  typedef std::tuple<valtype, valtype> NamespaceKeyType;

private:

  /** New or updated names.  */
  EntryMap entries;

  /** Deleted names.  */
  std::set<NamespaceKeyType> deleted;

  friend class CCacheKeyIterator;

public:

  inline void
  clear ()
  {
    entries.clear ();
    deleted.clear ();
  }

  /**
   * Check if the cache is "clean" (no cached changes).  This also
   * performs internal checks and fails with an assertion if the
   * internal state is inconsistent.
   * @return True iff no changes are cached.
   */
  inline bool
  empty () const
  {
    if (entries.empty() && deleted.empty()) {
      return true;
    }

    return false;
  }

  /* See if the given name is marked as deleted.  */
  inline bool
  isDeleted (const valtype& nameSpace, const valtype& key) const
  {
    auto name = std::make_tuple(nameSpace, key);
    return (deleted.count(name) > 0);
  }

  /* Try to get a name's associated data.  This looks only
     in entries, and doesn't care about deleted data.  */
  bool get(const valtype& nameSpace, const valtype& key, CKevaData& data) const;

  bool GetNamespace(const valtype& nameSpace, CKevaData& data) const;

  /* Insert (or update) a name.  If it is marked as "deleted", this also
     removes the "deleted" mark.  */
  void set(const valtype& nameSpace, const valtype& key, const CKevaData& data);

  void setNamespace(const valtype& nameSpace, const CKevaData& data);

  /* Delete a name.  If it is in the "entries" set also, remove it there.  */
  void remove(const valtype& nameSpace, const valtype& key);

  /* Return a name iterator that combines a "base" iterator with the changes
     made to it according to the cache.  The base iterator is taken
     ownership of.  */
  CKevaIterator* iterateKeys(CKevaIterator* base) const;

  /* Query the cached changes to the expire index.  In particular,
     for a given height and a given set of names that were indexed to
     this update height, apply possible changes to the set that
     are represented by the cached expire index changes.  */
  void updateNamesForHeight (unsigned nHeight, std::set<valtype>& names) const;

  /* Apply all the changes in the passed-in record on top of this one.  */
  void apply (const CKevaCache& cache);

  /* Write all cached changes to a database batch update object.  */
  void writeBatch (CDBBatch& batch) const;

};

#endif // H_BITCOIN_NAMES_COMMON
