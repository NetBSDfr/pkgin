/*
 * Copyright (c) 2026 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "pkghash.h"

#include <assert.h>

#include "external/lib.h"
#include "external/sha2.h"

struct pkghash {
	char sha256_hex[SHA256_DIGEST_STRING_LENGTH];
};

struct pkghash_ctx {
	SHA256_CTX		sha256_ctx;
	const struct pkghash	*expected;
};

struct pkghash *
pkghash_import(const char *type, const char *hex)
{
	struct pkghash *H;

	if (strcmp(type, "sha256") != 0)
		return NULL;
	/* Note: SHA256_DIGEST_STRING_LENGTH includes NUL terminator. */
	if (strlen(hex) != SHA256_DIGEST_STRING_LENGTH - 1)
		return NULL;
	H = xmalloc(sizeof(*H));
	memcpy(H->sha256_hex, hex, SHA256_DIGEST_STRING_LENGTH);
	return H;
}

struct pkghash_ctx *
pkghash_verify_init(const struct pkghash *H)
{
	struct pkghash_ctx *C;

	C = xmalloc(sizeof(*C));
	SHA256_Init(&C->sha256_ctx);
	C->expected = H;

	return C;
}

void
pkghash_verify_update(struct pkghash_ctx *C, const void *buf, size_t len)
{

	SHA256_Update(&C->sha256_ctx, buf, len);
}

bool
pkghash_verify_final(struct pkghash_ctx *C)
{
	uint8_t sha256[SHA256_DIGEST_LENGTH];
	char sha256_hex[SHA256_DIGEST_STRING_LENGTH];
	unsigned i;

	SHA256_Final(sha256, &C->sha256_ctx);

	/*
	 * Note: This computation does not run in constant time, but we
	 * don't care because none of the hashes involved are secret.
	 */
	for (i = 0; i < SHA256_DIGEST_LENGTH; i++) {
		snprintf(sha256_hex + 2*i, sizeof(sha256_hex) - 2*i, "%02hhx",
		    sha256[i]);
	}
	if (strcmp(sha256_hex, C->expected->sha256_hex) != 0)
		return false;
	return true;
}

bool
pkghash_verify_file(const struct pkghash *H, const char *path)
{
	char buf[BUFSIZ];
	struct pkghash_ctx *C = NULL;
	FILE *fp = NULL;
	bool result = false;
	size_t n;

	C = pkghash_verify_init(H);
	if ((fp = fopen(path, "r")) == NULL)
		goto out;
	while ((n = fread(buf, 1, sizeof(buf), fp)) != 0)
		pkghash_verify_update(C, buf, n);
	if (!feof(fp))
		goto out;
	result = pkghash_verify_final(C);

out:	fclose(fp);
	free(C);
	return result;
}
