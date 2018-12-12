// Copyright (c) 2014-2017 Daniel Kraft
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// Copyright (c) 2018 the Kevacoin Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <keva/common.h>

#include <script/keva.h>


/* ************************************************************************** */
/* CKevaData.  */

void
CKevaData::fromScript (unsigned h, const COutPoint& out,
                       const CKevaScript& script)
{
  if (script.isAnyUpdate()) {
    if (!script.isDelete()) {
      value = script.getOpValue();
    }
  } else if (script.isNamespaceRegistration()) {
    value = script.getOpNamespaceDisplayName();
  } else {
    assert(false);
  }
  nHeight = h;
  prevout = out;
  addr = script.getAddress();
}

/* ************************************************************************** */
/* CKevaIterator.  */

CKevaIterator::~CKevaIterator ()
{
  /* Nothing to be done here.  This may be overwritten by
     subclasses if they need a destructor.  */
}

/* ************************************************************************** */
/* CKevaCacheNameIterator.  */

class CCacheKeyIterator : public CKevaIterator
{

private:

  /** Reference to cache object that is used.  */
  const CKevaCache& cache;

  /** Base iterator to combine with the cache.  */
  CKevaIterator* base;

  /** Whether or not the base iterator has more entries.  */
  bool baseHasMore;

  /** "Next" key of the base iterator.  */
  valtype baseKey;

  /** "Next" data of the base iterator.  */
  CKevaData baseData;

  /** Iterator of the cache's entries.  */
  CKevaCache::EntryMap::const_iterator cacheIter;

  /* Call the base iterator's next() routine to fill in the internal
     "cache" for the next entry.  This already skips entries that are
     marked as deleted in the cache.  */
  void advanceBaseIterator();

public:

  /**
   * Construct the iterator.  This takes ownership of the base iterator.
   * @param c The cache object to use.
   * @param b The base iterator.
   */
  CCacheKeyIterator(const CKevaCache& c, CKevaIterator* b);

  /* Destruct, this deletes also the base iterator.  */
  ~CCacheKeyIterator();

  /* Implement iterator methods.  */
  void seek(const valtype& key);
  bool next(valtype& key, CKevaData& data);

};

CCacheKeyIterator::CCacheKeyIterator(const CKevaCache& c, CKevaIterator* b)
  : cache(c), base(b), CKevaIterator(b->getNamespace())
{
  /* Add a seek-to-start to ensure that everything is consistent.  This call
     may be superfluous if we seek to another position afterwards anyway,
     but it should also not hurt too much.  */
  seek(valtype());
}

CCacheKeyIterator::~CCacheKeyIterator()
{
  delete base;
}

void
CCacheKeyIterator::advanceBaseIterator()
{
  assert (baseHasMore);
  do {
    baseHasMore = base->next(baseKey, baseData);
  } while (baseHasMore && cache.isDeleted(nameSpace, baseKey));
}

void
CCacheKeyIterator::seek(const valtype& start)
{
  cacheIter = cache.entries.lower_bound(std::make_tuple(nameSpace, start));
  base->seek(start);

  baseHasMore = true;
  advanceBaseIterator();
}

bool CCacheKeyIterator::next(valtype& key, CKevaData& data)
{
  /* Exit early if no more data is available in either the cache
     nor the base iterator.  */
  if (!baseHasMore && cacheIter == cache.entries.end())
    return false;

  /* Determine which source to use for the next.  */
  bool useBase;
  if (!baseHasMore)
    useBase = false;
  else if (cacheIter == cache.entries.end())
    useBase = true;
  else {
    /* A special case is when both iterators are equal.  In this case,
        we want to use the cached version.  We also have to advance
        the base iterator.  */
    if (baseKey == std::get<1>(cacheIter->first))
      advanceBaseIterator ();

    /* Due to advancing the base iterator above, it may happen that
        no more base entries are present.  Handle this gracefully.  */
    if (!baseHasMore)
      useBase = false;
    else {
      assert(baseKey != std::get<1>(cacheIter->first));

      CKevaCache::KeyComparator cmp;
      useBase = cmp(std::make_tuple(nameSpace, baseKey), cacheIter->first);
    }
  }

  /* Use the correct source now and advance it.  */
  if (useBase) {
    key = baseKey;
    data = baseData;
    advanceBaseIterator();
  } else {
    key = std::get<1>(cacheIter->first);
    data = cacheIter->second;
    ++cacheIter;
  }
  return true;
}

/* ************************************************************************** */
/* CKevaCache.  */

bool
CKevaCache::get(const valtype& nameSpace, const valtype& key, CKevaData& data) const
{
  const EntryMap::const_iterator i = entries.find(std::make_tuple(nameSpace, key));
  if (i == entries.end ())
    return false;

  data = i->second;
  return true;
}

bool
CKevaCache::GetNamespace(const valtype& nameSpace, CKevaData& data) const
{
  return get(nameSpace, ValtypeFromString(CKevaScript::KEVA_DISPLAY_NAME_KEY), data);
}

void
CKevaCache::setNamespace(const valtype& nameSpace, const CKevaData& data)
{
  set(nameSpace, ValtypeFromString(CKevaScript::KEVA_DISPLAY_NAME_KEY), data);
}

void
CKevaCache::set(const valtype& nameSpace, const valtype& key, const CKevaData& data)
{
  auto name = std::make_tuple(nameSpace, key);
  const std::set<NamespaceKeyType>::iterator di = deleted.find(name);
  if (di != deleted.end()) {
    deleted.erase(di);
  }

  const EntryMap::iterator ei = entries.find(name);
  if (ei != entries.end())
    ei->second = data;
  else
    entries.insert(std::make_pair(name, data));
}

void
CKevaCache::remove(const valtype& nameSpace, const valtype& key)
{
  auto name = std::make_tuple(nameSpace, key);
  const EntryMap::iterator ei = entries.find(name);
  if (ei != entries.end())
    entries.erase(ei);

  deleted.insert(name);
}

CKevaIterator*
CKevaCache::iterateKeys(CKevaIterator* base) const
{
  return new CCacheKeyIterator(*this, base);
}

void
CKevaCache::updateNamesForHeight (unsigned nHeight,
                                  std::set<valtype>& names) const
{
  /* Seek in the map of cached entries to the first one corresponding
     to our height.  */
}

void CKevaCache::apply(const CKevaCache& cache)
{
  for (EntryMap::const_iterator i = cache.entries.begin(); i != cache.entries.end(); ++i) {
    set(std::get<0>(i->first), std::get<1>(i->first), i->second);
  }

  for (std::set<NamespaceKeyType>::const_iterator i = cache.deleted.begin(); i != cache.deleted.end(); ++i) {
    remove(std::get<0>(*i), std::get<1>(*i));
  }
}
