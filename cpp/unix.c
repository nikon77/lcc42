#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "cpp.h"

extern	int    getopt(int, char *const *, const char *);
extern	char   *optarg, rcsid[];
extern	int    optind;

int		verbose; /* 打印详细信息的选项flag */
int		Mflag;	/* only print active include files(生成C文件的依赖关系的选项flag) */
char	*objname; /* "src.o: " 目标文件名 */
int		Cplusplus = 1; /* =1，默认兼容C++的单行注释 */

/**
 * setup - 建立关键字hash表;处理命令行选项、参数;输出行控制信息
 */
void setup(int argc, char **argv) {
	int c, i;
	FILE *fd;
	char *fp, *dp;
	Tokenrow tr;
	extern void setup_kwtab(void);

	setup_kwtab(); /* 建立关键字hash表 */
	while ((c = getopt(argc, argv, "MNOVv+I:D:U:F:lg")) != -1) /* 如果选项字符后面跟一个冒号说明该选项有附加的参数 */
		switch (c) {
		case 'N': /* 不包含系统头文件目录（TODO：要保证命令行上，包含系统头文件目录的-I选项在-N选项之前出现，否则程序行为不正确） */
			for (i=0; i< NINCLUDE; i++)
				if (includelist[i].always==1)
					includelist[i].deleted = 1; /* 使always成员无效（即不包含该头文件目录项） */
			break;
		case 'I': /* 附加一个包含目录（从倒数第2项往前插入include目录） */
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
		case 'D': /* 定义一个宏 */
		case 'U': /* 取消一个宏 */
			setsource("<cmdarg>", NULL, optarg); /* 在输入源栈的栈顶添加一个新的输入源节点 */
			maketokenrow(3, &tr); /* 创建一个 token row */
			gettokens(&tr, 1);	/* 从输入源得到一个token row */
			doadefine(&tr, c);	/* 定义（或取消定义）该宏 */
			unsetsource(); /* 取消输入源栈的输入源节点 */
			break;
		case 'M': /* 生成c文件的依赖关系 */
			Mflag++;
			break;
		case 'v': /* 打印版本信息 */
			fprintf(stderr, "%s%s\n", argv[0], rcsid);
			break;
		case 'V': /* 输出详细信息 */
			verbose++;
			break;
		case '+': /* 兼容C++选项（NOTE:lcc默认是兼容C++选项的） */
			Cplusplus++;
			break;
		default: /* c == '?' (unknown option...) */
			break;
		}
	dp = "."; /* 默认的路径 */
	fp = "<stdin>"; /* 默认的输入文件名是标准输入 */
	fd = stdin; /* 默认设定fd为标准输入stdin文件指针 */
	if (optind < argc) { /* 如果命令行上有输入文件的话 */
		if ((fp = strrchr(argv[optind], '/')) != NULL) { /* 从右往左搜索字符'/' */
			int len = fp - argv[optind]; /* 计算路径的dirname的长度. (man 1 dirname) */
			dp = (char*)newstring((uchar*)argv[optind], len+1, 0); /* 将dirname字符串复制到dp中 */
			dp[len] = '\0'; /* 设置字符串结尾的空字符 */
		}
		fp = (char*)newstring((uchar*)argv[optind], strlen(argv[optind]), 0); /* 得到输入源文件的文件名 */
		if ((fd = fopen(fp, "r")) == NULL) /* 以只读模式打开输入源文件 */
			error(FATAL, "Can't open input file %s", fp); /* 如果打开失败，则打印警告信息并退出进程 */
	}
	if (optind+1 < argc) { /* 如果命令行上还指定了输出文件（一般扩展名为 .i） */
		FILE *fdo = freopen(argv[optind+1], "w", stdout); /* 将标准输出重定向到文件 */
		if (fdo == NULL)
			error(FATAL, "Can't open output file %s", argv[optind+1]);
	}
	if(Mflag) /* 如果开启了生成filename.o文件的依赖关系的flag选项 */
		setobjname(fp); /* 那么设定目标文件名 */
	includelist[NINCLUDE-1].always = 0;
	includelist[NINCLUDE-1].file = dp; /* 将当前目录放进头文件包含目录列表中（但暂不开启always flag） */
	setsource(fp, fd, NULL); /* 将命令行上的c文件（或标准输入）压入输入源栈 */
}



/**
 * memmove - 内存复制
 * @dp: 目标地址
 * @sp: 源地址
 * @n:  字节数
 * 返回值: 返回0表示n<=0;
 * NOTE: memmove is defined here because some vendors don't provide it at
 * all and others do a terrible job (like calling malloc)
 * 注意：其实这个memmove函数只能确保复制后的dp是原来sp中的备份，
 * 并不能确保在memmove返回后sp中的内容还是原来的内容。
 * 换句话说：事情的关键不在于你做什么，而在于你的目的是什么。
 * 很显然，这个函数的目的只是让dp是sp原来内容的拷贝。
 * 不废话了...
 */
void * memmove(void *dp, const void *sp, size_t n) {
	unsigned char *cdp, *csp;

	if (n <= 0) /* 如果n小于等于0 */
		return 0; /* 直接返回0 */
	cdp = dp; /* 保存目标地址 */
	csp = (unsigned char *)sp; /* 保存源地址 */
	if (cdp < csp) { /* 如果目标地址小于源地址 */
		do {
			*cdp++ = *csp++;
		} while (--n);
	} else { /* 如果目标地址大于源地址 */
		cdp += n;
		csp += n;
		do {
			*--cdp = *--csp;
		} while (--n);
	}
	return 0;
}
