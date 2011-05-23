
/*
 * Copyright (C) 2010 Mail.RU
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

#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>

#include <third_party/queue.h>
#include <gsasl.h>

#include <fiber.h>
#include <say.h>
#include <user.h>
#include <plug.h>
#include <auth.h>

#include <cfg/tarantool_box_cfg.h>

extern struct tarantool_cfg cfg;
static Gsasl * sasl_ctx = NULL;

static int
auth_sasl_getcred(char * id, struct user ** u, enum auth_result * ar)
{
	char * proto = strrchr(id, ':');

	if (proto == NULL)
		return -1;

	char user[64];
	snprintf(user, sizeof(user), "%.*s", (int)(proto - id), id);

	proto++;
	if (!strcmp(proto, "rw"))
		*ar = AUTH_PROTO_RW;
	else
	if (!strcmp(proto, "ro"))
		*ar = AUTH_PROTO_RO;
	else
	if (!strcmp(proto, "admin"))
		*ar = AUTH_PROTO_ADMIN;
	else
	if (!strcmp(proto, "feeder"))
		*ar = AUTH_PROTO_FEEDER;
	else
		return -1;

	*u = user_match(user);

	return (*u) ? 0 : -1; 
}

static int
auth_sasl_cb_plain(Gsasl * ctx, Gsasl_session * session)
{
	int rc = GSASL_AUTHENTICATION_ERROR;

	char * id = (char*)gsasl_property_fast(session, GSASL_AUTHID);
	char * key = (char*)gsasl_property_fast(session, GSASL_PASSWORD);

	if (id == NULL || key == NULL)
		goto fail;

	enum auth_result ar = AUTH_FAIL;
	struct user * u = NULL;

	if (auth_sasl_getcred(id, &u, &ar) == -1)
		goto fail;

	if (strncmp((char*)u->key, key, u->key_size))
		goto fail;

	/* user capabilities against protocol can be
	 * examined here */

	gsasl_callback_hook_set(ctx, (void*)ar);
	gsasl_session_hook_set(session, (void*)u);

	rc = GSASL_OK;
fail:
	return rc;
}

static int
auth_sasl_cb_anonymous(Gsasl * ctx, Gsasl_session * session)
{
	int rc = GSASL_AUTHENTICATION_ERROR;

	char * token =
		(char*)gsasl_property_fast(session, GSASL_ANONYMOUS_TOKEN);

	if (token == NULL)
		goto fail;

	enum auth_result ar = AUTH_FAIL;
	struct user * u = NULL;

	if (auth_sasl_getcred(token, &u, &ar) == -1)
		goto fail;

	/* no user key check for "anonymous" login, but
	 * user still have to exists */

	gsasl_callback_hook_set(ctx, (void*)ar);
	gsasl_session_hook_set(session, (void*)u);

	rc = GSASL_OK;
fail:
	return rc;
}

static int
auth_sasl_cb_cram(Gsasl * ctx, Gsasl_session * session)
{
	/* challenge-response authentication mechanism (cram) 
	 * for CRAM-MD5, SCRAM-SHA-1, DIGEST-MD5 */

	int rc = GSASL_AUTHENTICATION_ERROR;

	char * id = (char*)gsasl_property_fast(session, GSASL_AUTHID);

	if (id == NULL)
		goto fail;

	enum auth_result ar = AUTH_FAIL;
	struct user * u = NULL;

	if (auth_sasl_getcred(id, &u, &ar) == -1)
		goto fail;

	gsasl_property_set(session, GSASL_PASSWORD, (char*)u->key);

	/* user capabilities against protocol can be
	 * examined here */

	gsasl_callback_hook_set(ctx, (void*)ar);
	gsasl_session_hook_set(session, (void*)u);

	rc = GSASL_OK;
fail:
	return rc;
}

static int
auth_sasl_cb(Gsasl * ctx, Gsasl_session * session, Gsasl_property prop)
{
	int rc = GSASL_NO_CALLBACK;

	if (prop == GSASL_VALIDATE_SIMPLE)
		return auth_sasl_cb_plain(ctx, session);
	else
	if (prop == GSASL_VALIDATE_ANONYMOUS)
		return auth_sasl_cb_anonymous(ctx, session);
	else
	if (prop == GSASL_PASSWORD)
		return auth_sasl_cb_cram(ctx, session);

	return rc;
}

static int
auth_sasl_negotiate(Gsasl_session * session)
{
	int rc = 0;
	char * p = NULL;

	while (1) {

		if (fiber_bread(fiber->rbuf, 0) <= 0)
			return -1;

		((char*)fiber->rbuf->data)[fiber->rbuf->len] = 0;

		rc = gsasl_step64(session, fiber->rbuf->data, &p);

		if (rc != GSASL_NEEDS_MORE && rc != GSASL_OK)
			break;

		int len = strlen(p);

		if (len || rc == GSASL_NEEDS_MORE) {

			add_iov((void*)p, len + 1);

			ssize_t r = fiber_flush_output();
			
			if (r < 0) {
				free(p);
				return -1;
			}
		}

		free(p);

		tbuf_reset(fiber->rbuf);

		if (rc == GSASL_OK)
			break;
	}

	return (rc == GSASL_OK) ? 0 : -1;
}

static bool
auth_sasl_init(void)
{
	if (gsasl_init(&sasl_ctx) != GSASL_OK)
		return false;

	gsasl_callback_set(sasl_ctx, auth_sasl_cb);
	return true;
}

static void
auth_sasl_free(void)
{
	if (sasl_ctx)
		gsasl_done(sasl_ctx);
}

static enum auth_result
auth_sasl_handshake(struct user ** u)
{
	*u = NULL;
	Gsasl_session * session = NULL;

	if (gsasl_server_start(sasl_ctx, cfg.auth_sasl, &session) != GSASL_OK)
		return AUTH_FAIL;

	int r = auth_sasl_negotiate(session);

	if (r < 0) {
		gsasl_finish(session);
		return AUTH_FAIL;
	}

	*u = (struct user*)gsasl_session_hook_get(session);

	int proto = (int)gsasl_callback_hook_get(sasl_ctx);

	gsasl_finish(session);
	return proto;
}

static
struct plug_desc auth_sasl_desc = {

	.name   = "auth_sasl",
	.author = "pmwka",
	.desc   = "tarantool SASL authentication plugin"
};

int
tnt_module_init(struct plug_desc **pd)
{
	*pd = &auth_sasl_desc;

	char * sasl_mechs[] = {
		"PLAIN", "ANONYMOUS", "CRAM-MD5", "DIGEST-MD5", "SCRAM-SHA-1", NULL
	};

	int i;
	for (i = 0 ; sasl_mechs[i] ; i++)
		if (!strcmp(sasl_mechs[i], cfg.auth_sasl))
			break;

	if (sasl_mechs[i] == NULL) {
		say_warn("[auth_sasl]: unsupported authentication mechanism %s",
			cfg.auth_sasl);
		return TNT_MODULE_FAIL;
	}

	int result =
			auth_add("sasl", "SASL authentication",
				auth_sasl_init,
				auth_sasl_free,
				auth_sasl_handshake);

	if (result == -1)
		return TNT_MODULE_FAIL;

	return TNT_MODULE_OK;
}
