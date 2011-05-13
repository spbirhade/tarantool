
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

	tnt_result_t result = tnt_set_auth(t, TNT_AUTH_CHAP,
			"test",
			(unsigned char*)"1234567812345678", 16);

	if ( result != TNT_EOK ) {

		printf("tnt_init_auth() failed: %s\n", tnt_error(result));
		return 1;
	}

	result = tnt_connect(t, "localhost", 15312);

	if ( result != TNT_EOK ) {

		printf("tnt_connect() failed: %s\n", tnt_error(result));
		return 1;
	}

	printf("connected\n");

	tnt_free(t);
	return 0;
}
