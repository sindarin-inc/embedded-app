#include "md.h"

void mbedtls_md_init(mbedtls_md_context_t *ctx) { return; }
const mbedtls_md_info_t *mbedtls_md_info_from_type(mbedtls_md_type_t type) { return 0; }

int mbedtls_md_setup(mbedtls_md_context_t *ctx, const mbedtls_md_info_t *type, int i) { return 0; }
void mbedtls_md_starts(mbedtls_md_context_t *ctx) { return; }
void mbedtls_md_free(mbedtls_md_context_t *ctx) { return; }

int mbedtls_md_update(mbedtls_md_context_t *ctx, const unsigned char *input, int ilen) { return 0; }

int mbedtls_md_finish(mbedtls_md_context_t *ctx, unsigned char *output) { return 0; }
