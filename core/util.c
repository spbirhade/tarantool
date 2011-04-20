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

#include "config.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#ifdef HAVE_BFD
#include <bfd.h>
#endif /* HAVE_BFD */

#include <util.h>
#include <fiber.h>

#ifndef HAVE_LIBC_STACK_END
void *__libc_stack_end;
#endif

void
close_all_xcpt(int fdc, ...)
{
	int keep[fdc];
	va_list ap;
	struct rlimit nofile;

	va_start(ap, fdc);
	for (int j = 0; j < fdc; j++) {
		keep[j] = va_arg(ap, int);
	}
	va_end(ap);

	if (getrlimit(RLIMIT_NOFILE, &nofile) != 0)
		nofile.rlim_cur = 10000;

	for (int i = 3; i < nofile.rlim_cur; i++) {
		bool found = false;
		for (int j = 0; j < fdc; j++) {
			if (keep[j] == i) {
				found = true;
				break;
			}
		}
		if (!found)
			close(i);
	}
}

void
coredump(int dump_interval)
{
	static time_t last_coredump = 0;
	time_t now = time(NULL);

	if (now - last_coredump < dump_interval)
		return;

	last_coredump = now;

	if (fork() == 0) {
		close_all_xcpt(0);
#ifdef ENABLE_GCOV
		__gcov_flush();
#endif
		abort();
	}
}

void *
xrealloc(void *ptr, size_t size)
{
	void *ret = realloc(ptr, size);
	if (size > 0 && ret == NULL)
		abort();
	return ret;
}

#ifdef ENABLE_BACKTRACE

/*
 * We use a global static buffer because it is too late to do any
 * allocation when we are printing backtrace and fiber stack is
 * small.
 */

static char backtrace_buf[4096 * 4];

/*
 * note, stack unwinding code assumes that binary is compiled with frame pointers
 */

struct frame {
	struct frame *rbp;
	void *ret;
};

char *
backtrace(void *frame_, void *stack, size_t stack_size)
{
	struct frame *frame = frame_;
	void *stack_top = stack + stack_size;
	void *stack_bottom = stack;

	char *p = backtrace_buf;
	size_t r, len = sizeof(backtrace_buf);
	while (stack_bottom <= (void *)frame && (void *)frame < stack_top) {
		r = snprintf(p, len, "        - { frame: %p, caller: %p",
			     (void *)frame + 2 * sizeof(void *), frame->ret);

		if (r >= len)
			goto out;
		p += r;
		len -= r;

#ifdef HAVE_BFD
		struct symbol *s = addr2symbol(frame->ret);
		if (s != NULL) {
			r = snprintf(p, len, " <%s+%"PRI_SZ"> ", s->name, frame->ret - s->addr);
			if (r >= len)
				goto out;
			p += r;
			len -= r;

		}
#endif /* HAVE_BFD */
		r = snprintf(p, len, " }\r\n");
		if (r >= len)
			goto out;
		p += r;
		len -= r;

#ifdef HAVE_BFD
		if (s != NULL && strcmp(s->name, "main") == 0)
			break;

#endif
		frame = frame->rbp;
	}
	r = 0;
out:
	p += MIN(len - 1, r);
	*p = 0;
        return backtrace_buf;
}
#endif /* ENABLE_BACKTRACE */

void __attribute__ ((noreturn))
assert_fail(const char *assertion, const char *file, unsigned int line, const char *function)
{
	fprintf(stderr, "%s:%i: %s: assertion %s failed.\n", file, line, function, assertion);

#ifdef ENABLE_BACKTRACE
	void *frame = __builtin_frame_address(0);
	void *stack_top;
	size_t stack_size;

	if (fiber == NULL || fiber->name == NULL || strcmp(fiber->name, "sched") == 0) {
		stack_top = frame; /* we don't know where the system stack top is */
		stack_size = __libc_stack_end - frame;
	} else {
		stack_top = fiber->coro.stack;
		stack_size = fiber->coro.stack_size;
	}

	fprintf(stderr, "%s", backtrace(frame, stack_top, stack_size));
#endif /* ENABLE_BACKTRACE */
	close_all_xcpt(0);
	abort();
}

#ifdef HAVE_BFD
static struct symbol *symbols;
static size_t symbol_count;

int
compare_symbol(const void *_a, const void *_b)
{
	const struct symbol *a = _a, *b = _b;
	if (a->addr > b->addr)
		return 1;
	if (a->addr == b->addr)
		return 0;
	return -1;
}

void
load_symbols(const char *name)
{
	long storage_needed;
	asymbol **symbol_table = NULL;
	long number_of_symbols;
	bfd *h;
	char **matching;
	int j;

	bfd_init();
	h = bfd_openr (name, NULL);
	if (h == NULL)
		goto out;

	if (bfd_check_format(h, bfd_archive))
		goto out;

	if (!bfd_check_format_matches(h, bfd_object, &matching))
		goto out;

	storage_needed = bfd_get_symtab_upper_bound(h);

	if (storage_needed <= 0)
                goto out;

	symbol_table = malloc(storage_needed);
	number_of_symbols = bfd_canonicalize_symtab (h, symbol_table);
	if (number_of_symbols < 0)
		goto out;

	for (int i = 0; i < number_of_symbols; i++) {
		struct bfd_section *section;
		unsigned long int vma, size;
		section = bfd_get_section(symbol_table[i]);
		vma = bfd_get_section_vma(h, section);
		size = bfd_get_section_size(section);

		if (symbol_table[i]->flags & BSF_FUNCTION &&
		    vma + symbol_table[i]->value > 0 &&
		    symbol_table[i]->value < size)
			symbol_count++;
	}

	if (symbol_count == 0)
		goto out;

	j = 0;
	symbols = malloc(symbol_count * sizeof(struct symbol));

	for (int i = 0; i < number_of_symbols; i++) {
		struct bfd_section *section;
		unsigned long int vma, size;
		section = bfd_get_section(symbol_table[i]);
		vma = bfd_get_section_vma(h, section);
		size = bfd_get_section_size(section);

		if (symbol_table[i]->flags & BSF_FUNCTION &&
		    vma + symbol_table[i]->value > 0 &&
		    symbol_table[i]->value < size)
		{
			symbols[j].name = strdup(symbol_table[i]->name);
			symbols[j].addr = (void *)(uintptr_t)(vma + symbol_table[i]->value);
			symbols[j].end = (void *)(uintptr_t)(vma + size);
			j++;
		}
	}

	qsort(symbols, symbol_count, sizeof(struct symbol), compare_symbol);

	for (int j = 0; j < symbol_count - 1; j++)
		symbols[j].end = MIN(symbols[j].end, symbols[j + 1].addr - 1);

out:
	if (symbol_count == 0)
		say_warn("no symbols were loaded");

	if (symbol_table)
		free(symbol_table);
}

struct symbol *
addr2symbol(void *addr)
{
	int low = 0, high = symbol_count, middle = -1;
	struct symbol *ret, key = {.addr = addr};

	while(low < high) {
		middle = low + ((high - low) >> 1);
		int diff = compare_symbol(symbols + middle, &key);

		if (diff < 0) {
			low = middle + 1;
		} else if (diff > 0) {
			high = middle;
		} else {
			ret = symbols + middle;
			goto out;
		}
	}
	ret = symbols + high - 1;
out:
	if (middle != -1 && ret->addr <= key.addr && key.addr <= ret->end)
		return ret;
	return NULL;
}

#endif /* HAVE_BFD */
