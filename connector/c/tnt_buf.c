
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <tnt_error.h>
#include <tnt_mem.h>
#include <tnt_buf.h>

int
tnt_buf_init(tnt_buf_t * buf, int size,
	tnt_buftxf_t tx, tnt_buftxvf_t txv, void * ptr)
{
	buf->tx = tx;
	buf->txv = txv;
	buf->ptr = ptr;
	buf->size = size;
	buf->off = 0;
	buf->top = 0;
	buf->buf = NULL;
	if (size > 0) {
		buf->buf = tnt_mem_alloc(size);
		if (buf->buf == NULL)
			return -1;
	}
	return 0;
}

void
tnt_buf_free(tnt_buf_t * buf)
{
	if (buf->buf)
		tnt_mem_free(buf->buf);
}
