#ifndef BENC_H
#define BENC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

enum {
    BENC_INT = 1,
    BENC_STR,
    BENC_LIST,
    BENC_DICT 
};

/* Error codes */
enum {
    BENC_OK = 0,               /* no error */
    BENC_INVALID,              /* Invalid bencode data */
    BENC_INCOMPLETE,           /* Not enough data */
    BENC_NOMEM,                /* Out of memory */
    BENC_TRAILING_DATA         /* Extra data after complete value */
};

/* Base type */
struct benc {
   uint8_t type;
};

struct benc_str {
    uint8_t type;
    const uint8_t *data;   /* Points to string data (this is not null terminated) */
    size_t len;
};

struct benc_int {
    uint8_t type;
    int64_t value;
};

struct benc_list {
    uint8_t type;
    struct benc **items;
    size_t size;        /* Number of elements */
    size_t capacity;   /* Allocated capacity */
};

struct benc_dict_node {
    uint64_t hash;     /* Hash of the key (using uint64_t for better hash quality) */
    struct benc_str *key;
    struct benc *value;
    int next;       /* Index for collision handling, -1 means ends of the chain */
};

struct benc_dict {
    uint8_t type;
    struct benc_dict_node *nodes;
    int *buckets;      /* Hash table buckets */
    size_t size;       /* Number of key-values pairs currently stored in *nodes */
    size_t capacity;   /* Allocated capacity, (number of buckets) */
};

/* Main decoding/encoding functions */
struct benc *benc_decode(const uint8_t *data, const size_t len, int *error);
uint8_t *benc_encode(size_t *len, const struct benc *b);

/* Print a bencode struct to stdout*/
int benc_print(const struct benc *b);

/* Memory management */
void benc_free(struct benc *b);

/* Error handling */
const char *benc_strerror(int error);

/* Dictionary */
struct benc *benc_dict_get(const struct benc_dict *dict, const char *key);
int benc_dict_set(struct benc_dict *dict, struct benc_str *key, struct benc *value);

struct benc_str *benc_dict_get_str(const struct benc_dict *dict, const char *key);

#endif /* BENC_H */
