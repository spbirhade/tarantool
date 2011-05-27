
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

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <gsasl.h>

#include <tnt_error.h>
#include <tnt_mem.h>
#include <tnt_aes.h>
#include <tnt_cmac.h>
#include <tnt.h>
#include <tnt_io.h>
#include <tnt_auth_sasl.h>

static char*
tnt_auth_sasl_proto(tnt_t * t)
{
	switch (t->proto) {

		case TNT_PROTO_ADMIN:
			return "admin";

		case TNT_PROTO_RW:
			return "rw";

		case TNT_PROTO_RO:
			return "ro";

		case TNT_PROTO_FEEDER:
			return "feeder";
	}

	return "unknown";
}

static tnt_error_t
tnt_auth_sasl_sendfirst(tnt_t * t, Gsasl_session * session)
{
	int rc = 0;
	char buf[8096], * data = NULL;
	tnt_error_t e;

	do {
		rc = gsasl_step64(session, buf, &data);

		if (rc != GSASL_NEEDS_MORE && rc != GSASL_OK)
			break;

		int len = strlen(data);

		if (len || rc == GSASL_NEEDS_MORE) {
			e = tnt_io_send(t, data, len + 1);

			if (e != TNT_EOK) {

				free(data);
				return e;
			}
		}

		free(data);

		if (rc == GSASL_NEEDS_MORE)
			if (tnt_io_recv_raw(t, buf, sizeof(buf)) == -1)
				return TNT_ESYSTEM;

	} while(rc == GSASL_NEEDS_MORE);

	e = TNT_EAUTH;

	if (rc == GSASL_OK)
		e = TNT_EOK;

	return e;
}

static tnt_error_t
tnt_auth_sasl_do(tnt_t * t, Gsasl * ctx, char * mech)
{
	Gsasl_session * session = NULL;
	if (gsasl_client_start(ctx, mech, &session) != GSASL_OK)
		return TNT_EFAIL;

	char utk[64];
	snprintf(utk, sizeof(utk), "%s:%s",
		(char*)t->auth_id, tnt_auth_sasl_proto(t));

	if (!strcmp(mech, "ANONYMOUS")) {
		gsasl_property_set(session, GSASL_ANONYMOUS_TOKEN, utk);
		goto negotiate;
	}

	gsasl_property_set(session, GSASL_AUTHID, utk);
	gsasl_property_set(session, GSASL_PASSWORD, (char*)t->auth_key);

	if (!strcmp(mech, "DIGEST-MD5")) {
		gsasl_property_set(session, GSASL_SERVICE, "tarantool");

		char host[64];
		if (gethostname(host, sizeof(host)) == -1) {
			gsasl_finish(session);
			return TNT_EFAIL;
		}

		gsasl_property_set(session, GSASL_HOSTNAME, host);
	}

negotiate:; /* < making compiler happy */
	tnt_error_t e = tnt_auth_sasl_sendfirst(t, session);

	gsasl_finish(session);
	return e;
}

tnt_error_t
tnt_auth_sasl(tnt_t * t)
{
	char * sasl_mechs[] = {
		"PLAIN", "ANONYMOUS", "CRAM-MD5", "DIGEST-MD5", "SCRAM-SHA-1", NULL
	};

	int i;
	for (i = 0 ; sasl_mechs[i] ; i++)
		if (!strcmp(sasl_mechs[i], t->auth_mech))
			break;

	if (sasl_mechs[i] == NULL)
		return TNT_EFAIL;

	Gsasl * ctx = NULL;

	if (gsasl_init(&ctx) != GSASL_OK)
		return TNT_EFAIL;

	tnt_error_t r = tnt_auth_sasl_do(t, ctx, t->auth_mech);

	gsasl_done(ctx);
	return r;
}
