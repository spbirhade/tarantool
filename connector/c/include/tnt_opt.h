#ifndef TNT_OPT_H_
#define TNT_OPT_H_

typedef struct tnt_opt_t tnt_opt_t;

typedef enum {
	TNT_AUTH_NONE,
	TNT_AUTH_CHAP,
	TNT_AUTH_SASL
} tnt_auth_t;

typedef enum {
	TNT_PROTO_ADMIN,
	TNT_PROTO_RW,
	TNT_PROTO_RO,
	TNT_PROTO_FEEDER
} tnt_proto_t;

typedef enum {
	TNT_OPT_PROTO,
	TNT_OPT_HOSTNAME,
	TNT_OPT_PORT,
	TNT_OPT_TMOUT_CONNECT,
	TNT_OPT_TMOUT_RECV,
	TNT_OPT_TMOUT_SEND,
	TNT_OPT_SEND_CB,
	TNT_OPT_SEND_CBV,
	TNT_OPT_SEND_CB_ARG,
	TNT_OPT_SEND_BUF,
	TNT_OPT_RECV_CB,
	TNT_OPT_RECV_CB_ARG,
	TNT_OPT_RECV_BUF,
	TNT_OPT_AUTH,
	TNT_OPT_AUTH_ID,
	TNT_OPT_AUTH_KEY,
	TNT_OPT_AUTH_MECH,
	TNT_OPT_MALLOC,
	TNT_OPT_REALLOC,
	TNT_OPT_FREE,
	TNT_OPT_DUP
} tnt_opt_type_t;

struct tnt_opt_t {
	tnt_proto_t proto;
	char * hostname;
	int port;
	int tmout_connect;
	int tmout_recv;
	int tmout_send;
	void * send_cb;
	void * send_cbv;
	void * send_cb_arg;
	int send_buf;
	void * recv_cb;
	void * recv_cb_arg;
	int recv_buf;
	tnt_auth_t auth;
	char * auth_id;
	int auth_id_size;
	unsigned char * auth_key;
	int auth_key_size;
	char * auth_mech;
	tnt_mallocf_t malloc;
	tnt_reallocf_t realloc;
	tnt_dupf_t dup;
	tnt_freef_t free;
};

void
tnt_opt_init(tnt_opt_t * opt);

void
tnt_opt_free(tnt_opt_t * opt);

tnt_error_t
tnt_opt_set(tnt_opt_t * opt, tnt_opt_type_t name, va_list args);

#endif
