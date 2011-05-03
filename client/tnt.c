
#include <stdlib.h>
#include <stdio.h>

#include <connector/c/client.h>
#include <auth_chap.h>

int 
main(int argc, char * argv[])
{
	(void)argc;
	(void)argv;

	struct tnt * tnt = tnt_init();

	int result = tnt_init_auth(tnt,
		TNT_AUTH_CHAP, TNT_PROTO_RW, "test", "1234567812345678", 16);

	if ( result != TNT_EOK ) {

		printf("tnt_init_auth() failed\n");
		return 1;
	}

	result = tnt_connect(tnt, "localhost", 15312);

	if ( result != TNT_EOK ) {

		printf("error: %s\n", tnt_error(result));
		return 1;
	}

	printf("connected\n");

	tnt_free(tnt);

	return 0;
}
