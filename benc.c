#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "benc.h"

/* 
   Decoding context 
   The context serves as our parsing state, tracking where we are in the buffer as we decode values.
   We'll pass this context to our decoding functions to avoid having to pass the same parameters repeatedly.
*/
struct benc_decode_ctx {
    const uint8_t *data;   /* Data to parse */
    size_t len;            /* Length of data */
    size_t pos;            /* Current position */
    int error;             /* Error code if any */
};

/*
  Encoding context. 
*/
struct benc_encode_ctx {
    uint8_t *data;     /* Data encoded */
    size_t len;        /* Lenght of data */
    size_t pos;        /* Current position */
};

// Forward declarations
struct benc *benc_ctx_decode(struct benc_decode_ctx *ctx);

/**
 * Optimized function to check if a character is a digit
 */
static inline int isdigit(uint8_t c) {
    return c - '0' < 10;
}
/**
 * Convert a string to an int64_t, optimized for bencode parsing
 * 
 * @param result Pointer to store the converted value
 * @param str Pointer to the start of the string to parse
 * @param end Pointer to the end of the string buffer
 * @return Pointer to the character after the last digit, or NULL on error
 */
static const uint8_t *str_to_int64(int64_t *result,  const uint8_t *str, const uint8_t *end) {
    register const uint8_t *s = str;
    register int64_t acc;
    register int c;
    register int neg = 0;
    register int64_t cutoff;
    register int cutlim;

    // Check for sign
    if (s < end && *s == '-') {
        neg = 1;
        s++;
    }

    /*
	 * Compute the cutoff value between legal numbers and illegal
	 * numbers.  That is the largest legal value, divided by the
	 * base 10.  An input number that is greater than this value, if
	 * followed by a legal input character, is too big.  One that
	 * is equal to this value may be valid or not; the limit
	 * between valid and invalid numbers is then based on the last
	 * digit.  For instance, if the range for longs is
	 * [-2147483648..2147483647] cutoff will be set to 214748364 and 
     * cutlim to either  7 (neg==0) or 8 (neg==1), meaning that if we 
     * have accumulated a value > 214748364, or equal but 
     * the next digit is > 7 (or 8),the number is too big	
     */
    cutoff = neg ? -(INT64_MIN / 10) : INT64_MAX / 10;
    cutlim = neg ? -(INT64_MIN % 10) : INT64_MAX % 10;

    // Parse digits
    for (acc = 0; s < end && isdigit(*s); s++) {
        c = *s - '0';
        
        if (acc > cutoff || (acc == cutoff && c > cutlim))
            return NULL; // Overflow
        
        acc *= 10;
        acc += c;
    }

    // Check if we parsed at least one digit
    if (s == str || (neg && s == str + 1))
        return NULL;

    // Negate if necessary
    if (neg)
        acc = -acc;

    *result = acc;
    return s;
}

/**
 * Parse a positive size_t value from the current position,
 * used to parse bencoded string size (i.e 5:hello)
 * 
 * @param result Pointer to store the size value
 * @param ctx The parsing context
 * @return 0 on success, -1 on error
 */
static int parse_size(size_t *result, struct benc_decode_ctx *ctx) {
    // Check if we have data left
    if (ctx->pos >= ctx->len) {
        ctx->error = BENC_INCOMPLETE;
        return -1;
    }
    
    // First character must be a digit
    if (ctx->data[ctx->pos] < '0' || ctx->data[ctx->pos] > '9') {
        ctx->error = BENC_INVALID;
        return -1;
    }
    
    // Check for leading zeros (invalid in bencode except for "0")
    if (ctx->data[ctx->pos] == '0' && 
        ctx->pos + 1 < ctx->len && 
        ctx->data[ctx->pos + 1] >= '0' && ctx->data[ctx->pos + 1] <= '9') {
        ctx->error = BENC_INVALID;
        return -1;
    }
    
    // Parse the number using our optimized function
    int64_t value;
    const uint8_t *new_pos = str_to_int64(&value, ctx->data + ctx->pos, ctx->data + ctx->len);
    if (new_pos == NULL || value < 0) {
        ctx->error = BENC_INVALID;
        return -1;
    }
    
    // Check if value fits in size_t
    if ((uint64_t)value > SIZE_MAX) {
        ctx->error = BENC_INVALID;
        return -1;
    }
    
    // Update position
    ctx->pos = new_pos - ctx->data;
    
    *result = (size_t)value;
    return 0;
}

/**
 * Parse a signed int64_t value from the current position
 * Used to parse a bencoded number (for example i50e)
 * @param result Pointer to store the integer value
 * @param ctx The parsing context
 * @return 0 on success, -1 on error
 */
static int parse_int64(int64_t *result, struct benc_decode_ctx *ctx) { 
    // Check if we have data left
    if (ctx->pos >= ctx->len) {
        ctx->error = BENC_INCOMPLETE;
        return -1;
    }
    
    // First character must be a digit or minus sign
    if (!(ctx->data[ctx->pos] == '-' || (ctx->data[ctx->pos] >= '0' && ctx->data[ctx->pos] <= '9'))) {
        ctx->error = BENC_INVALID;
        return -1;
    }
    
    // Check for negative zero (invalid in bencode)
    if (ctx->data[ctx->pos] == '-' && 
        ctx->pos + 1 < ctx->len && ctx->data[ctx->pos + 1] == '0') {
        ctx->error = BENC_INVALID;
        return -1;
    }
    
    // Check for leading zeros for positive numbers (invalid except for "0")
    if (ctx->data[ctx->pos] == '0' && 
        ctx->pos + 1 < ctx->len && 
        ctx->data[ctx->pos + 1] >= '0' && ctx->data[ctx->pos + 1] <= '9') {
        ctx->error = BENC_INVALID;
        return -1;
    }
    
    
    // Check for leading zeros for negative numbers (invalid)
    if (ctx->data[ctx->pos] == '-' && 
        ctx->pos + 1 < ctx->len && ctx->data[ctx->pos + 1] == '0' &&
        ctx->pos + 2 < ctx->len && ctx->data[ctx->pos + 2] >= '0' && ctx->data[ctx->pos + 2] <= '9') {
        ctx->error = BENC_INVALID;
        return -1;
    }
    
    // Parse the number using our optimized function
    const uint8_t *new_pos = str_to_int64(result, ctx->data + ctx->pos, ctx->data + ctx->len);
    if (new_pos == NULL) {
        ctx->error = BENC_INVALID;
        return -1;
    }
    
    // Update position
    ctx->pos = new_pos - ctx->data;
    
    return 0;
}

static size_t type_size(int type)
{
    switch (type) {
    case BENC_STR:
        return sizeof(struct benc_str);
    case BENC_INT:
        return sizeof(struct benc_int);
    case BENC_LIST:
        return sizeof(struct benc_list);
    case BENC_DICT:
        return sizeof(struct benc_dict);
    default:
        fprintf(stderr, "Error, unknown type %c", type);
        abort();
    }
}

static void *alloc(int type)
{
    struct benc *b = calloc(1, type_size(type));
    if (b == NULL)
        return NULL;
    b->type = type;
    return b;
}

static struct benc *decode_str(struct benc_decode_ctx *ctx) {
    size_t start_pos = ctx->pos;
    
    // Parse the string length
    size_t str_len = 0;
    if (parse_size(&str_len, ctx) != 0)
        return NULL;
    
    // Check for the colon delimiter
    if (ctx->pos >= ctx->len || ctx->data[ctx->pos] != ':') {
        ctx->pos = start_pos;
        ctx->error = BENC_INVALID;
        return NULL;
    }
    
    // Skip the colon
    ctx->pos++;
    
    // Check if we have enough data for the string
    if (ctx->len - ctx->pos < str_len) {
        ctx->pos = start_pos;
        ctx->error = BENC_INCOMPLETE;
        return NULL;
    }
    
    // Allocate and initialize the string structure
    struct benc_str *str = alloc(BENC_STR);
    if (str == NULL) {
        ctx->error = BENC_NOMEM;
        return NULL;
    }
    
    // Set string data - note: this points directly to input data
    str->data = ctx->data + ctx->pos;
    str->len = str_len;
    
    // Advance position past the string
    ctx->pos += str_len;
    
    return (struct benc *) str;
}

static struct benc *decode_int(struct benc_decode_ctx *ctx) {
    if (ctx->pos >= ctx->len || ctx->data[ctx->pos] != 'i') {
        ctx->error = BENC_INVALID;
        return NULL;
    }

    ctx->pos++; // Skip 'i'

    int64_t value;
    if (parse_int64(&value, ctx) != 0)
        return NULL;

    if (ctx->pos >= ctx->len || ctx->data[ctx->pos] != 'e') {
        ctx->error = BENC_INVALID;
        return NULL;
    }

    ctx->pos++; // Skip 'e'

    // Allocate and initialize the integer structure
    struct benc_int *integer = alloc(BENC_INT);
    if (integer == NULL) {
        ctx->error = BENC_NOMEM;
        return NULL;
    }
    
    integer->value = value;
    return (struct benc *) integer;
}

static int benc_list_resize(struct benc_list *list){
    size_t new_capacity;

    if (list->capacity == 0)
        new_capacity = 5;
    else
        new_capacity = list->capacity * 2;

    struct benc **new_items = realloc(list->items,new_capacity * sizeof(struct benc *));
    if (new_items == NULL)
        return -1;

    list->capacity = new_capacity;
    list->items = new_items;

    return 0;
}
static int benc_list_append(struct benc_list *list, struct benc *item) {
    if (list->size == list->capacity) {
        if (benc_list_resize(list)) return -1;
    }

    list->items[list->size] = item;
    list->size++;

    return 0;
}

static struct benc *decode_list(struct benc_decode_ctx *ctx) {
    struct benc_list *list = alloc(BENC_LIST);
    if (list == NULL) {
        ctx->error = BENC_NOMEM;
        return NULL;
    }

    ctx->pos++; // Skip 'l'

    while (ctx->pos < ctx->len && ctx->data[ctx->pos] != 'e') {
        struct benc *item = benc_ctx_decode(ctx);
        if (item == NULL){
            benc_free((struct benc *) item);
            return NULL;
        }
       
        if (benc_list_append(list,item)){
            benc_free((struct benc *) list);
            ctx->error = BENC_NOMEM;
            return NULL;
        }
    }

    if (ctx->pos >= ctx->len) {
        ctx->error = BENC_INVALID;
        return NULL;
    }

    ctx->pos++; // Skip 'e'
    return (struct benc *)list;
}

struct benc *benc_decode(const uint8_t *data, const size_t len, int *error) {
    struct benc_decode_ctx ctx = { .data = data, .len = len };
    struct benc *b = benc_ctx_decode(&ctx);
    if (b != NULL && ctx.pos != ctx.len) {
        benc_free(b);
        *error = BENC_TRAILING_DATA;
        return NULL;
    }
    *error = ctx.error;
    return b;
}


void benc_free(struct benc *b) {
    if (!b) return;
    
    switch (b->type) {
        case BENC_LIST: {
            struct benc_list *list = (struct benc_list *)b;
            for (size_t i = 0; i < list->size; i++) {
                benc_free(list->items[i]);
            }
            free(list->items);
            free(list);
            break;
        }
        
        case BENC_DICT: {
            struct benc_dict *dict = (struct benc_dict *)b;
            for (size_t i = 0; i < dict->size; i++) {
                benc_free((struct benc *)dict->nodes[i].key);
                benc_free(dict->nodes[i].value);
            }
            free(dict->buckets);
            free(dict->nodes);
            free(dict);
            break;
        }
        
        default:
            free(b);
            break;
    }
}

const char *benc_strerror(int error) {
    switch (error) {
    case BENC_OK:
        return "ok";
        break;
    case BENC_INCOMPLETE:
        return "incomplete";
        break;
    case BENC_INVALID:
        return "invalid";
        break;
    case BENC_NOMEM:
        return "out of memory";
        break;
    case BENC_TRAILING_DATA:
        return "trailing data";
        break;
    default:
        return "unkown";
    }
}

/*
 * FNV-1a hash function for strings
 * A simple non-cryptographic hash function that works well for most purposes
 */
static int64_t str_hash(const uint8_t *data, size_t len) {
    int64_t hash = 0xcbf29ce484222325ULL; // FNV offset basis
    for (size_t i = 0; i < len; i++) {
        hash ^= data[i];
        hash *= 0x100000001b3ULL; // FNV prime
    }
    return hash;
}

/*
 * Get the bucket index for an hash value
 */
static int hash_bucket(uint64_t hash, const struct benc_dict *dict){
    return (int)hash & (dict->capacity-1); // hash % capacity
}

/*
 * 
 * Resize a dict to a new capacity, 
 * new capacity must be a power of 2
 *
 */
static int benc_dict_resize(struct benc_dict *dict) {
    int *new_buckets;
    struct benc_dict_node *new_nodes;

    size_t new_capacity;

    if (dict->capacity == 0)
        new_capacity = 4;
    else
        new_capacity = dict->capacity * 2;
    
    // Allocate new memory for buckets and nodes
    new_buckets = realloc(dict->buckets,sizeof(int) * new_capacity);
    if (!new_buckets) return -1;

    new_nodes = realloc(dict->nodes, sizeof(struct benc_dict_node) * new_capacity);
    if (!new_nodes) {
        free(new_buckets);
        return -1;
    }

    // Update dictionary structure
    dict->buckets = new_buckets;
    dict->nodes = new_nodes;
    dict->capacity = new_capacity;

    // Initialize all buckets to empty (-1)
    for (size_t i = 0; i < new_capacity; i++){
        dict->buckets[i] = -1;
    }

    // Insert again the nodes into buckets: rehash all existing entries 
    for (size_t i = 0; i < dict->size; i++) {
        struct benc_dict_node *node = &dict->nodes[i];
        int bucket = hash_bucket(node->hash,dict);

        // Insize_t sert at the head of bucket chain
        node->next = dict->buckets[bucket];
        dict->buckets[bucket] = i;
    }

    return 0;
}

/* 
 * Find a note position by key, returns -1 if not found
 */
static int benc_dict_find_pos(const struct benc_dict *dict, const struct benc_str *key){
    if (dict->size == 0) return -1;

    uint64_t hash = str_hash(key->data,key->len);
    int bucket = hash_bucket(hash,dict);
    int pos = dict->buckets[bucket];

    while (pos != -1){
        struct benc_dict_node *node = &dict->nodes[pos];
        if (node->hash == hash && 
            node->key->len == key->len &&
            memcmp(node->key->data,key->data,key->len) == 0){
            return pos;
        }
        pos = node->next;
    }

    return -1;
}

/*
 * Set a key-value pair in the dictionary
 * Use a zero-copy approach for keys and values
 */

int benc_dict_set(struct benc_dict *dict, struct benc_str *key, struct benc *value){
    if (!dict || !key || !value) return -1;

    int pos = benc_dict_find_pos(dict,key);

    // Check if the key already exists
    if (pos != -1){
        // Update existing entry
        benc_free(dict->nodes[pos].value);
        dict->nodes[pos].value = value;

        // Free the incoming key since we are not using it
        benc_free((struct benc *)key);
        return 0;
    }

    // Resize is needed? 
    if (dict->size == dict->capacity){
        if (benc_dict_resize(dict) != 0) return -1;
    }

    // Add new node
    uint64_t hash = str_hash(key->data,key->len);
    int bucket = hash_bucket(hash,dict);
    pos = (int)dict->size;
    
    struct benc_dict_node node = dict->nodes[pos];

    node.hash = hash;
    node.key = key;
    node.value = value;
    node.next = dict->buckets[bucket];

    dict->nodes[pos] = node;
    dict->buckets[bucket] = pos;
    dict->size++;

    return 0;
}

/*
 * Get a value from a key, returns NULL otherwise
 */
struct benc *benc_dict_get(const struct benc_dict *dict, const char *key){
    if (!dict || !key) return NULL;
    
    struct benc_str benc_key = {.data = (const uint8_t *)key, .len = strlen(key)};
    int pos = benc_dict_find_pos(dict,&benc_key);

    if (pos == -1) return NULL;

    return dict->nodes[pos].value;
}

struct benc_str *benc_dict_get_str(const struct benc_dict *dict, const char *key){
    struct benc *value = benc_dict_get(dict,key);
    if (!value || value->type != BENC_STR)  return NULL;

    return (struct benc_str *)value;
}

static struct benc *decode_dict(struct benc_decode_ctx *ctx) {
    struct benc_dict *dict = alloc(BENC_DICT);
    if (dict == NULL) {
        ctx->error = BENC_NOMEM;
        return NULL;
    }

    ctx->pos++; // Skip 'd'

    while (ctx->pos < ctx->len && ctx->data[ctx->pos] != 'e') {
        // Key
        struct benc_str *key = (struct benc_str *)decode_str(ctx);
        if (key == NULL){
            benc_free((struct benc *)dict);
            return NULL;
        }

        // Value
        struct benc *value = benc_ctx_decode(ctx);

        if (benc_dict_set(dict,key,value) != 0) {
            benc_free((struct benc *)dict);
            ctx->error = BENC_NOMEM;
            return NULL;
        }
    }

    if (ctx->pos >= ctx->len) {
        ctx->error = BENC_INVALID;
        return NULL;
    }

    ctx->pos++; // Skip 'e'
    return (struct benc *)dict;
}

struct benc *benc_ctx_decode(struct benc_decode_ctx *ctx) {
    if (ctx->pos >= ctx->len) {
        ctx->error = BENC_INCOMPLETE;
        return NULL;
    }

    uint8_t c = ctx->data[ctx->pos];

    if (c >= '0' && c <= '9') {
        return decode_str(ctx);
    } else if (c == 'i') {
        return decode_int(ctx);
    } else if (c == 'l') {
        return decode_list(ctx);
    } else if (c == 'd') {
        return decode_dict(ctx);
    }
    
    ctx->error = BENC_INVALID;
    return NULL;
}

int benc_print(const struct benc *b) {
    switch (b->type){
    case BENC_STR:
        const struct benc_str *s = (const struct benc_str *) b;
        printf("\"%.*s\"", (int)s->len, s->data);
        break;
    case BENC_INT:
        const struct benc_int *i = (const struct benc_int *) b;
        printf("%lld", (long long)i->value);
        break;
    case BENC_LIST:
        printf("[");
        const struct benc_list *l = (const struct benc_list *) b;
        for (size_t pos=0; pos < l->size; pos++){
            if (benc_print(l->items[pos]))
               return -1;
            if (pos < l->size-1)
               printf(",");
        }
        printf("]");
        break;
    case BENC_DICT:
        printf("{");
        const struct benc_dict *d = (const struct benc_dict *) b;
        for (size_t pos=0; pos < d->size; pos++){
            struct benc_str *key = d->nodes[pos].key;
            printf("\"%.*s\":", (int)key->len, key->data);
            
            struct benc *value = d->nodes[pos].value;
            benc_print(value);
            if (pos < d->size-1)
                printf(",");
        }
        printf("}");
        break;
    default:
       printf("<type not recognized>");
    }
    return 0;
}

struct benc_buffer {
    uint8_t *data;
    size_t len;
    size_t capacity;
};

static int benc_buffer_init(struct benc_buffer *buf, size_t initial_size) {
    buf->data = malloc(initial_size);
    if (!buf->data) return -1;

    buf->len = 0;
    buf->capacity = initial_size;
    return 0;
}

static int benc_buffer_append(struct benc_buffer *buf, const uint8_t *data, size_t len) {
    if (buf->len + len > buf->capacity){
        // Resize
        size_t need_capacity = buf->len + len;
        size_t new_capacity = buf->capacity * 2;

        if (new_capacity < need_capacity) {
            new_capacity = need_capacity + (need_capacity / 4); // Add 25% extra space to avoid immediate realloc on the next call
        }

        uint8_t *new_data = realloc(buf->data, new_capacity);
        if (!new_data) return -1;

        buf->data = new_data;
        buf->capacity = new_capacity;
    }

    memcpy(buf->data + buf->len, data, len);
    buf->len += len;
    return 0;
}

static int benc_encode_to_buffer(struct benc_buffer *buf, const struct benc *b){
    uint8_t tmp[24]; // just enough for int64_t + delimeters
    int len;

    switch (b->type){
        case BENC_INT:
            const struct benc_int *i = (const struct benc_int *)b;
            len = snprintf((char *)tmp,sizeof(tmp), "i%llde",i->value);
            if (len <= 0) return -1;
            return benc_buffer_append(buf,tmp,len);

        case BENC_STR:
            const struct benc_str *s = (const struct benc_str *)b;
            len = snprintf((char *)tmp,sizeof(tmp), "%zu:",s->len);
            if (len <= 0) return -1; 

            if (benc_buffer_append(buf,tmp,len) != 0)
                return -1;

            return benc_buffer_append(buf,s->data,s->len);
        
        case BENC_LIST:
            const struct benc_list *l = (const struct benc_list *)b;
            if (benc_buffer_append(buf,(const uint8_t *)"l",1) != 0) 
                return -1;

            for (size_t i = 0; i < l->size; i++){
                if (benc_encode_to_buffer(buf,l->items[i]) != 0) 
                    return -1;
            }

            return benc_buffer_append(buf,(const uint8_t *)"e",1); 
        
        case BENC_DICT:
            const struct benc_dict *d = (const struct benc_dict *)b;
            if (benc_buffer_append(buf,(const uint8_t *)"d",1) != 0)
                return -1;
            
            for (size_t i = 0; i < d->size; i++){
                struct benc_dict_node *node = &d->nodes[i];

                if (benc_encode_to_buffer(buf,(const struct benc *)node->key) != 0) 
                    return -1;

                if (benc_encode_to_buffer(buf,(const struct benc *)node->value) != 0)
                    return -1;
            }

            return benc_buffer_append(buf,(const uint8_t *)"e",1);
    }

    return -1;
}

uint8_t *benc_encode(size_t *len, const struct benc *b){
    struct benc_buffer buf;

    if (benc_buffer_init(&buf, 1024) != 0) 
        return NULL;

    if (benc_encode_to_buffer(&buf,b) != 0) {
        free(buf.data);
        return NULL;
    }

    *len = buf.len;
    return buf.data;
}
