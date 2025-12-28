/*
    mini-lisp-x86 - A compiler for a subset of Common Lisp to x86_64
    Copyright (C) 2025 BolvarsDad

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>. 

    File: hashmap.c
    Purpose: Implements a hashmap..
*/

#include <stdio.h>
#include <stdint.h>

#include "hashmap.h"

#define LOAD_FACTOR_THRESHHOLD 0.75

/*
 * UNUSED: Might be enabled later on for benchmarking purposes.
 *
unsigned int
hash_djb2(const char *str)
{
    unsigned int hash = 5381;

    while (*str)
        hash = ((hash << 5) + hash) + (unsigned char)*str++;

    return hash % TABLESZ;
}

unsigned long
hash_sdbm(const char *str)
{
    unsigned long hash = 0;

    while (*str)
        hash = *str++ + (hash << 6) + (hash << 16) - hash;

    return hash;
}
*/

/*
 * hash_fnv1a_32 - compute 32-bit FNV-1a hash of a NUL-terminated string
 * @str: input string to hash
 */
uint32_t
hash_fnv1a_32(const char *str)
{
    uint32_t hash = 2166136261u; /* FNV offset basis */

    while (*str)
        hash = (hash ^ (unsigned char)*str++) * 16777619; /* XOR then multiply by FNV prime */

    return hash;
}

/*
 *  u32hashmap_create - initializes the empty table and allocates space on the heap for buckets.
 *  Returns NULL on allocation failure
 *  @capacity: initial capacity for the table, 
 */
struct u32hashmap *
u32hashmap_create(size_t capacity)
{
    struct u32hashmap *map = malloc(sizeof(*map));
    if (!map) return NULL;

    map->entries = calloc(map->capacity, sizeof(*map->entries));
    if (!map->entries) {
        free(map);
        return 0;
    }

    map->count = 0;
    map->capacity = capacity;

    return map;
}

// Free all memory associated with the hashmap.
void
u32hashmap_destroy(struct u32hashmap *map)
{
    if (!map) return;

    for (size_t i = 0; i < map->capacity; ++i) {
        struct u32bucket *bucket = map->entries[i];
        if (bucket != NULL) {
            free(bucket->key);
            free(bucket);
        }
    }

    free(map->entries);
    free(map);
}

static int
u32hashmap_resize(struct u32hashmap *map)
{
    if (!map)
        return -1;

    struct u32bucket   **old_entries    = map->entries;
    size_t               old_capacity   = map->capacity;
    size_t               old_count      = map->count;
    size_t               new_capacity   = old_capacity * 2;

    /* Allocate new bucket array */
    map->entries = calloc(new_capacity, sizeof(*map->entries));
    if (!map->entries) {
        free(map);
        return -1;
    }

    map->capacity = new_capacity;
    map->count = 0;

    // Rehash all existing entries
    for (size_t i = 0; i < old_capacity; i++) {
        struct u32bucket *bucket = old_entries[i];
        
        if (bucket && bucket->status == BUCKET_FILLED) {
            // Insert into new table
            if (!u32hashmap_insert(map, bucket->key, bucket->val)) {
                // realistically we shouldn't reach here
                free(map->entries);
                map->entries = old_entries;
                map->capacity = old_capacity;
                map->count = old_count;
                return -1;
            }
            
            free(bucket->key);
            free(bucket);
        } else if (bucket) {
            free(bucket->key);
            free(bucket);
        }
    }

    free(old_entries);
    return 0;
}

int
u32hashmap_delete(struct u32hashmap *map, char const *key)
{
    if (!map || !key) return 0;

    uint32_t hash = hash_fnv1a_32(key);
    size_t idx = hash % map->capacity;

    for (size_t i = 0; i < map->capacity; ++i) {
        size_t probe = (idx + i) % map->capacity;
        struct u32bucket *bucket = map->entries[probe];
        
        if (!bucket) return 0; // empty slot

        if (bucket->status == BUCKET_FILLED && strcmp(bucket->key, key) == 0) {
            bucket->status = BUCKET_DELETED;
            map->count--;
            free(bucket);
            
            return 1;
        }
    }

    return 0;
}

static int
u32hashmap_resize_if_needed(struct u32hashmap *map)
{
    double load_factor = (double)map->count / (double)map->capacity;

    if (load_factor >= LOAD_FACTOR_THRESHHOLD)
        return u32hashmap(resize(map));

    return 0;
}

/*
 *  Resize the hashmap when the load factor gets too high (>= 0.75)
 *  Returns 1 on success, 0 on failure.
 */
int
u32hashmap_resize(struct u32hashmap *map)
{
    if (!map) return 0;

    struct u32bucket **entries_old = map->entries;
    size_t capacity_old = map->capacity;

    struct u32bucket **entries_new = calloc(capacity_old * 2, sizeof(struct u32bucket *));
    if (!entries_new) {
        free(map);
        return 0;
    }

    map->entries    = entries_new;
    map->count      = 0;
    map->capacity   = capacity_old * 2;

    for (size_t i = 0; i < map->capacity_old; ++i)
    {
        struct u32bucket *bucket = entries_old[i];
        if (bucket != NULL && bucket->status == BUCKET_FILLED) {
            u32hashmap_insert(map, bucket->key, bucket->val);
            free(bucket);
        }
    }

    free(entries_old);
    
    return 1;
}

/*
 *  Look up a key and return its value
 *  Returns 1 if found (stored in *val), 0 if not found
 */
int
u32hashmap_lookup(struct u32hashmap *map, char const *key, uint32_t val)
{
    if (!map || !key || !val) return 0;

    uint32_t hash = hash_fnv1a_32(key);
    size_t idx = hash % map->capacity;

    for (size_t i = 0; i < map->capacity; ++i) {
        size_t probe = (idx + i) % map->capacity;
        struct u32bucket *bucket = map->entries[probe];

        if (!bucket) return 0; // empty slot

        if (bucket->status == BUCKET_FILLED && strcmp(bucket->key, key) == 0) {
            *val = bucket->val;
            return 1;
        }
    }

    return 0;
}
