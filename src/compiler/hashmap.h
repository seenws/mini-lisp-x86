#ifndef MINI_LISP_X86_SRC_COMPILER_HASHMAP_H_
#define MINI_LISP_X86_SRC_COMPILER_HASHMAP_H_

#include <stddef.h>

struct u32bucket {
    const char          *key;
    void                *val;
    struct u32bucket    *next;
};

struct u32hashmap {
    struct u32bucket **buckets;
    size_t nbuckets;
    size_t nfilled;
};

// TODO: benchmark against other hash functions later
unsigned long hash_djb2(const char *s);

struct u32hashmap  *hashmap_new    (size_t initial_size);
void                hashmap_free    (struct u32hashmap *hashmap);
int                 hashmap_lookup  (struct u32hashmap *hashmap, const char *key, void **out_val);
int                 hashmap_insert  (struct u32hashmap *hashmap, const char *key, void *val);
int                 hashmap_remove  (struct u32hashmap *hashmap, const char *key);

#endif
