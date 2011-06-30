#ifndef TNT_AUTH_CHAP_H_
#define TNT_AUTH_CHAP_H_

#define TNT_AUTH_CHAP_VERSION     (1)
#define TNT_AUTH_CHAP_MAGIC       ("TNT")
#define TNT_AUTH_CHAP_MAGIC_SIZE  (3)
#define TNT_AUTH_CHAP_TOKEN_SIZE  (8)
#define TNT_AUTH_CHAP_ID_SIZE     (8)
#define TNT_AUTH_CHAP_HASH_SIZE   (16)

typedef struct tnt_auth_chap_hdr_server1_t
	tnt_auth_chap_hdr_server1_t;

typedef struct tnt_auth_chap_hdr_server2_t
	tnt_auth_chap_hdr_server2_t;

typedef struct tnt_auth_chap_hdr_client_t
	tnt_auth_chap_hdr_client_t;

struct tnt_auth_chap_hdr_server1_t {
	unsigned char magic[TNT_AUTH_CHAP_MAGIC_SIZE];
	unsigned char version;
	unsigned char token[TNT_AUTH_CHAP_TOKEN_SIZE];
};

#define TNT_AUTH_CHAP_RESP_FAIL   (0)
#define TNT_AUTH_CHAP_RESP_OK     (1)

struct tnt_auth_chap_hdr_server2_t {
	unsigned char magic[TNT_AUTH_CHAP_MAGIC_SIZE];
	unsigned char resp;
};

#define TNT_AUTH_CHAP_PROTO_ADMIN  (0)
#define TNT_AUTH_CHAP_PROTO_RW     (1)
#define TNT_AUTH_CHAP_PROTO_RO     (2)
#define TNT_AUTH_CHAP_PROTO_FEEDER (3)

struct tnt_auth_chap_hdr_client_t {
	unsigned char magic[TNT_AUTH_CHAP_MAGIC_SIZE];
	unsigned char id[TNT_AUTH_CHAP_ID_SIZE];
	unsigned char hash[TNT_AUTH_CHAP_HASH_SIZE];
	unsigned char proto;
};

tnt_error_t
tnt_auth_chap(tnt_t * t);

#endif
