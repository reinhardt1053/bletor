#include "sha1.h"
#include <string.h>
#include <stdio.h>

#define rol(value, bits) (((value) << (bits)) | ((value) >> (32 - (bits))))

static void sha1_transform(uint32_t state[5], const uint8_t buffer[64]);

void sha1_init(struct sha1_ctx *context) {
    context->state[0] = 0x67452301;
    context->state[1] = 0xEFCDAB89;
    context->state[2] = 0x98BADCFE;
    context->state[3] = 0x10325476;
    context->state[4] = 0xC3D2E1F0;
    context->count[0] = context->count[1] = 0;
}

void sha1_update(struct sha1_ctx *context, const uint8_t *data, size_t len) {
    size_t i, j;

    j = (context->count[0] >> 3) & 63;
    if ((context->count[0] += len << 3) < (len << 3)) context->count[1]++;
    context->count[1] += (len >> 29);
    
    if ((j + len) > 63) {
        memcpy(&context->buffer[j], data, (i = 64-j));
        sha1_transform(context->state, context->buffer);
        for (; i + 63 < len; i += 64) {
            sha1_transform(context->state, &data[i]);
        }
        j = 0;
    } else i = 0;
    memcpy(&context->buffer[j], &data[i], len - i);
}

void sha1_final(uint8_t digest[20], struct sha1_ctx *context) {
    uint8_t finalcount[8];
    uint32_t i;

    for (i = 0; i < 8; i++) {
        finalcount[i] = (uint8_t)((context->count[(i >= 4 ? 0 : 1)]
                                   >> ((3-(i & 3)) * 8) ) & 255);
    }
    
    sha1_update(context, (uint8_t *)"\200", 1);
    while ((context->count[0] & 504) != 448) {
        sha1_update(context, (uint8_t *)"\0", 1);
    }
    sha1_update(context, finalcount, 8);
    
    for (i = 0; i < 20; i++) {
        digest[i] = (uint8_t)((context->state[i>>2] >> ((3-(i & 3)) * 8) ) & 255);
    }
}

static void sha1_transform(uint32_t state[5], const uint8_t buffer[64]) {
    uint32_t a, b, c, d, e;
    uint32_t block[80];

    // Copy buffer into block
    for (int i = 0; i < 16; i++) {
        block[i] = (buffer[4*i+0] << 24) +
                   (buffer[4*i+1] << 16) +
                   (buffer[4*i+2] << 8) +
                   (buffer[4*i+3]);
    }

    // Extend the 16 32-bit words into 80 32-bit words
    for (int i = 16; i < 80; i++) {
        block[i] = rol(block[i-3] ^ block[i-8] ^ block[i-14] ^ block[i-16], 1);
    }

    // Initialize hash value for this chunk
    a = state[0];
    b = state[1];
    c = state[2];
    d = state[3];
    e = state[4];

    // Main loop
    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        
        if (i < 20) {
            f = (b & c) | ((~b) & d);
            k = 0x5A827999;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDC;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6;
        }
        
        uint32_t temp = rol(a, 5) + f + e + k + block[i];
        e = d;
        d = c;
        c = rol(b, 30);
        b = a;
        a = temp;
    }

    // Add this chunk's hash to result so far
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
}

// Function to calculate SHA1 hash of binary data
void sha1_hash(const uint8_t *data, size_t len, uint8_t hash[SHA1_DIGEST_LENGTH]) {
    struct sha1_ctx ctx;
    sha1_init(&ctx);
    sha1_update(&ctx, data, len);
    sha1_final(hash, &ctx);
}

// Helper function to print the hash in hex format
void print_sha1(const uint8_t hash[SHA1_DIGEST_LENGTH]) {
    for (int i = 0; i < SHA1_DIGEST_LENGTH; i++) {
        printf("%02x", hash[i]);
    }
    printf("\n");
}
