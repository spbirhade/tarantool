
#include <stdlib.h>
#include <stdio.h>

#include <libtnt.h>

int 
main(int argc, char * argv[])
{
	(void)argc;
	(void)argv;

	tnt_t * t = tnt_alloc();
	if (t == NULL) {
		printf("tnt_alloc() failed\n");
		return 1;
	}

	tnt_set(t, TNT_OPT_HOSTNAME, "localhost");

	if (tnt_init(t) == -1) {
		printf("tnt_init() failed\n");
		return 1;
	}

	if (tnt_connect(t) == -1) {
		printf("tnt_connect() failed: %s\n", tnt_perror(t));
		return 1;
	}

	printf("connected\n");

	tnt_free(t);
	return 0;
}
