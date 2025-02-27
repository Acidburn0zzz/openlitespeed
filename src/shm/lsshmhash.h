/*****************************************************************************
*    Open LiteSpeed is an open source HTTP server.                           *
*    Copyright (C) 2013 - 2015  LiteSpeed Technologies, Inc.                 *
*                                                                            *
*    This program is free software: you can redistribute it and/or modify    *
*    it under the terms of the GNU General Public License as published by    *
*    the Free Software Foundation, either version 3 of the License, or       *
*    (at your option) any later version.                                     *
*                                                                            *
*    This program is distributed in the hope that it will be useful,         *
*    but WITHOUT ANY WARRANTY; without even the implied warranty of          *
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the            *
*    GNU General Public License for more details.                            *
*                                                                            *
*    You should have received a copy of the GNU General Public License       *
*    along with this program. If not, see http://www.gnu.org/licenses/.      *
*****************************************************************************/
#ifndef LSSHMHASH_H
#define LSSHMHASH_H

#ifdef LSSHM_DEBUG_ENABLE
class debugBase;
#endif

#include <lsdef.h>
#include <lsr/ls_str.h>
#include <shm/lsshm.h>
#include <shm/lsshmpool.h>

#define LSSHM_LRU_NONE      0
#define LSSHM_LRU           1

/**
 * @file
 *  HASH element
 *
 *  +--------------------
 *  | Element info
 *  | hkey
 *  +--------------------  <--- start of variable data
 *  | key len
 *  | key
 *  +--------------------
 *  | LRU info (optional)
 *  +--------------------  <--- value offset
 *  | value len
 *  | value
 *  +--------------------
 *
 *  It is reasonable to support max 32k key string.
 *  Otherwise we should change to uint32_t
 */


typedef uint32_t     LsShmHKey;
typedef int32_t      LsShmHElemLen_t;
typedef uint32_t     LsShmHElemOffs_t;


typedef struct
{
    LsShmOffset_t        x_iLinkNext;     // offset to next in linked list
    LsShmOffset_t        x_iLinkPrev;     // offset to prev in linked list
    time_t               x_lasttime;      // last update time (sec)
} LsShmHElemLink;

typedef struct ls_vardata_s
{
    int32_t         x_size;
    uint8_t         x_data[0];
} ls_vardata_t;

typedef struct lsShm_hElem_s
{
    LsShmHElemLen_t      x_iLen;          // element size
    LsShmHElemOffs_t     x_iValOff;
    LsShmOffset_t        x_iNext;         // offset to next in element list
    LsShmHKey            x_hkey;          // the key itself
    uint32_t             x_aData[0];

    uint8_t         *getKey() const
    { return ((ls_vardata_t *)x_aData)->x_data; }
    int32_t          getKeyLen() const
    { return ((ls_vardata_t *)x_aData)->x_size; }
    uint8_t         *getVal() const
    { return ((ls_vardata_t *)((uint8_t*)x_aData + x_iValOff))->x_data; }
    int32_t          getValLen() const
    { return ((ls_vardata_t *)((uint8_t*)x_aData + x_iValOff))->x_size; }
    void             setKeyLen(int32_t len)
    { ((ls_vardata_t *)x_aData)->x_size = len; }
    void             setValLen(int32_t len)
    { ((ls_vardata_t *)((uint8_t*)x_aData + x_iValOff))->x_size = len; }

    LsShmHElemLink  *getLruLinkPtr() const
    { return ((LsShmHElemLink *)((uint8_t*)x_aData + x_iValOff) - 1); }
    LsShmOffset_t    getLruLinkNext() const
    { return getLruLinkPtr()->x_iLinkNext; }
    LsShmOffset_t    getLruLinkPrev() const
    { return getLruLinkPtr()->x_iLinkPrev; }
    time_t           getLruLasttime() const
    { return getLruLinkPtr()->x_lasttime; }
    void             setLruLinkNext(LsShmOffset_t off)
    { getLruLinkPtr()->x_iLinkNext = off; }
    void             setLruLinkPrev(LsShmOffset_t off)
    { getLruLinkPtr()->x_iLinkPrev = off; }

    const uint8_t   *first() const   { return getKey(); }
    uint8_t         *second() const  { return getVal(); }

    // real value len
    LsShmSize_t      realValLen() const
    {
        return (x_iLen - sizeof(struct lsShm_hElem_s) - x_iValOff - sizeof(
                    ls_vardata_t));
    }
} LsShmHElem;

typedef struct
{
    LsShmOffset_t   x_iOffset;   // offset location of hash element
} LsShmHIdx;

typedef struct
{
    LsShmXSize_t    m_iHashInUse;   // shm allocated to hash (bytes)
} LsShmHTableStat;

typedef struct ls_shmhtable_s LsShmHTable;


class LsShmHashLruAddon
{

public:
    LsShmHashLruAddon() {}
    virtual ~LsShmHashLruAddon() {}

    virtual int setLruData(LsShmOffset_t offVal, const uint8_t *pVal, int valLen);

    virtual int getLruData(LsShmOffset_t offVal, LsShmOffset_t *pData, int cnt);

    virtual int getLruDataPtrs(LsShmOffset_t offVal, int (*func)(void *pData));

    virtual int clrdata(uint8_t *pValue);
    virtual int chkdata(uint8_t *pValue);
    
};


class AutoBuf;
class LsShmHash : public ls_shmhash_s
{
public:
    typedef LsShmHElem *iterator;
    typedef const LsShmHElem *const_iterator;
    typedef LsShmOffset_t iteroffset;

    typedef int (*for_each_fn)(iteroffset iterOff);
    typedef int (*for_each_fn2)(iteroffset iterOff, void *pUData);

    static LsShmHKey hashString(const void *__s, size_t len);
    static int compString(const void *pVal1, const void *pVal2, size_t len);

    static LsShmHKey hash32id(const void *__s, size_t len);
    static LsShmHKey hashBuf(const void *__s, size_t len);
    static LsShmHKey hashXXH32(const void *__s, size_t len);

    static LsShmHKey iHashString(const void *__s, size_t len);
    static int iCompString(const void *pVal1, const void *pVal2, size_t len);

    static int cmpIpv6(const void *pVal1, const void *pVal2, size_t len);
    static LsShmHKey hfIpv6(const void *pKey, size_t len);

    LsShmStatus_t status()
    { return m_status; }

public:
    LsShmHash(LsShmPool *pool,
              const char *name, LsShmHasher_fn hf, LsShmValComp_fn vc, int lru_mode);
    virtual ~LsShmHash();

    static LsShmHash *open(
        const char *pShmName, const char *pHashName, int init_size, int lruMode);

    static LsShmOffset_t allocHTable(LsShmPool * pPool, int init_size, 
                                     int iMode, int iLruMode, 
                                     LsShmOffset_t lockOffset);

    //int init(size_t init_size);
    int init(LsShmOffset_t offset);

    static LsShmSize_t roundUp(LsShmSize_t sz);
    
    static int sz2TableSz(LsShmSize_t sz)
    {   return (sz * sizeof(LsShmHIdx)); }


    static LsShmHash *checkHTable(GHash::iterator itor, LsShmPool *pool,
                                  const char *name, LsShmHasher_fn hf, LsShmValComp_fn vc);
    
    static int chkHashTable(LsShm *pShm, LsShmReg *pReg, int *pMode, int *pLruMode);


    ls_attr_inline LsShmPool *getPool() const
    {   return m_pPool;     }

    ls_attr_inline LsShmOffset_t ptr2offset(const void *ptr) const
    {   return m_pPool->ptr2offset(ptr); }

    ls_attr_inline void *offset2ptr(LsShmOffset_t offset) const
    {   return m_pPool->offset2ptr(offset); }

    ls_attr_inline iterator offset2iterator(iteroffset offset) const
    {   return (iterator)m_pPool->offset2ptr(offset); }

    ls_attr_inline void *offset2iteratorData(iteroffset offset) const
    {   return ((iterator)m_pPool->offset2ptr(offset))->getVal(); }

    LsShmOffset_t alloc2(LsShmSize_t size, int &remapped);

    void release2(LsShmOffset_t offset, LsShmSize_t size);

    LsShmOffset_t getHTableStatOffset() const;

    LsShmOffset_t getHTableReservedOffset() const;

    int round4(int x) const
    {   return (x + 0x3) & ~0x3; }

    void *getIterDataPtr(iterator iter) const
    {   return iter->getVal(); }

    const char *name() const
    {   return obj.m_pName; }

    void close();

    void clear();

    ls_strpair_t *setParms(ls_strpair_t *pParms,
                            const void *pKey, int keyLen, const void *pValue, int valueLen)
    {
        ls_str_set(&pParms->key, (char *)pKey, keyLen);
        ls_str_set(&pParms->value, (char *)pValue, valueLen);
        return pParms;
    }

    void remove(const void *pKey, int keyLen)
    {
        iteroffset iterOff;
        ls_strpair_t parms;
        ls_str_set(&parms.key, (char *)pKey, keyLen);
        if ((iterOff = findIterator(&parms)) != 0)
            eraseIterator(iterOff);
    }

    LsShmOffset_t find(const void *pKey, int keyLen, int *valLen)
    {
        iteroffset iterOff;
        ls_strpair_t parms;
        ls_str_set(&parms.key, (char *)pKey, keyLen);
        if ((iterOff = findIterator(&parms)) == 0)
        {
            *valLen = 0;
            return 0;
        }
        iterator iter = offset2iterator(iterOff);
        *valLen = iter->getValLen();
        return ptr2offset(iter->getVal());
    }

    LsShmOffset_t get(
        const void *pKey, int keyLen, int *valLen, int *pFlag)
    {
        iteroffset iterOff;
        ls_strpair_t parms;
        if ((iterOff = getIterator(
                           setParms(&parms, pKey, keyLen, NULL, *valLen), pFlag)) == 0)
        {
            *valLen = 0;
            return 0;
        }
        iterator iter = offset2iterator(iterOff);
        *valLen = iter->getValLen();
        return ptr2offset(iter->getVal());
    }

    LsShmOffset_t insert(
        const void *pKey, int keyLen, const void *pValue, int valueLen)
    {
        ls_strpair_t parms;
        iteroffset iterOff = insertIterator(
                                 setParms(&parms, pKey, keyLen, pValue, valueLen));
        return (iterOff == 0) ?
               0 : ptr2offset(offset2iteratorData(iterOff));
    }

    LsShmOffset_t set(
        const void *pKey, int keyLen, const void *pValue, int valueLen)
    {
        ls_strpair_t parms;
        iteroffset iterOff = setIterator(
                                 setParms(&parms, pKey, keyLen, pValue, valueLen));
        return (iterOff == 0) ?
               0 : ptr2offset(offset2iteratorData(iterOff));
    }

    LsShmOffset_t update(
        const void *pKey, int keyLen, const void *pValue, int valueLen)
    {
        ls_strpair_t parms;
        iteroffset iterOff = updateIterator(
                                 setParms(&parms, pKey, keyLen, pValue, valueLen));
        return (iterOff == 0) ?
               0 : ptr2offset(offset2iteratorData(iterOff));
    }

    int touchLru(iteroffset iterOff);
    
    int linkSetTopTime(LsShmOffset_t offset, time_t lasttime);
    
    int linkMvTopTime(LsShmOffset_t offset, time_t lasttime);

    void eraseIterator(iteroffset iterOff)
    {
        autoLockChkRehash();
        eraseIteratorHelper(offset2iterator(iterOff));
        autoUnlock();
    }

    //
    //  Note - iterators should not be saved.
    //         use ptr2offset(iterator) to save the offset
    //
    iteroffset findIterator(ls_strpair_t *pParms)
    {
        autoLockChkRehash();
        iteroffset iterOff = (*m_find)(this, pParms);
        autoUnlock();
        return iterOff;
    }

    iteroffset getIterator(ls_strpair_t *pParms, int *pFlag)
    {
        autoLockChkRehash();
        iteroffset iterOff = (*m_get)(this, pParms, pFlag);
        autoUnlock();
        return iterOff;
    }

    iteroffset insertIterator(ls_strpair_t *pParms)
    {
        autoLockChkRehash();
        iteroffset iterOff = (*m_insert)(this, pParms);
        autoUnlock();
        return iterOff;
    }

    iteroffset setIterator(ls_strpair_t *pParms)
    {
        autoLockChkRehash();
        iteroffset iterOff = (*m_set)(this, pParms);
        autoUnlock();
        return iterOff;
    }

    iteroffset updateIterator(ls_strpair_t *pParms)
    {
        autoLockChkRehash();
        iteroffset iterOff = (*m_update)(this, pParms);
        autoUnlock();
        return iterOff;
    }

    iteroffset nextLruIterOff(iteroffset iterOff)
    {
        if (m_iLruMode == LSSHM_LRU_NONE)
            return 0;
        return ((iterOff == 0) ?
            getLru()->linkLast : (offset2iterator(iterOff))->getLruLinkNext());
    }

    iteroffset prevLruIterOff(iteroffset iterOff)
    {
        if (m_iLruMode == LSSHM_LRU_NONE)
            return 0;
        return ((iterOff == 0) ?
            getLru()->linkFirst : (offset2iterator(iterOff))->getLruLinkPrev());
    }

    iteroffset nextTmLruIterOff(time_t tmCutoff);
    iteroffset prevTmLruIterOff(time_t tmCutoff);

    iterator nextLruIterator(iterator iter)
    {
        if (m_iLruMode == LSSHM_LRU_NONE)
            return NULL;
        return offset2iterator((iter == NULL) ?
            getLru()->linkLast : iter->getLruLinkNext());
    }

    iterator prevLruIterator(iterator iter)
    {
        if (m_iLruMode == LSSHM_LRU_NONE)
            return NULL;
        return offset2iterator((iter == NULL) ?
            getLru()->linkFirst : iter->getLruLinkPrev());
    }

    LsShmHasher_fn getHashFn() const   {   return m_hf;    }
    LsShmValComp_fn getValComp() const {   return m_vc;    }

    void setFullFactor(int f);
    void setGrowFactor(int f);

    bool empty() const;
    LsShmSize_t size() const;
    LsShmSize_t capacity() const;

    void incrTableSize();
    void decrTableSize();


    iteroffset begin();
    iteroffset end()                {   return 0;   }
    iteroffset next(iteroffset iterOff);
    int for_each(iteroffset beg, iteroffset end, for_each_fn fun);
    int for_each2(iteroffset beg, iteroffset end, for_each_fn2 fun,
                  void *pUData);

    void destroy();

    //  @brief stat
    //  @brief gether statistic on HashTable
    int stat(LsHashStat *pHashStat, for_each_fn2 fun, void *pData);

    // LRU stuff
    LsHashLruInfo *getLru() const;

    LsShmOffset_t getLruTop()
    {
        return ((m_iLruMode != LSSHM_LRU_NONE) ?
                getLru()->linkFirst : (LsShmOffset_t) - 1);
    }

    int trim(time_t tmCutoff, int (*func)(iterator iter, void *arg),
             void *arg);
    int check();

    void enableLock()
    {   m_iLockEnable = 1; };

    void disableLock()
    {   m_iLockEnable = 0; };

    int lock()
    {
        if (m_iLockEnable != 0)
            return 0;
        return getPool()->getShm()->lockRemap(m_pShmLock);
    }

    int unlock()
    {   return m_iLockEnable ? 0 : ls_shmlock_unlock(m_pShmLock); }

    void lockChkRehash();

    int getRef()     { return m_iRef; }
    int upRef()      { return ++m_iRef; }
    int downRef()    { return --m_iRef; }

protected:
    typedef iteroffset(*hash_find)(LsShmHash *pThis, ls_strpair_t *pParms);
    typedef iteroffset(*hash_get)(LsShmHash *pThis, ls_strpair_t *pParms,
                                  int *pFlag);
    typedef iteroffset(*hash_insert)(LsShmHash *pThis, ls_strpair_t *pParms);
    typedef iteroffset(*hash_set)(LsShmHash *pThis, ls_strpair_t *pParms);
    typedef iteroffset(*hash_update)(LsShmHash *pThis, ls_strpair_t *pParms);

    uint32_t getIndex(uint32_t k, uint32_t n)
    {   return k % n ; }

    ls_attr_inline LsShmHTable *getHTable() const
    {   return (LsShmHTable *)m_pPool->offset2ptr(m_iOffset);   }

    LsShmSize_t fullFactor() const;
    LsShmSize_t growFactor() const;

    ls_attr_inline LsShmHIdx *getHIdx() const;

    ls_attr_inline uint8_t *getBitMap() const;
    int getBitMapEnt(uint32_t indx);
    void setBitMapEnt(uint32_t indx);
    void clrBitMapEnt(uint32_t indx);

    int         rehash();
    iteroffset  find2(LsShmHKey key, ls_strpair_t *pParms);
    iteroffset  insert2(LsShmHKey key, ls_strpair_t *pParms);

    static iteroffset findNum(LsShmHash *pThis, ls_strpair_t *pParms);
    static iteroffset getNum(LsShmHash *pThis, ls_strpair_t *pParms,
                             int *pFlag);
    static iteroffset insertNum(LsShmHash *pThis, ls_strpair_t *pParms);
    static iteroffset setNum(LsShmHash *pThis, ls_strpair_t *pParms);
    static iteroffset updateNum(LsShmHash *pThis, ls_strpair_t *pParms);

    static iteroffset findPtr(LsShmHash *pThis, ls_strpair_t *pParms);
    static iteroffset getPtr(LsShmHash *pThis, ls_strpair_t *pParms,
                             int *pFlag);
    static iteroffset insertPtr(LsShmHash *pThis, ls_strpair_t *pParms);
    static iteroffset setPtr(LsShmHash *pThis, ls_strpair_t *pParms);
    static iteroffset updatePtr(LsShmHash *pThis, ls_strpair_t *pParms);

    //
    //  @brief eraseIterator_helper
    //  @brief  should only be called after SHM-HASH-LOCK has been acquired.
    //
    void eraseIteratorHelper(iterator iter);

    iteroffset doGet(iteroffset iterOff, LsShmHKey key, ls_strpair_t *pParms,
                     int *pFlag);
    iteroffset doInsert(iteroffset iterOff, LsShmHKey key, ls_strpair_t *pParms);
    iteroffset doSet(iteroffset iterOff, LsShmHKey key, ls_strpair_t *pParms);

    iteroffset doUpdate(iteroffset iterOff, LsShmHKey key, ls_strpair_t *pParms);

#ifdef notdef
    static iteroffset doExpand(LsShmHash *pThis,
                               iteroffset iterOff, LsShmHKey key, ls_strpair_t *pParms, uint16_t flags);
#endif

    void setIterData(iterator iter, const void *pValue)
    {
        if (pValue != NULL)
            ::memcpy(iter->getVal(), pValue, iter->getValLen());
    }

    void setIterKey(iterator iter, const void *pKey)
    {   ::memcpy(iter->getKey(), pKey, iter->getKeyLen()); }

    static int release_hash_elem(iteroffset iterOff, void *pUData);

    int autoLock()
    {
        if (m_iLockEnable == 0)
            return 0;
        return getPool()->getShm()->lockRemap(m_pShmLock);
    }

    int autoUnlock()
    {   return m_iLockEnable && ls_shmlock_unlock(m_pShmLock); }

    void autoLockChkRehash();

    // stat helper
    int statIdx(iteroffset iterOff, for_each_fn2 fun, void *pUData);

    int setupLock()
    {   return m_iLockEnable && ls_shmlock_setup(m_pShmLock); }

    // auxiliary double linked list of hash elements
    void set_linkNext(LsShmOffset_t offThis, LsShmOffset_t offNext)
    {
        ((LsShmHElem *)offset2ptr(offThis))->setLruLinkNext(offNext);
    }

    void set_linkPrev(LsShmOffset_t offThis, LsShmOffset_t offPrev)
    {
        ((LsShmHElem *)offset2ptr(offThis))->setLruLinkPrev(offPrev);
    }

    void linkHElem(LsShmHElem *pElem, LsShmOffset_t offElem);

    void unlinkHElem(LsShmHElem *pElem);

    void linkSetTop(LsShmHElem *pElem);
    
    int8_t addExtraSpace(uint8_t iterExtra, uint8_t dataExtra)
    {
        m_dataExtraSpace += dataExtra;
        dataExtra = m_iterExtraSpace;
        m_iterExtraSpace += iterExtra;
        return dataExtra;
    }

protected:
    uint32_t            m_iMagic;
    LsShmPool          *m_pPool;
    LsShmOffset_t       m_iOffset;

    LsShmHasher_fn      m_hf;
    LsShmValComp_fn     m_vc;
    hash_insert         m_insert;
    hash_update         m_update;
    hash_set            m_set;
    hash_find           m_find;
    hash_get            m_get;

    uint8_t             m_iterExtraSpace;
    uint8_t             m_dataExtraSpace;
    int8_t              m_iLockEnable;
    int8_t              m_iMode;        // mode 0=Num, 1=Ptr
    uint8_t             m_iLruMode;     // lru=1, wlru=2, xlru=3
    ls_shmlock_t       *m_pShmLock;     // local lock for Hash
    LsShmStatus_t       m_status;
    LsShmHashLruAddon  *m_pLruAddon;
    // house keeping
    int m_iRef;


private:
    // disable the bad boys!
    LsShmHash(const LsShmHash &other);
    LsShmHash &operator=(const LsShmHash &other);

    void releaseHTableShm();

#ifdef LSSHM_DEBUG_ENABLE
    // for debug purpose - should debug this later
    friend class debugBase;
#endif
};


template< class T >
class TShmHash
    : public LsShmHash
{
private:
    TShmHash() {};
    TShmHash(const TShmHash &rhs);
    ~TShmHash() {};
    void operator=(const TShmHash &rhs);
public:
    class iterator
    {
        LsShmHash::iterator m_iter;
    public:
        iterator()
        {}

        iterator(LsShmHash::iterator iter) : m_iter(iter)
        {}
        iterator(LsShmHash::const_iterator iter)
            : m_iter((LsShmHash::iterator)iter)
        {}

        iterator(const iterator &rhs) : m_iter(rhs.m_iter)
        {}

        const void *first() const
        {  return  m_iter->first();   }

        T *second() const
        {   return (T *)(m_iter->second());   }

        operator LsShmHash::iterator()
        {   return m_iter;  }
    };
    typedef iterator const_iterator;

    LsShmOffset_t get(const void *pKey, int keyLen, int *valueLen, int *pFlag)
    {
        *valueLen = sizeof(T);
        return LsShmHash::get(pKey, keyLen, valueLen, pFlag);
    }

    LsShmOffset_t set(const void *pKey, int keyLen, T *pValue)
    {   return LsShmHash::set(pKey, keyLen, (const void *)pValue, sizeof(T));  }

    LsShmOffset_t insert(const void *pKey, int keyLen, T *pValue)
    {   return LsShmHash::insert(pKey, keyLen, (const void *)pValue, sizeof(T));  }

    LsShmOffset_t update(const void *pKey, int keyLen, T *pValue)
    {   return LsShmHash::update(pKey, keyLen, (const void *)pValue, sizeof(T));  }

    iteroffset begin()
    {   return LsShmHash::begin();  }

};

#endif // LSSHMHASH_H

