/*
 * Copyright (C) 2010 Mail.RU
 * Copyright (C) 2010 Yuriy Vostrikov
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

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>

#include <fiber.h>
#include <palloc.h>
#include <salloc.h>
#include <say.h>
#include <stat.h>
#include <tarantool.h>
#include TARANTOOL_CONFIG
#include <tbuf.h>
#include <util.h>

static const char *help =
	"available commands:" CRLF
	" - help" CRLF
	" - exit" CRLF
	" - show info" CRLF
	" - show fiber" CRLF
	" - show configuration" CRLF
	" - show slab" CRLF
	" - show palloc" CRLF
	" - show stat" CRLF
	" - save coredump" CRLF
	" - save snapshot" CRLF
	" - exec module command" CRLF
	" - reload configuration" CRLF;


static const char *unknown_command = "unknown command. try typing help." CRLF;

%%{
	machine admin;
	write data;
}%%


static void
end(struct tbuf *out)
{
	tbuf_printf(out, "..." CRLF);
}

static void
start(struct tbuf *out)
{
	tbuf_printf(out, "---" CRLF);
}

static void
ok(struct tbuf *out)
{
	start(out);
	tbuf_printf(out, "ok" CRLF);
	end(out);
}

static void
fail(struct tbuf *out, struct tbuf *err)
{
	start(out);
	tbuf_printf(out, "fail:%.*s" CRLF, err->len, (char *)err->data);
	end(out);
}

static int
admin_dispatch(void)
{
	struct tbuf *out = tbuf_alloc(fiber->pool);
	struct tbuf *err = tbuf_alloc(fiber->pool);
	int cs;
	char *p, *pe;
	char *strstart, *strend;

	while ((pe = memchr(fiber->rbuf->data, '\n', fiber->rbuf->len)) == NULL) {
		if (fiber_bread(fiber->rbuf, 1) <= 0)
			return 0;
	}

	pe++;
	p = fiber->rbuf->data;

	%%{
		action show_configuration {
			tarantool_cfg_iterator_t *i;
			char *key, *value;

			start(out);
			tbuf_printf(out, "configuration:" CRLF);
			i = tarantool_cfg_iterator_init();
			while ((key = tarantool_cfg_iterator_next(i, &cfg, &value)) != NULL) {
				if (value) {
					tbuf_printf(out, "  %s: \"%s\"" CRLF, key, value);
					free(value);
				} else {
					tbuf_printf(out, "  %s: (null)" CRLF, key);
				}
			}
			end(out);
		}

		action help {
			start(out);
			tbuf_append(out, help, strlen(help));
			end(out);
		}

		action mod_exec {
			start(out);
			mod_exec(strstart, strend - strstart, out);
			end(out);
		}

		action reload_configuration {
			if (reload_cfg(err))
				fail(out, err);
			else
				ok(out);
		}

		action save_snapshot {
			int ret = snapshot(NULL, 0);

			if (ret == 0)
				ok(out);
			else {
				tbuf_printf(err, " can't save snapshot, errno %d (%s)",
					    ret, strerror(ret));

				fail(out, err);
			}
		}

		eol = "\n" | "\r\n";
		show = "sh"("o"("w")?)?;
		info = "in"("f"("o")?)?;
		check = "ch"("e"("c"("k")?)?)?;
		configuration = "co"("n"("f"("i"("g"("u"("r"("a"("t"("i"("o"("n")?)?)?)?)?)?)?)?)?)?)?;
		fiber = "fi"("b"("e"("r")?)?)?;
		slab = "sl"("a"("b")?)?;
		mod = "mo"("d")?;
		palloc = "pa"("l"("l"("o"("c")?)?)?)?;
		stat = "st"("a"("t")?)?;
		help = "h"("e"("l"("p")?)?)?;
		exit = "e"("x"("i"("t")?)?)? | "q"("u"("i"("t")?)?)?;
		save = "sa"("v"("e")?)?;
		coredump = "co"("r"("e"("d"("u"("m"("p")?)?)?)?)?)?;
		snapshot = "sn"("a"("p"("s"("h"("o"("t")?)?)?)?)?)?;
		exec = "ex"("e"("c")?)?;
		string = [^\r\n]+ >{strstart = p;}  %{strend = p;};
		reload = "re"("l"("o"("a"("d")?)?)?)?;

		commands = (help			%help						|
			    exit			%{return 0;}					|
			    show " "+ info		%{start(out); mod_info(out); end(out);}		|
			    show " "+ fiber		%{start(out); fiber_info(out); end(out);}	|
			    show " "+ configuration 	%show_configuration				|
			    show " "+ slab		%{start(out); slab_stat(out); end(out);}	|
			    show " "+ palloc		%{start(out); palloc_stat(out); end(out);}	|
			    show " "+ stat		%{start(out); stat_print(out);end(out);}	|
			    save " "+ coredump		%{coredump(60); ok(out);}			|
			    save " "+ snapshot		%save_snapshot					|
			    exec " "+ string		%mod_exec					|
			    check " "+ slab		%{slab_validate(); ok(out);}			|
			    reload " "+ configuration	%reload_configuration);

	        main := commands eol;
		write init;
		write exec;
	}%%

	tbuf_ltrim(fiber->rbuf, (void *)pe - (void *)fiber->rbuf->data);

	if (p != pe) {
		start(out);
		tbuf_append(out, unknown_command, strlen(unknown_command));
		end(out);
	}

	return fiber_write(out->data, out->len);
}


static void
admin_handler(void *_data __attribute__((unused)))
{
	for (;;) {
		if (admin_dispatch() <= 0)
			return;
		fiber_gc();
	}
}

int
admin_init(void)
{
	if (fiber_server(cfg.admin_port, admin_handler, NULL, NULL) == NULL) {
		say_syserror("can't bind to %d", cfg.admin_port);
		return -1;
	}
	return 0;
}



/*
 * Local Variables:
 * mode: c
 * End:
 * vim: syntax=c
 */
