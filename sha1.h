#ifndef SHA1_H
#define SHA1_H

#include <stdint.h>
#include <stddef.h>

// SHA1 constants
#define SHA1_DIGEST_LENGTH 20

// SHA1 context structure
struct sha1_ctx {
    uint32_t state[5];
    uint32_t count[2];
    uint8_t buffer[64];
};

// SHA1 functions
void sha1_init(struct sha1_ctx *context);
void sha1_update(struct sha1_ctx *context, const uint8_t *data, size_t len);
void sha1_final(uint8_t digest[SHA1_DIGEST_LENGTH], struct sha1_ctx *context);

// Utility functions
void sha1_hash(const uint8_t *data, size_t len, uint8_t hash[SHA1_DIGEST_LENGTH]);
void print_sha1(const uint8_t hash[SHA1_DIGEST_LENGTH]);

#endif /* SHA1_H */
