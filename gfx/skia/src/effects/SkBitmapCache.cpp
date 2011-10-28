
/*
 * Copyright 2010 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#include "SkBitmapCache.h"

struct SkBitmapCache::Entry {
    Entry*      fPrev;
    Entry*      fNext;

    void*       fBuffer;
    size_t      fSize;
    SkBitmap    fBitmap;

    Entry(const void* buffer, size_t size, const SkBitmap& bm)
            : fPrev(NULL),
              fNext(NULL),
              fBitmap(bm) {
        fBuffer = sk_malloc_throw(size);
        fSize = size;
        memcpy(fBuffer, buffer, size);
    }

    ~Entry() { sk_free(fBuffer); }

    bool equals(const void* buffer, size_t size) const {
        return (fSize == size) && !memcmp(fBuffer, buffer, size);
    }
};

SkBitmapCache::SkBitmapCache(int max) : fMaxEntries(max) {
    fEntryCount = 0;
    fHead = fTail = NULL;

    this->validate();
}

SkBitmapCache::~SkBitmapCache() {
    this->validate();

    Entry* entry = fHead;
    while (entry) {
        Entry* next = entry->fNext;
        delete entry;
        entry = next;
    }
}

SkBitmapCache::Entry* SkBitmapCache::detach(Entry* entry) const {
    if (entry->fPrev) {
        SkASSERT(fHead != entry);
        entry->fPrev->fNext = entry->fNext;
    } else {
        SkASSERT(fHead == entry);
        fHead = entry->fNext;
    }
    if (entry->fNext) {
        SkASSERT(fTail != entry);
        entry->fNext->fPrev = entry->fPrev;
    } else {
        SkASSERT(fTail == entry);
        fTail = entry->fPrev;
    }
    return entry;
}

void SkBitmapCache::attachToHead(Entry* entry) const {
    entry->fPrev = NULL;
    entry->fNext = fHead;
    if (fHead) {
        fHead->fPrev = entry;
    } else {
        fTail = entry;
    }
    fHead = entry;
}

bool SkBitmapCache::find(const void* buffer, size_t size, SkBitmap* bm) const {
    AutoValidate av(this);

    Entry* entry = fHead;
    while (entry) {
        if (entry->equals(buffer, size)) {
            if (bm) {
                *bm = entry->fBitmap;
            }
            // move to the head of our list, so we purge it last
            this->detach(entry);
            this->attachToHead(entry);
            return true;
        }
        entry = entry->fNext;
    }
    return false;
}

void SkBitmapCache::add(const void* buffer, size_t len, const SkBitmap& bm) {
    AutoValidate av(this);

    if (fEntryCount == fMaxEntries) {
        SkASSERT(fTail);
        delete this->detach(fTail);
        fEntryCount -= 1;
    }

    Entry* entry = new Entry(buffer, len, bm);
    this->attachToHead(entry);
    fEntryCount += 1;
}

///////////////////////////////////////////////////////////////////////////////

#ifdef SK_DEBUG

void SkBitmapCache::validate() const {
    SkASSERT(fEntryCount >= 0 && fEntryCount <= fMaxEntries);

    if (fEntryCount > 0) {
        SkASSERT(NULL == fHead->fPrev);
        SkASSERT(NULL == fTail->fNext);

        if (fEntryCount == 1) {
            SkASSERT(fHead == fTail);
        } else {
            SkASSERT(fHead != fTail);
        }

        Entry* entry = fHead;
        int count = 0;
        while (entry) {
            count += 1;
            entry = entry->fNext;
        }
        SkASSERT(count == fEntryCount);

        entry = fTail;
        while (entry) {
            count -= 1;
            entry = entry->fPrev;
        }
        SkASSERT(0 == count);
    } else {
        SkASSERT(NULL == fHead);
        SkASSERT(NULL == fTail);
    }
}

#endif

