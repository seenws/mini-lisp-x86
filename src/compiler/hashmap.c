#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "hashmap.h"
#include "../util/mlispc_strdup.h"

static int hashmap_resize(struct u32hashmap *hashmap);

// creates a new hashmap with at least 16 buckets
// returns the hashmap on success, NULL on falure
struct u32hashmap *
hashmap_new(size_t initial_size)
{
    struct u32hashmap *hashmap = malloc(sizeof(*hashmap));
    if (hashmap == NULL) {
        return NULL;
    }

    if (initial_size < 16)
        hashmap->nbuckets = 16;
    else
        hashmap->nbuckets = initial_size;

    hashmap->nfilled = 0;
    hashmap->buckets = calloc(hashmap->nbuckets, sizeof(struct u32bucket *));

    if (hashmap->buckets == NULL) {
        free(hashmap);
        return NULL;
    }

    return hashmap;
}

// frees the hashmap and its contents
// does not free values (caller responsibility)
// safe to call on NULL
void
hashmap_free(struct u32hashmap *hashmap)
{
    if (hashmap == NULL)
        return;

    for (size_t i = 0; i < hashmap->nbuckets; ++i) {
        struct u32bucket *bucket = hashmap->buckets[i];

        while (bucket != NULL) {
            struct u32bucket *next = bucket->next;

            free((void *)bucket->key); // cast away const
            free(bucket);

            bucket = next;
        }
    }

    free(hashmap->buckets);
    free(hashmap);
}

// inserts or updates a key-value pair
// returns 0 on success, -1 on failure (e.g., allocation error)
int
hashmap_insert(struct u32hashmap *hashmap, const char *key, void *val)
{
    if (hashmap == NULL || key == NULL || val == NULL)
        return -1;

    if (hashmap->nfilled > hashmap->nbuckets * 0.75) {
        if (!hashmap_resize(hashmap))
            return -1;  // Resize failed, abort insert
    }

    unsigned long h = hash_djb2(key) % hashmap->nbuckets;
    struct u32bucket *bucket = hashmap->buckets[h];

    // Check for existing key and update if found
    while (bucket != NULL) {
        if (strcmp(bucket->key, key) == 0) {
            bucket->val = val;
            return 0;
        }
        bucket = bucket->next;
    }

    // Create new bucket
    bucket = malloc(sizeof(*bucket));
    if (bucket == NULL) {
        fprintf(stderr, "Unable to allocate new bucket.\n");
        return -1;
    }

    bucket->key = mlispc_strdup(key);
    if (bucket->key == NULL) {
        free(bucket);
        fprintf(stderr, "Unable to duplicate key.\n");
        return -1;
    }

    bucket->val = val;
    bucket->next = hashmap->buckets[h];
    hashmap->buckets[h] = bucket;
    hashmap->nfilled++;

    return 0;
}

// looks for the value associated with a key
// returns  1 on success (value stored in out_val)
// returns -1 on error (e.g., invalid args)
// returns  0 if not found
int
hashmap_lookup(struct u32hashmap *hashmap, const char *key, void **out_val)
{
    if (hashmap == NULL || key == NULL || out_val == NULL)
        return -1;

    *out_val = NULL; // default to not found

    unsigned long hash = hash_djb2(key) % hashmap->nbuckets;
    struct u32bucket *bucket = hashmap->buckets[hash];

    while (bucket != NULL) {
        if (strcmp(bucket->key, key) == 0) {
            *out_val = bucket->val;
            return 1;
        }

        bucket = bucket->next;
    }

    return 0; // not found is not an error
}

// removes a key-value pair if found
// returns 0 on success (removed or not found), -1 on error
int
hashmap_remove(struct u32hashmap *hashmap, const char *key)
{
    if (hashmap == NULL || key == NULL)
        return -1;

    unsigned long h = hash_djb2(key) % hashmap->nbuckets;
    struct u32bucket **prev = &hashmap->buckets[h];
    struct u32bucket *bucket = *prev;

    while (bucket != NULL) {
        if (strcmp(bucket->key, key) == 0) {
            *prev = bucket->next;

            // does not free bucket->val (called responsibility)
            free((void *)bucket->key);  // Cast away const
            free(bucket);

            hashmap->nfilled--;

            return 0;
        }
        prev = &bucket->next;
        bucket = *prev;
    }

    return 0;  // Not found is not an error
}

// resizes the hashmap to twice the buckets.
// returns 1 on success, 0 on failure.
static int
hashmap_resize(struct u32hashmap *hashmap)
{
    if (hashmap == NULL)
        return 0;

    size_t new_size = hashmap->nbuckets * 2;
    if (new_size < hashmap->nbuckets) // overflow check
        return 0;

    struct u32bucket **new_buckets = calloc(new_size, sizeof(struct u32bucket *));
    if (new_buckets == NULL)
        return 0;

    for (size_t i = 0; i < hashmap->nbuckets; ++i) {
        struct u32bucket *bucket = hashmap->buckets[i];

        while (bucket != NULL) {
            struct u32bucket *next = bucket->next;

            unsigned long hash = hash_djb2(bucket->key) % new_size;
            bucket->next = new_buckets[hash];
            new_buckets[hash] = bucket;
            bucket = next;
        }
    }

    free(hashmap->buckets);

    hashmap->buckets = new_buckets;
    hashmap->nbuckets = new_size;

    return 1;
}

unsigned long
hash_djb2(const char *str)
{
    unsigned long hash = 5381;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;

    return hash;
}
