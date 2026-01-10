#include <stdio.h>
#include <stdint.h>

typedef int64_t word;

#define INTEGER_TAG 0x0
#define INTEGER_MASK 0x3

// 
word encode_integer(long value) {
    return (word)value << 2;
}

word decode_integer(word value) {
    return (long)value >> 2;
}
