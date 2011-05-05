#ifndef TNT_CMAC_H_
#define TNT_CMAC_H_

/* AES-CMAC OpenBSD implementation */

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

#define TNT_AES_CMAC_KEY_LENGTH    (16)
#define TNT_AES_CMAC_DIGEST_LENGTH (16)

typedef struct {

	tnt_aes_ctx_t aes;
	u_int8_t      X[16];
	u_int8_t	  M_last[16];
	u_int		  M_n;

} TNT_AES_CMAC_CTX;

void TNT_AES_CMAC_Init(TNT_AES_CMAC_CTX *);
void TNT_AES_CMAC_SetKey(TNT_AES_CMAC_CTX *, const u_int8_t [TNT_AES_CMAC_KEY_LENGTH]);
void TNT_AES_CMAC_Update(TNT_AES_CMAC_CTX *, const u_int8_t *, u_int);
void TNT_AES_CMAC_Final(u_int8_t [TNT_AES_CMAC_DIGEST_LENGTH], TNT_AES_CMAC_CTX *);

#endif
