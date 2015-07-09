#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

void print_argv(int argc, char *argv[]) {
	int i;
	for (i = 0; i < argc; i++) {
		printf("argv[%d]=%s , ", i, argv[i]);
	}
	printf("\n");
}

int main(int argc, char *argv[]) {
	int nflag, opt;
	int nsecs, tflag;

	print_argv(argc,argv);

	nsecs = 0;
	tflag = 0;
	nflag = 0;
	while ((opt = getopt(argc, argv, "nt:")) != -1) {
		switch (opt) {
		case 'n':
			nflag = 1;
			break;
		case 't':
			nsecs = atoi(optarg);
			tflag = 1;
			break;
		default: /* '?' */
			fprintf(stderr, "Usage: %s [-t nsecs] [-n] name\n", argv[0]);
			exit(EXIT_FAILURE);
		}
	}

	printf("nflag=%d; tflag=%d; optind=%d\n", nflag, tflag, optind);
	print_argv(argc,argv);

	if (optind >= argc) {
		fprintf(stderr, "Expected argument after options\n");
		exit(EXIT_FAILURE);
	}

	printf("name argument = %s\n", argv[optind]);

	/* Other code omitted */

	exit(EXIT_SUCCESS);
}

