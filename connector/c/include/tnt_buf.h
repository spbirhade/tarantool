#ifndef TNT_BUF_H_
#define TNT_BUF_H_

typedef int (*tnt_buftxf_t)(void * ptr, char * buf, int size);
typedef int (*tnt_buftxvf_t)(void * ptr, void * iovec, int count);

typedef struct tnt_buf_t tnt_buf_t;

struct tnt_buf_t {
	char * buf;
	int off;
	int top;
	int size;
	tnt_buftxf_t tx;
	tnt_buftxvf_t txv;
	void * ptr;
};

int
tnt_buf_init(tnt_buf_t * buf, int size,
	tnt_buftxf_t tx, tnt_buftxvf_t txv, void * ptr);

void
tnt_buf_free(tnt_buf_t * buf);

#endif
