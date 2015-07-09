#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "cpp.h"

extern	int getopt(int, char *const *, const char *);
extern	char	*optarg, rcsid[];
extern	int	optind;
int		verbose;
int		Mflag;	/* only print active include files */
char	*objname; /* "src.$O: " */
int		Cplusplus = 1; /* =1，默认兼容C++的单行注释 */

void print_argv(int argc, char *argv[]) {
	int i;
	for (i = 0; i < argc; i++) {
		printf("[%d]=%s , ", i, argv[i]);
	}
	printf("\n");
}

/**
 * setup - TODO
 */
void setup(int argc, char **argv)
{
	int c, i;
	FILE *fd;
	char *fp, *dp;
	Tokenrow tr;
	extern void setup_kwtab(void);

	setup_kwtab(); /* 建立关键字hash表 */
	print_argv(argc,argv);
	while ((c = getopt(argc, argv, "MNOVv+I:D:U:F:lg")) != -1)
		switch (c) {
		case 'N': /* 不包含系统头文件目录 */
			for (i=0; i<NINCLUDE; i++)
				if (includelist[i].always==1)
					includelist[i].deleted = 1;
			break;
		case 'I': /* 附加一个包含目录（从后往前插入include目录） */
			for (i=NINCLUDE-2; i>=0; i--) {
				if (includelist[i].file==NULL) {
					includelist[i].always = 1;
					includelist[i].file = optarg;
					break;
				}
			}
			if (i<0)
				error(FATAL, "Too many -I directives");
			break;
		case 'D':
		case 'U':
			setsource("<cmdarg>", NULL, optarg); /* 在输入源栈的栈顶添加一个新的输入源节点 */
			maketokenrow(3, &tr); /* 创建一个token row */
			gettokens(&tr, 1);	/* 从输入源得到一个token row */
			doadefine(&tr, c);
			unsetsource(); /* 取消输入源栈的输入源节点 */
			break;
		case 'M': /* TODO: 这个选项是干嘛的？ */
			Mflag++;
			break;
		case 'v': /* 打印版本信息 */
			fprintf(stderr, "%s %s\n", argv[0], rcsid);
			break;
		case 'V': /* 输出详细信息 */
			verbose++;
			break;
		case '+': /* 兼容C++选项 */
			Cplusplus++;
			break;
		default: /* '?' */
			break;
		}
	print_argv(argc,argv);
	dp = ".";
	fp = "<stdin>";
	fd = stdin;
	if (optind < argc) {
		if ((fp = strrchr(argv[optind], '/')) != NULL) {
			int len = fp - argv[optind];
			dp = (char*)newstring((uchar*)argv[optind], len+1, 0);
			dp[len] = '\0';
		}
		fp = (char*)newstring((uchar*)argv[optind], strlen(argv[optind]), 0);
		if ((fd = fopen(fp, "r")) == NULL)
			error(FATAL, "Can't open input file %s", fp);
	}
	if (optind+1 < argc) {
		FILE *fdo = freopen(argv[optind+1], "w", stdout);
		if (fdo == NULL)
			error(FATAL, "Can't open output file %s", argv[optind+1]);
	}
	if(Mflag)
		setobjname(fp);
	includelist[NINCLUDE-1].always = 0;
	includelist[NINCLUDE-1].file = dp;
	setsource(fp, fd, NULL);
}



/* memmove is defined here because some vendors don't provide it at
   all and others do a terrible job (like calling malloc) */
void *
memmove(void *dp, const void *sp, size_t n)
{
	unsigned char *cdp, *csp;

	if (n<=0)
		return 0;
	cdp = dp;
	csp = (unsigned char *)sp;
	if (cdp < csp) {
		do {
			*cdp++ = *csp++;
		} while (--n);
	} else {
		cdp += n;
		csp += n;
		do {
			*--cdp = *--csp;
		} while (--n);
	}
	return 0;
}
