#include <stdio.h>
#include <string.h>
#include "cpp.h"

/* TODO: 该文件需要精简 */

int opterr = 1; /* 非0时，打印错误提示 */
int optind = 0; /* The variable optind is the index of the next element to be processed in argv. */
int optopt; /* getopt()函数返回时其中保存了选项flag字符，例如：如果选项为-U,那么optopt就保存字符'U' */
char * optarg = 0; /* getopt()函数返回时其中保存了该option后面的argument字符串 */
static char *nextchar;

struct option {
	const char *name;
	int has_arg;
	int *flag;
	int val;
};

/* ordering的值永远是PERMUTE */
#define PERMUTE 1
int ordering = PERMUTE;

static int first_nonopt;
static int last_nonopt;

static void exchange(char **argv) {
	int nonopts_size = (last_nonopt - first_nonopt) * sizeof(char *);
	char **temp = (char **) domalloc(nonopts_size);

	memcpy(temp, &argv[first_nonopt], nonopts_size);
	memcpy(&argv[first_nonopt], &argv[last_nonopt],
			(optind - last_nonopt) * sizeof(char *));
	memcpy(&argv[first_nonopt + optind - last_nonopt], temp, nonopts_size);

	first_nonopt += (optind - last_nonopt);
	last_nonopt = optind;
}

int _getopt_internal(int argc, char * const *argv, const char *optstring,
		const struct option *longopts, int *longind, int long_only) {
	int option_index;

	optarg = 0;

	if (optind == 0) {
		first_nonopt = last_nonopt = optind = 1;
		nextchar = NULL;
	}

	if (nextchar == NULL || *nextchar == '\0') {
		if (ordering == PERMUTE) {
			if (first_nonopt != last_nonopt && last_nonopt != optind)
				exchange((char **) argv);
			else if (last_nonopt != optind)
				first_nonopt = optind;
			while (optind < argc
					&& (argv[optind][0] != '-' || argv[optind][1] == '\0'))
				optind++;
			last_nonopt = optind;
		}

		if (optind != argc && !strcmp(argv[optind], "--")) {
			optind++;

			if (first_nonopt != last_nonopt && last_nonopt != optind)
				exchange((char **) argv);
			else if (first_nonopt == last_nonopt)
				first_nonopt = optind;
			last_nonopt = argc;

			optind = argc;
		}

		if (optind == argc) {
			if (first_nonopt != last_nonopt)
				optind = first_nonopt;
			return EOF;
		}

		if (argv[optind][1] == '\0') {
			optarg = argv[optind++];
			return 1;
		}

		nextchar = (argv[optind] + 1
				+ (longopts != NULL && argv[optind][1] == '-'));
	}

	if (longopts != NULL
			&& ((argv[optind][0] == '-' && (argv[optind][1] == '-' || long_only)))) {
		const struct option *p;
		char *s = nextchar;
		int exact = 0;
		int ambig = 0;
		const struct option *pfound = NULL;
		int indfound;

		while (*s && *s != '=')
			s++;

		/* Test all options for either exact match or abbreviated matches.  */
		for (p = longopts, option_index = 0; p->name; p++, option_index++)
			if (!strncmp(p->name, nextchar, s - nextchar)) {
				if (s - nextchar == strlen(p->name)) {
					/* Exact match found.  */
					pfound = p;
					indfound = option_index;
					exact = 1;
					break;
				} else if (pfound == NULL) {
					/* First nonexact match found.  */
					pfound = p;
					indfound = option_index;
				} else
					/* Second nonexact match found.  */
					ambig = 1;
			}

		if (ambig && !exact) {
			if (opterr)
				fprintf(stderr, "%s: option `%s' is ambiguous\n", argv[0],
						argv[optind]);
			nextchar += strlen(nextchar);
			optind++;
			return '?';
		}

		if (pfound != NULL) {
			option_index = indfound;
			optind++;
			if (*s) {
				/* Don't test has_arg with >, because some C compilers don't
				 allow it to be used on enums. */
				if (pfound->has_arg)
					optarg = s + 1;
				else {
					if (opterr) {
						if (argv[optind - 1][1] == '-')
							/* --option */
							fprintf(stderr,
									"%s: option `--%s' doesn't allow an argument\n",
									argv[0], pfound->name);
						else
							/* +option or -option */
							fprintf(stderr,
									"%s: option `%c%s' doesn't allow an argument\n",
									argv[0], argv[optind - 1][0], pfound->name);
					}
					nextchar += strlen(nextchar);
					return '?';
				}
			} else if (pfound->has_arg == 1) {
				if (optind < argc)
					optarg = argv[optind++];
				else {
					if (opterr)
						fprintf(stderr,
								"%s: option `%s' requires an argument\n",
								argv[0], argv[optind - 1]);
					nextchar += strlen(nextchar);
					return '?';
				}
			}
			nextchar += strlen(nextchar);
			if (longind != NULL)
				*longind = option_index;
			if (pfound->flag) {
				*(pfound->flag) = pfound->val;
				return 0;
			}
			return pfound->val;
		}

		if (!long_only
				|| argv[optind][1]
						== '-'|| strchr(optstring, *nextchar) == NULL) {
			if (opterr) {
				if (argv[optind][1] == '-')
					/* --option */
					fprintf(stderr, "%s: unrecognized option `--%s'\n", argv[0],
							nextchar);
				else
					/* +option or -option */
					fprintf(stderr, "%s: unrecognized option `%c%s'\n", argv[0],
							argv[optind][0], nextchar);
			}
			nextchar += strlen(nextchar);
			optind++;
			return '?';
		}
	}

	/* Look at and handle the next option-character.  */

	{
		char c = *nextchar++;
		char *temp = strchr(optstring, c);

		/* Increment `optind' when we start to process its last character.  */
		if (*nextchar == '\0')
			optind++;

		if (temp == NULL || c == ':') {
			if (opterr) {
				if (c < 040 || c >= 0177)
					fprintf(stderr,
							"%s: unrecognized option, character code 0%o\n",
							argv[0], c);
				else
					fprintf(stderr, "%s: unrecognized option `-%c'\n", argv[0],
							c);
			}
			return '?';
		}
		if (temp[1] == ':') {
			if (temp[2] == ':') {
				/* This is an option that accepts an argument optionally.  */
				if (*nextchar != '\0') {
					optarg = nextchar;
					optind++;
				} else
					optarg = 0;
				nextchar = NULL;
			} else {
				/* This is an option that requires an argument.  */
				if (*nextchar != 0) {
					optarg = nextchar;
					/* If we end this ARGV-element by taking the rest as an arg,
					 we must advance to the next element now.  */
					optind++;
				} else if (optind == argc) {
					if (opterr)
						fprintf(stderr,
								"%s: option `-%c' requires an argument\n",
								argv[0], c);
					c = '?';
				} else
					/* We already incremented `optind' once;
					 increment it again when taking next ARGV-elt as argument.  */
					optarg = argv[optind++];
				nextchar = NULL;
			}
		}
		return c;
	}
}

/**
 * 注意：该版本的getopt和UNIX系统的getopt是有细微差别的：
 * 此版本的getopt函数没有实现opstring中的“首字符是+号”的功能。
 */
int getopt(int argc, char * const *argv, const char *optstring) {
	return _getopt_internal(argc, argv, optstring, (const struct option *) 0,
			(int *) 0, 0);
}
