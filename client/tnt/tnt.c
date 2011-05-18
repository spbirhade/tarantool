
#include <stdlib.h>
#include <stdio.h>

#include <libtnt.h>

int 
main(int argc, char * argv[])
{
	(void)argc;
	(void)argv;

	tnt_t * t = tnt_init(TNT_PROTO_RW, 8096, 8096);

	if (t == NULL) {

		printf("tnt_init() failed\n");
		return 1;
	}

	if (tnt_set_auth(t, TNT_AUTH_CHAP,
			"test",
			(unsigned char*)"1234567812345678", 16) == -1) {

		printf("tnt_init_auth() failed: %s\n", tnt_perror(t));
		return 1;
	}

	if (tnt_connect(t, "localhost", 15312) == -1) {

		printf("tnt_connect() failed: %s\n", tnt_perror(t));
		return 1;
	}

	printf("connected\n");

	tnt_free(t);
	return 0;
}
