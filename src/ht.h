#ifndef HT_H_
#define HT_H_

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include "da.h"

typedef struct HT__Bucket HT__Bucket;
struct HT__Bucket {
    HT__Bucket *next;    
    StringView key;
    size_t val;
};

typedef struct {
    size_t capacity;
    size_t count;
    HT__Bucket **items;
} HashTable;

int ht_insert(HashTable *ht, StringView key, size_t val);
int ht_lookup(HashTable *ht, StringView key, size_t *val);
int ht_remove(HashTable *ht, StringView key);
void ht_free(HashTable *ht);

#ifdef HT_IMPLEMENTATION

HT__Bucket* ht__new_bucket(StringView key, size_t val)
{
    HT__Bucket *b = malloc(sizeof(*b));
    if(!b) return NULL;

    b->next = NULL;
    b->key = key;
    b->val = val;

    return b;
}

void ht__bucket_free(HT__Bucket *b)
{
    free(b);
}

uint64_t ht__hash(StringView key)
{
    const uint64_t magic_prime = 0x00000100000001b3;
    uint64_t hash = 0xcbf29ce484222325;

    for(size_t i = 0; i < key.len; ++i)
    {
        hash = (hash ^ (unsigned char) key.s[i]) * magic_prime;
    }

    return hash;
}

int ht_insert(HashTable *ht, StringView key, size_t val)
{
    assert(ht);
    // TODO resize
    if(0 == ht->capacity)
    {
        ht->capacity = 1024;
        ht->items = calloc(ht->capacity, sizeof(*ht->items));
        if(!ht->items) return 1;
    }


    const uint64_t hash = ht__hash(key) % ht->capacity;

    HT__Bucket *prev = NULL;
    for(HT__Bucket *b = ht->items[hash]; b; b = b->next)
    {
        if(sv_cmp(b->key, key) == 0)
        {
            b->val = val;
            return 0;
        }
        prev = b;
    }

    HT__Bucket *b = ht__new_bucket(key, val);
    if(!b) return 1;

    if(prev)
    {
        prev->next = b;
    }
    else
    {
        ht->items[hash] = b;
    }

    ht->count++;
    return 0;
}

int ht_lookup(HashTable *ht, StringView key, size_t *val)
{
    assert(ht);
    assert(val);
    if(0 == ht->capacity) return 1;

    const uint64_t hash = ht__hash(key) % ht->capacity;

    for(HT__Bucket *b = ht->items[hash]; b; b = b->next)
    {
        if(sv_cmp(b->key, key) == 0)
        {
            *val = b->val;
            return 0;
        }
    }

    return 1;
}

int ht_remove(HashTable *ht, StringView key)
{
    assert(ht);
    if(0 == ht->capacity) return 1;

    const uint64_t hash = ht__hash(key) % ht->capacity;

    HT__Bucket *prev = NULL;
    for(HT__Bucket *b = ht->items[hash]; b; b = b->next)
    {
        if(sv_cmp(b->key, key) == 0)
        {
            if(prev)
            {
                prev->next = b->next;
            }
            else
            {
                ht->items[hash] = b->next;
            }

            ht->count--;
            ht__bucket_free(b);
            return 0;
        }
        prev = b;
    }

    return 1;
}

void ht_free(HashTable *ht)
{
    // TODO this could benefit from an arena?
    assert(ht);

    for(size_t i = 0; i < ht->capacity; ++i)
    {
        HT__Bucket *b = ht->items[i];
        while(b)
        {
            HT__Bucket *n = b->next;
            ht__bucket_free(b);
            b = n;
        }
    }

    free(ht->items);
}

#endif // HT_IMPLEMENTATION

#endif // HT_H_
