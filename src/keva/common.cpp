// Copyright (c) 2014-2017 Daniel Kraft
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// Copyright (c) 2018 Jianping Wu
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <keva/common.h>

#include <script/keva.h>

typedef std::vector<unsigned char> valtype;

bool fNameHistory = false;

/* ************************************************************************** */
/* CKevaData.  */

void
CKevaData::fromScript (unsigned h, const COutPoint& out,
                       const CKevaScript& script)
{
  assert (script.isAnyUpdate ());
  value = script.getOpValue ();
  nHeight = h;
  prevout = out;
  addr = script.getAddress ();
}

/* ************************************************************************** */
/* CNameIterator.  */

CNameIterator::~CNameIterator ()
{
  /* Nothing to be done here.  This may be overwritten by
     subclasses if they need a destructor.  */
}

/* ************************************************************************** */
/* CKevaCacheNameIterator.  */

// JWU TODO: this doesn't work at all!!!!
class CCacheNameIterator : public CNameIterator
{

private:

  /** Reference to cache object that is used.  */
  const CKevaCache& cache;

  /** Base iterator to combine with the cache.  */
  CNameIterator* base;

  /** Whether or not the base iterator has more entries.  */
  bool baseHasMore;
  /** "Next" name of the base iterator.  */
  valtype baseName;
  /** "Next" data of the base iterator.  */
  CKevaData baseData;

  /** Iterator of the cache's entries.  */
  CKevaCache::EntryMap::const_iterator cacheIter;

  /* Call the base iterator's next() routine to fill in the internal
     "cache" for the next entry.  This already skips entries that are
     marked as deleted in the cache.  */
  void advanceBaseIterator ();

public:

  /**
   * Construct the iterator.  This takes ownership of the base iterator.
   * @param c The cache object to use.
   * @param b The base iterator.
   */
  CCacheNameIterator (const CKevaCache& c, CNameIterator* b);

  /* Destruct, this deletes also the base iterator.  */
  ~CCacheNameIterator ();

  /* Implement iterator methods.  */
  void seek (const valtype& name);
  bool next (valtype& name, CKevaData& data);

};

CCacheNameIterator::CCacheNameIterator (const CKevaCache& c, CNameIterator* b)
  : cache(c), base(b)
{
  /* Add a seek-to-start to ensure that everything is consistent.  This call
     may be superfluous if we seek to another position afterwards anyway,
     but it should also not hurt too much.  */
  seek (valtype ());
}

CCacheNameIterator::~CCacheNameIterator ()
{
  delete base;
}

void
CCacheNameIterator::advanceBaseIterator ()
{
  assert (baseHasMore);
  do
    baseHasMore = base->next (baseName, baseData);
  while (baseHasMore && cache.isDeleted(baseName, baseName));
}

void
CCacheNameIterator::seek (const valtype& start)
{
  cacheIter = cache.entries.lower_bound (start);
  base->seek (start);

  baseHasMore = true;
  advanceBaseIterator ();
}

bool
CCacheNameIterator::next (valtype& name, CKevaData& data)
{
  /* Exit early if no more data is available in either the cache
     nor the base iterator.  */
  if (!baseHasMore && cacheIter == cache.entries.end ())
    return false;

  /* Determine which source to use for the next.  */
  bool useBase;
  if (!baseHasMore)
    useBase = false;
  else if (cacheIter == cache.entries.end ())
    useBase = true;
  else
    {
      /* A special case is when both iterators are equal.  In this case,
         we want to use the cached version.  We also have to advance
         the base iterator.  */
      if (baseName == cacheIter->first)
        advanceBaseIterator ();

      /* Due to advancing the base iterator above, it may happen that
         no more base entries are present.  Handle this gracefully.  */
      if (!baseHasMore)
        useBase = false;
      else
        {
          assert (baseName != cacheIter->first);

          CKevaCache::NameComparator cmp;
          useBase = cmp (baseName, cacheIter->first);
        }
    }

  /* Use the correct source now and advance it.  */
  if (useBase)
    {
      name = baseName;
      data = baseData;
      advanceBaseIterator ();
    }
  else
    {
      name = cacheIter->first;
      data = cacheIter->second;
      ++cacheIter;
    }

  return true;
}

/* ************************************************************************** */
/* CKevaCache.  */

bool
CKevaCache::get (const valtype& nameSpace, const valtype& key, CKevaData& data) const
{
  valtype name = nameSpace;
  name.insert( name.end(), key.begin(), key.end() );
  const EntryMap::const_iterator i = entries.find (name);
  if (i == entries.end ())
    return false;

  data = i->second;
  return true;
}

void
CKevaCache::set (const valtype& nameSpace, const valtype& key, const CKevaData& data)
{
  valtype name = nameSpace;
  name.insert( name.end(), key.begin(), key.end() );
  const std::set<valtype>::iterator di = deleted.find (name);
  if (di != deleted.end ())
    deleted.erase (di);

  const EntryMap::iterator ei = entries.find (name);
  if (ei != entries.end ())
    ei->second = data;
  else
    entries.insert (std::make_pair (name, data));
}

void
CKevaCache::remove (const valtype& nameSpace, const valtype& key)
{
  valtype name = nameSpace;
  name.insert( name.end(), key.begin(), key.end() );
  const EntryMap::iterator ei = entries.find (name);
  if (ei != entries.end ())
    entries.erase (ei);

  deleted.insert (name);
}

CNameIterator*
CKevaCache::iterateNames (CNameIterator* base) const
{
  return new CCacheNameIterator (*this, base);
}

#if 0
bool
CKevaCache::getHistory (const valtype& name, CNameHistory& res) const
{
  assert (fNameHistory);

  const std::map<valtype, CNameHistory>::const_iterator i = history.find (name);
  if (i == history.end ())
    return false;

  res = i->second;
  return true;
}

void
CKevaCache::setHistory (const valtype& name, const CNameHistory& data)
{
  assert (fNameHistory);

  const std::map<valtype, CNameHistory>::iterator ei = history.find (name);
  if (ei != history.end ())
    ei->second = data;
  else
    history.insert (std::make_pair (name, data));
}
#endif

void
CKevaCache::updateNamesForHeight (unsigned nHeight,
                                  std::set<valtype>& names) const
{
  /* Seek in the map of cached entries to the first one corresponding
     to our height.  */

#if 0
  const ExpireEntry seekEntry(nHeight, valtype ());
  std::map<ExpireEntry, bool>::const_iterator it;

  for (it = expireIndex.lower_bound (seekEntry); it != expireIndex.end (); ++it)
    {
      const ExpireEntry& cur = it->first;
      assert (cur.nHeight >= nHeight);
      if (cur.nHeight > nHeight)
        break;

      if (it->second)
        names.insert (cur.name);
      else
        names.erase (cur.name);
    }
#endif
}

#if 0
void
CKevaCache::addExpireIndex (const valtype& name, unsigned height)
{
  const ExpireEntry entry(height, name);
  expireIndex[entry] = true;
}

void
CKevaCache::removeExpireIndex (const valtype& name, unsigned height)
{
  const ExpireEntry entry(height, name);
  expireIndex[entry] = false;
}
#endif

#if 0
void
CKevaCache::apply(const CKevaCache& cache)
{
  for (EntryMap::const_iterator i = cache.entries.begin (); i != cache.entries.end (); ++i) {
    set (i->first, i->second);
  }

  for (std::set<valtype>::const_iterator i = cache.deleted.begin (); i != cache.deleted.end (); ++i) {
    remove (*i);
  }

  for (std::map<valtype, CNameHistory>::const_iterator i
        = cache.history.begin (); i != cache.history.end (); ++i)
    setHistory (i->first, i->second);

  for (std::map<ExpireEntry, bool>::const_iterator i
        = cache.expireIndex.begin (); i != cache.expireIndex.end (); ++i)
    expireIndex[i->first] = i->second;
}
#endif
