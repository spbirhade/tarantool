
/*
 * Copyright (C) 2011 Mail.RU
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitnted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*-
 * Copyright (c) 2008 Damien Bergamini <damien.bergamini@free.fr>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This code implements the CMAC (Cipher-based Message Authentication)
 * algorithm described in FIPS SP800-38B using the AES-128 cipher.
 */

#include <stdlib.h>
#include <string.h>

#include <sys/types.h>

#include <tnt_aes.h>
#include <tnt_cmac.h>

#define LSHIFT(v, r) do { \
	int i; \
	for (i = 0; i < 15; i++) \
		(r)[i] = (v)[i] << 1 | (v)[i + 1] >> 7; \
	(r)[15] = (v)[15] << 1; \
} while (0)

#define XOR(v, r) do { \
	int i; \
	for (i = 0; i < 16; i++) \
		(r)[i] ^= (v)[i]; \
} while (0)

void
tnt_aes_cmac_init(tnt_aes_cmac_ctx *ctx)
{
	memset(ctx->X, 0, sizeof ctx->X);
	ctx->M_n = 0;
}

void
tnt_aes_cmac_setkey(tnt_aes_cmac_ctx *ctx, const u_int8_t key[TNT_AES_CMAC_KEY_LENGTH])
{
	tnt_aes_set_key_enc_only(&ctx->aes, key, 128);
}

#ifndef MIN
#define MIN(a, b)((a) < (b) ? (a) : (b))
#endif

void
tnt_aes_cmac_update(tnt_aes_cmac_ctx *ctx, const u_int8_t *data, u_int len)
{
	u_int mlen;

	if (ctx->M_n > 0) {
		mlen = MIN(16 - ctx->M_n, len);
		memcpy(ctx->M_last + ctx->M_n, data, mlen);
		ctx->M_n += mlen;
		if (ctx->M_n < 16 || len == mlen)
			return;
		XOR(ctx->M_last, ctx->X);
		tnt_aes_encrypt(&ctx->aes, ctx->X, ctx->X);
		data += mlen;
		len -= mlen;
	}
	while (len > 16) {	/* not last block */
		XOR(data, ctx->X);
		tnt_aes_encrypt(&ctx->aes, ctx->X, ctx->X);
		data += 16;
		len -= 16;
	}
	/* potential last block, save it */
	memcpy(ctx->M_last, data, len);
	ctx->M_n = len;
}

void
tnt_aes_cmac_final(u_int8_t digest[TNT_AES_CMAC_DIGEST_LENGTH], tnt_aes_cmac_ctx *ctx)
{
	u_int8_t K[16];

	/* generate subkey K1 */
	memset(K, 0, sizeof K);
	tnt_aes_encrypt(&ctx->aes, K, K);

	if (K[0] & 0x80) {
		LSHIFT(K, K);
		K[15] ^= 0x87;
	} else
		LSHIFT(K, K);

	if (ctx->M_n == 16) {
		/* last block was a complete block */
		XOR(K, ctx->M_last);
	} else {
		/* generate subkey K2 */
		if (K[0] & 0x80) {
			LSHIFT(K, K);
			K[15] ^= 0x87;
		} else
			LSHIFT(K, K);

		/* padding(M_last) */
		ctx->M_last[ctx->M_n] = 0x80;
		while (++ctx->M_n < 16)
			ctx->M_last[ctx->M_n] = 0;

		XOR(K, ctx->M_last);
	}
	XOR(ctx->M_last, ctx->X);
	tnt_aes_encrypt(&ctx->aes, ctx->X, digest);

	memset(K, 0, sizeof K);
}
