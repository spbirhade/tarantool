
/*
 * Copyright (C) 2011 Mail.RU
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <tnt_error.h>
#include <tnt_mem.h>
#include <tnt_opt.h>
#include <tnt_buf.h>
#include <tnt.h>
#include <tnt_io.h>
#include <tnt_auth.h>
#include <tnt_aes.h>
#include <tnt_cmac.h>
#include <tnt_auth_chap.h>
#include <tnt_auth_sasl.h>

tnt_error_t
tnt_auth_validate(tnt_t * t)
{
	switch (t->opt.auth) {
	case TNT_AUTH_NONE:
		return TNT_EOK;
	case TNT_AUTH_CHAP:
		if (t->opt.auth_id == NULL)
			return TNT_EBADVAL;
		if (t->opt.auth_key == NULL)
			return TNT_EBADVAL;
		if ((t->opt.auth_id_size + 1) > TNT_AUTH_CHAP_ID_SIZE)
			return TNT_EBADVAL;
		if (t->opt.auth_key_size != TNT_AES_CMAC_KEY_LENGTH)
			return TNT_EBADVAL;
		break;
	case TNT_AUTH_SASL:
		if (t->opt.auth_mech == NULL)
			return TNT_EBADVAL;
		break;
	}
	return TNT_EOK;
}

tnt_error_t
tnt_auth(tnt_t * t)
{
	switch (t->opt.auth) {
	case TNT_AUTH_NONE:
		return TNT_EOK;
	case TNT_AUTH_CHAP:
		return tnt_auth_chap(t);
	case TNT_AUTH_SASL:
		return tnt_auth_sasl(t);
	}
	return TNT_EOK;
}
