#ifndef MINI_LISP_X86_SRC_COMPILER_HASHMAP_H_
#define MINI_LISP_X86_SRC_COMPILER_HASHMAP_H_

struct u32bucket {
    char const  *key;
    uint32_t     val;

    enum status {
        BUCKET_EMPTY = 0,
        BUCKET_FILLED,
        BUCKET_DELETED
    };
};

struct u32hashmap {
    size_t count;
    size_t capacity;

    struct u32bucket **entries;
};

#endif
