#pragma once

typedef struct mbedtls_md_context_t {
} mbedtls_md_context_t;

typedef enum mbedtls_md_type_t { MBEDTLS_MD_SHA256 } mbedtls_md_type_t;

typedef struct mbedtls_md_info_t {
} mbedtls_md_info_t;

void mbedtls_md_init(mbedtls_md_context_t *ctx);
const mbedtls_md_info_t *mbedtls_md_info_from_type(mbedtls_md_type_t type);

int mbedtls_md_setup(mbedtls_md_context_t *ctx, const mbedtls_md_info_t *type, int i);
void mbedtls_md_starts(mbedtls_md_context_t *ctx);
void mbedtls_md_free(mbedtls_md_context_t *ctx);

int mbedtls_md_update(mbedtls_md_context_t *ctx, const unsigned char *input, int ilen);

int mbedtls_md_finish(mbedtls_md_context_t *ctx, unsigned char *output);
