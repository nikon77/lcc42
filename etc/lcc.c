/*
 * lcc [ option ]... [ file | -llib ]...
 * front end for the ANSI C compiler
 */
static char version[] = "LCC Driver Version 4.2";

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <signal.h>

/* 如果命令行上没有定义全局变量TEMPDIR,则定义TEMPDIR为"/tmp" */
#ifndef TEMPDIR
#define TEMPDIR "/tmp"
#endif

typedef struct list *List;
struct list {		/* 循环列表节点: */
	char *str;		/* 选项或文件名 */
	List link;		/* 指向列表中下一元素 */
};

static void *alloc(int);
static List append(char *,List);
extern char *basepath(char *);
static int callsys(char *[]);
extern char *concat(char *, char *);
static int compile(char *, char *);
static void compose(char *[], List, List, List);
static void error(char *, char *);
static char *exists(char *);
static char *first(char *);
static int filename(char *, char *);
static List find(char *, List);
static void help(void);
static void initinputs(void);
static void interrupt(int);
static void opt(char *);
static List path2list(const char *);
extern int main(int, char *[]);
extern char *replace(const char *, int, int);
static void rm(List);
extern char *strsave(const char *);
extern char *stringf(const char *, ...);
extern int suffix(char *, char *[], int);
extern char *tempname(char *);

extern int access(char *, int);
extern int getpid(void);

extern char *cpp[], *include[], *com[], *as[],*ld[], inputs[], *suffixes[];
extern int option(char *);

static int errcnt;		/* 记录错误的数量 */
static int Eflag;		/* 若命令行上指明了预处理标志 -E ,则置此标志位 */
static int Sflag;		/* 若命令行上指明了预处理标志 -S ,则置此标志位 */
static int cflag;		/* 若命令行上指明了预处理标志 -c ,则置此标志位 */
static int verbose;		/* incremented for each -v */
static List llist[2];	/* llist[0]是库文件搜索路径列表(例如: -Ldir1 -Ldir2); llist[1]是库文件列表(例如: -lpcre -lpcap) */
static List alist;		/* 传递给汇编器as的选项列表 */
static List clist;		/* 传递给编译器rcc的选项列表 */
static List plist;		/* 编译器预定义的include目录列表+LCCINPUTS中的头文件搜索路径列表 */
static List ilist;		/* 临时存放LCCINPUTS中的头文件搜索路径列表 */
static List rmlist;		/* list of files to remove */
static char *outfile;		/* ld output file or -[cS] object file */
static int ac;			/* argument count */
static char **av;		/* argument vector */
char *tempdir = TEMPDIR;	/* 临时目录 */
static char *progname;		/* 程序名 */
static List lccinputs;		/* 环境变量LCCINPUTS中指定的头文件搜索路径列表 */

int main(int argc, char *argv[]) {
	int i, j, nf;
	
	progname = argv[0]; /* 保存程序名 */
	ac = argc + 50;
	av = alloc(ac*sizeof(char *)); /* 建立一个保存参数列表的向量空间,以备后用 */
	
	/* 设定忽略信号 */
	if (signal(SIGINT, SIG_IGN) != SIG_IGN)
		signal(SIGINT, interrupt);
	if (signal(SIGTERM, SIG_IGN) != SIG_IGN)
		signal(SIGTERM, interrupt);
#ifdef SIGHUP
	if (signal(SIGHUP, SIG_IGN) != SIG_IGN)
		signal(SIGHUP, interrupt);
#endif

	/* 得到当前系统的临时目录 */
	if (getenv("TMP"))
		tempdir = getenv("TMP");
	else if (getenv("TEMP"))
		tempdir = getenv("TEMP");
	else if (getenv("TMPDIR"))
		tempdir = getenv("TMPDIR");
	assert(tempdir);
	i = strlen(tempdir);
	/* 将临时目录字串中的trailing slash去掉 */
	for (; i > 0 && tempdir[i-1] == '/' || tempdir[i-1] == '\\'; i--)
		tempdir[i-1] = '\0';
	/* 如果命令行上没有任何参数或选项,则打印帮助信息后结束进程 */
	if (argc <= 1) {
		help();
		exit(0);
	}
	plist = append("-D__LCC__", 0); /* plist上插入一个符号定义选项 "-D__LCC__" */
	initinputs(); /* 将 LCCINPUTS 中的头文件搜索路径列表附加到系统默认头文件搜索路径列表之前 */
	/* 如果命令行上定义了LCCDIR，则覆盖默认的LCC安装路径 */
	if (getenv("LCCDIR"))
		option(stringf("-lccdir=%s", getenv("LCCDIR")));
	for (nf = 0, i = j = 1; i < argc; i++) {
		if (strcmp(argv[i], "-o") == 0) { /* 如果当前选项是-o选项(注意:此处只处理-o filename的选项,不处理-ofilename的选项) */
			if (++i < argc) { /* 如果-o的下一个选项不越界 */
				if (suffix(argv[i], suffixes, 2) >= 0) { /* 如果 -o filename 选项中filename是.c或.i的源文件 */
					error("-o would overwrite %s", argv[i]); /* print error infomation */
					exit(8); /* exit process */
				}
				outfile = argv[i]; /* 保存输出文件名 */
				//fprintf(stderr,"outfile=%s\n",outfile);
				continue;
			} else { /* 如果-o的下一个选项越界了,例如: lcc filename.c -o */
				error("unrecognized option `%s'", argv[i-1]); /* 打印错误信息 */
				exit(8); /* 结束进程 */
			}
		} else if (strcmp(argv[i], "-target") == 0) { /* 命令行中的 -target [name] 选项被忽略 */
			if (argv[i+1] && *argv[i+1] != '-')
				i++;
			continue;
		} else if (*argv[i] == '-' && argv[i][1] != 'l') { /* 如果是一个选项，且不是 -l 选项 */
			opt(argv[i]); /* 处理该选项.类似 lcc -otest test.c的选项将在此处理 */
			continue;
		} else if (*argv[i] != '-' && suffix(argv[i], suffixes, 3) >= 0) /* 如果是个文件名,且文件名是filename.{c,i,s} */
			nf++; /* 文件计数++ */
		argv[j++] = argv[i]; /* 删除掉前面已经处理过的选项 */
	}
	/* 如果待编译的源文件不止一个，且有-c或-S选项，且命令行上也指定了输出文件，那么-o选项会被忽略 */
	/* 例如: lcc -S -o ouput.o a.c b.c ,在此情况下lcc实际只会输出两个汇编源文件,而忽略-o选项 */
	if ((cflag || Sflag) && outfile && nf != 1) {
		fprintf(stderr, "%s: -o %s ignored\n", progname, outfile); /* 打印命令行上被忽略的输出文件名 */
		outfile = 0;
	}
	argv[j] = 0; /* 标记参数向量结尾的空指针 */
	for (i = 0; include[i]; i++)
		plist = append(include[i], plist); /* 将编译系统预定义的include目录列表插入plist列表中 */
	if (ilist) {
		List b = ilist; /* ilist的列表尾部赋值给b */
		do {
			b = b->link; /* 循环第一次时,b指向ilist的第一个元素(ilist中存放的是LCCINPUTS中的目录列表) */
			plist = append(b->str, plist); /* 将ilist的内容插入plist */
		} while (b != ilist);
	}
	ilist = 0; /* 清空ilist */
	for (i = 1; argv[i]; i++) /* 此时命令行参数向量中只剩下了-l选项或待编译的源文件列表 */
		if (strcmp(argv[i], "-l") == 0 && argv[i+1] && *argv[i+1] != '-') { /* -l file */
			llist[1] = append(argv[i++], llist[1]); /* 将-l添加进llist[1]列表 */
			llist[1] = append(argv[i],   llist[1]); /* 将file添加进llist[1]列表 */
		} else if (*argv[i] == '-') /* 如果当前参数是选项.类似 lcc -lpcre test.c中的-lpcre的选项，将在此处理 */
			opt(argv[i]); /* 处理此库名选项 */
		else { /* 如果当前参数是文件名 */
			char *name = exists(argv[i]);
			if (name) { /* 如果文件可读 */
				if (strcmp(name, argv[i]) != 0
				|| nf > 1 && suffix(name, suffixes, 3) >= 0) /* 如果待处理的文件不在当前目录或者被编译的文件数目大于1且文件扩展名为{c,i,s} */
					fprintf(stderr, "%s:\n", name); /* 则打印一则警告信息，信息内容为 该文件名后跟一个冒号 */
				filename(name, 0); /* 处理名为name的文件 */
			} else /* 如果没找到文件或文件不可被读取 */
				error("can't find `%s'", argv[i]); /* 则打印出错信息 */
		}
	/* 如果错误数等于0且命令行上没有-E,-S,-c标志,且库文件列表非空 */
	if (errcnt == 0 && !Eflag && !Sflag && !cflag && llist[1]) {
		compose(ld, llist[0], llist[1],
			append(outfile ? outfile : concat("a", first(suffixes[4])), 0)); /* 合成ld程序的命令行.第三个参数是是: 最终的可执行程序名 */
		if (callsys(av)) /* 调用ld程序完成链接 */
			errcnt++;
	}
	rm(rmlist);	/* 删除编译中产生的临时文件 */
	return errcnt ? EXIT_FAILURE : EXIT_SUCCESS;
}

/* alloc - allocate n bytes or die */
static void *alloc(int n) {
	static char *avail, *limit;
	
	n = (n + sizeof(char *) - 1)&~(sizeof(char *) - 1);
	if (n >= limit - avail) {
		avail = malloc(n + 4*1024);
		assert(avail);
		limit = avail + n + 4*1024;
	}
	avail += n;
	return avail - n;
}

/* append - append a node with string str onto list, return new list */	
static List append(char *str, List list) { /* 尾插法 */
	List p = alloc(sizeof *p);

	p->str = str;
	if (list) {
		p->link = list->link;
		list->link = p;
	} else
		p->link = p;
	return p;
}

/* basepath - return base name for name, e.g. /usr/drh/foo.c => foo */
char *basepath(char *name) {
	char *s, *b, *t = 0;

	for (b = s = name; *s; s++)
		if (*s == '/' || *s == '\\') {
			b = s + 1;
			t = 0;
		} else if (*s == '.')
			t = s;
	s = strsave(b);
	if (t)
		s[t-b] = 0;
	return s;
}

#ifdef WIN32
#include <process.h>
#else
#define _P_WAIT 0
extern int fork(void);
extern int wait(int *);
extern void execv(const char *, char *[]);

static int _spawnvp(int mode, const char *cmdname, const char *const argv[]) {
	int pid, n, status;

	switch (pid = fork()) {
	case -1:
		fprintf(stderr, "%s: no more processes\n", progname);
		return 100;
	case 0:
		execv(cmdname, (char **)argv);
		fprintf(stderr, "%s: ", progname);
		perror(cmdname);
		fflush(stdout);
		exit(100);
	}
	while ((n = wait(&status)) != pid && n != -1)
		;
	if (n == -1)
		status = -1;
	if (status&0377) {
		fprintf(stderr, "%s: fatal error in %s\n", progname, cmdname);
		status |= 0400;
	}
	return (status>>8)&0377;
}
#endif

/* callsys - execute the command described by av[0...], return status */
static int callsys(char **av) {
	int i, status = 0;
	static char **argv;
	static int argc;

	for (i = 0; av[i] != NULL; i++)
		;
	if (i + 1 > argc) {
		argc = i + 1;
		if (argv == NULL)
			argv = malloc(argc*sizeof *argv);
		else
			argv = realloc(argv, argc*sizeof *argv);
		assert(argv);
	}
	for (i = 0; status == 0 && av[i] != NULL; ) {
		int j = 0;
		char *s;
		for ( ; av[i] != NULL && (s = strchr(av[i], '\n')) == NULL; i++)
			argv[j++] = av[i];
		if (s != NULL) {
			if (s > av[i])
				argv[j++] = stringf("%.*s", s - av[i], av[i]);
			if (s[1] != '\0')
				av[i] = s + 1;
			else
				i++;
		}
		argv[j] = NULL;
		if (verbose > 0) {
			int k;
			fprintf(stderr, "%s", argv[0]);
			for (k = 1; argv[k] != NULL; k++)
				fprintf(stderr, " %s", argv[k]);
			fprintf(stderr, "\n");
		}
		if (verbose < 2)
			status = _spawnvp(_P_WAIT, argv[0], (const char * const *)argv);
		if (status == -1) {
			fprintf(stderr, "%s: ", progname);
			perror(argv[0]);
		}
	}
	return status;
}

/* concat - return concatenation of strings s1 and s2 */
char *concat(char *s1, char *s2) {
	int n = strlen(s1);
	char *s = alloc(n + strlen(s2) + 1);

	strcpy(s, s1);
	strcpy(s + n, s2);
	return s;
}

/* compile - compile src into dst, return status */
static int compile(char *src, char *dst) {
	compose(com, clist, append(src, 0), append(dst, 0));
	return callsys(av);
}

/* compose - compose cmd into av substituting a, b, c for $1, $2, $3, resp. */
static void compose(char *cmd[], List a, List b, List c) {
	int i, j;
	List lists[3];

	lists[0] = a;
	lists[1] = b;
	lists[2] = c;
	/* j是av的迭代器; i是cmd的迭代器 */
	for (i = j = 0; cmd[i]; i++) {
		char *s = strchr(cmd[i], '$'); /* 在cmd的参数选项里找字符$ */
		if (s && isdigit(s[1])) { /* 如果是$后跟一个number字符 */
			int k = s[1] - '0'; /* 得到此number */
			assert(k >=1 && k <= 3); /* 确保number是{1,2,3}三者之一 */
			if (b = lists[k-1]) { /* 将链表头复制到临时变量b里 */
				b = b->link; /* 拿到此链表的第一个元素 */
				av[j] = alloc(strlen(cmd[i]) + strlen(b->str) - 1); /* strlen("abc/$1/def") + strlen(b->str) - 1 */
				strncpy(av[j], cmd[i], s - cmd[i]); /* 拷贝$1之前的字符,例如: abc/ */
				av[j][s-cmd[i]] = '\0'; /* 将它设置为字符串规范,即: abc/ 结尾要跟一个空字符 */
				strcat(av[j], b->str); /* 将$1代表的内容连接到abc/之后 */
				strcat(av[j++], s + 2); /* 将$1之后的字串/def连接到$1被展开的字串后面去 */
				while (b != lists[k-1]) { /* 如果还没有到链表结尾 */
					b = b->link; /* b指向链表中下一节点 */
					assert(j < ac);
					av[j++] = b->str; /* TODO */
				};
			}
		} else if (*cmd[i]) { /* 如果还不到参数vector的最后一个元素 */
			assert(j < ac); /* 确保要写av的下标j小于ac */
			av[j++] = cmd[i]; /* 将该参数字串拷贝至av中 */
		}
	}
	av[j] = NULL; /* 置argument vector空指针结尾标记 */
}

/* error - issue error msg according to fmt, bump error count */
static void error(char *fmt, char *msg) {
	fprintf(stderr, "%s: ", progname);
	fprintf(stderr, fmt, msg);
	fprintf(stderr, "\n");
	errcnt++;
}

/* exists - if `name' readable return its path name or return null */
static char *exists(char *name) {
	List b;

	if ( (name[0] == '/' || name[0] == '\\' || name[2] == ':')
	&& access(name, 4) == 0) /* 如果name是绝对路径，且name可读(4代表二进制100) */
		return name; /* 直接返回此name */
	if (!(name[0] == '/' || name[0] == '\\' || name[2] == ':')
	&& (b = lccinputs))	/* 如果name非绝对路径,且lccinputs列表非空 */
		do { /* 我们通过循环查找这个name文件是否能在lccinputs的目录列表下被找到 */
			b = b->link; /* 设置b指向lccinputs链表的表头 */
			if (b->str[0]) {
				char buf[1024];
				sprintf(buf, "%s/%s", b->str, name); /* 拼接成一个完整路径 */
				if (access(buf, 4) == 0) /* 如果name路径可被访问 */
					return strsave(buf); /* 则返回name的完整路径 */
			} else if (access(name, 4) == 0) /* 如果b->str[0] == 0 表示在当前目录下查找name */
				return name; /* 如果name可在当前目录下被找到,则直接返回name */
		} while (b != lccinputs);
	if (verbose > 1)
		return name;
	return 0;
}

/* first - return first component in semicolon separated list */
static char *first(char *list) {
	char *s = strchr(list, ';');

	if (s) {
		char buf[1024];
		strncpy(buf, list, s-list);
		buf[s-list] = '\0';
		return strsave(buf);
	} else
		return list;
}

/* filename - process file name argument `name', return status */
static int filename(char *name, char *base) {
	int status = 0;
	static char *stemp, *itemp;

	if (base == 0)
		base = basepath(name); /* 获得文件的basepath,例如: /usr/drh/foo.c => foo */
	switch (suffix(name, suffixes, 4)) { /* 如果文件属于{c,i,s,o} */
	case 0:	/* 如果是.c源文件 */
		compose(cpp, plist, append(name, 0), 0); /* 构造cpp的命令行 */
		if (Eflag) { /* 如果命令行上定义了-E选项(仅仅对文件做预处理) */
			status = callsys(av); /* 调用cpp命令 */
			break; /* 结束switch分支语句 */
		}
		if (itemp == NULL) /* 如果没有 .i 的临时源文件名 */
			itemp = tempname(first(suffixes[1])); /* 创建一个临时文件名 */
		compose(cpp, plist, append(name, 0), append(itemp, 0)); /* 构造调用cpp的命令行 */
		status = callsys(av); /* 调用cpp命令 */
		if (status == 0) /* 如果cpp命令成功返回 */
			return filename(itemp, base); /* 拿着cpp输出的.i文件递归调用filename函数进行下一步的处理工作 */
		break;
	case 1:	/* 如果是.i源文件 */
		if (Eflag)  /* 如果-E选项被指定了 */
			break; /* 直接退出switch语句 */
		if (Sflag)  /* 如果-S选项被指定了 */
			status = compile(name, outfile ? outfile : concat(base, first(suffixes[2]))); /* 仅做汇编,汇编完调用break退出switch */
		else if ((status = compile(name, stemp?stemp:(stemp=tempname(first(suffixes[2]))))) == 0) /* 将i源文件编译成汇编文件 */
			return filename(stemp, base); /* 拿着rcc输出的汇编源文件递归调用filename函数进行下一步的处理工作 */
		break;
	case 2:	/* 如果是汇编源文件 */
		if (Eflag) /* 如果-E选项被指定了 */
			break; /* 直接退出switch语句 */
		if (!Sflag) { /* 如果命令行上没有指定-S选项 */
			char *ofile;
			if (cflag && outfile) /* 如果命令行上指定了-c选项 且有 -o filename 选项 */
				ofile = outfile;
			else if (cflag) /* 如果命令行上仅有-c选项没有-o选项 */
				ofile = concat(base, first(suffixes[3]));
			else	/* 命令行上仅指定了-o filename选项(那么这个filename一定是可执行程序名)或命令上既没有指定-o选项也没有指定-c选项 */
				ofile = tempname(first(suffixes[3]));
			compose(as, alist, append(name, 0), append(ofile, 0)); /* 构造调用as的命令行 */
			status = callsys(av); /* 调用as命令 */
			if (!find(ofile, llist[1])) /* 如果在llist[1]中没有找到as输出的目标文件 */
				llist[1] = append(ofile, llist[1]); /* 则把as输出的目标文件添加到llist[1]中去 */
		}
		break;
	case 3:	/* 如果是目标文件 .o */
		if (!find(name, llist[1])) /* 如果目标文件不在llist[1]中 */
			llist[1] = append(name, llist[1]); /* 则把该目标文件添加进llist[1]列表中去 */
		break;
	default: /* 如果该文件不是.c,.i,.s,.o，那么有可能是库文件或者源代码文件 */
		if (Eflag) { /* 如果命令行上仅仅指定了预处理-E选项 */
			compose(cpp, plist, append(name, 0), 0); /* 构造cpp的一个命令行 */
			status = callsys(av); /* 调用cpp命令执行预处理 */
		}
		llist[1] = append(name, llist[1]); /* 将该文件附加到llist[1]列表中去 */
		break;
	}
	if (status) /* 如果调用系统调用失败 */
		errcnt++; /* 错误数++ */
	return status; /* 返回系统调用的返回值 */
}

/* find - find 1st occurrence of str in list, return list node or 0 */
static List find(char *str, List list) {
	List b;
	
	if (b = list)
		do {
			if (strcmp(str, b->str) == 0)
				return b;
		} while ((b = b->link) != list);
	return 0;
}

/* help - print help message */
static void help(void) {
	static char *msgs[] = {
"", " [ option | file ]...\n",
"	except for -l, options are processed left-to-right before files\n",
"	unrecognized options are taken to be linker options\n",
"-A	warn about nonANSI usage; 2nd -A warns more\n",
"-b	emit expression-level profiling code; see bprint(1)\n",
#ifdef sparc
"-Bstatic -Bdynamic	specify static or dynamic libraries\n",
#endif
"-Bdir/	use the compiler named `dir/rcc'\n",
"-c	compile only\n",
"-dn	set switch statement density to `n'\n",
"-Dname -Dname=def	define the preprocessor symbol `name'\n",
"-E	run only the preprocessor on the named C programs and unsuffixed files\n",
"-g	produce symbol table information for debuggers\n",
"-help or -?	print this message on standard error\n",
"-Idir	add `dir' to the beginning of the list of #include directories\n",	
"-lx	search library `x'\n",
"-M	emit makefile dependencies; implies -E\n",
"-N	do not search the standard directories for #include files\n",
"-n	emit code to check for dereferencing zero pointers\n",
"-O	is ignored\n",
"-o file	leave the output in `file'\n",
"-P	print ANSI-style declarations for globals on standard error\n",
"-p -pg	emit profiling code; see prof(1) and gprof(1)\n",
"-S	compile to assembly language\n",
"-static	specify static libraries (default is dynamic)\n",
"-dynamic	specify dynamically linked libraries\n",
"-t -tname	emit function tracing calls to printf or to `name'\n",
"-target name	is ignored\n",
"-tempdir=dir	place temporary files in `dir/'", "\n"
"-Uname	undefine the preprocessor symbol `name'\n",
"-v	show commands as they are executed; 2nd -v suppresses execution\n",
"-w	suppress warnings\n",
"-Woarg	specify system-specific `arg'\n",
"-W[pfal]arg	pass `arg' to the preprocessor, compiler, assembler, or linker\n",
	0 };
	int i;
	char *s;

	msgs[0] = progname;
	for (i = 0; msgs[i]; i++) {
		fprintf(stderr, "%s", msgs[i]);
		if (strncmp("-tempdir", msgs[i], 8) == 0 && tempdir)
			fprintf(stderr, "; default=%s", tempdir);
	}
#define xx(v) if (s = getenv(#v)) fprintf(stderr, #v "=%s\n", s)
	xx(LCCINPUTS);
	xx(LCCDIR);
#ifdef WIN32
	xx(include);
	xx(lib);
#endif
#undef xx
}

/*
 * initinputs - 初始化头文件搜索路径列表
 */
static void initinputs(void) {
	char *s = getenv("LCCINPUTS"); /* 取得环境变量LCCINPUTS的字串值 */
	List list, b;

	/* 如果LCCINPUTS全局变量没有被定义并且inputs数组为空 */
	if (s == 0 && (s = inputs)[0] == 0)
		s = "."; /* s中至少应包含当前目录 */
	if (s) {
		lccinputs = path2list(s); /* 将s中的分号或冒号分隔的路径列表字串转化成List保存在lccinputs中 */
		if (b = lccinputs) /* 如果链表非空 */
			do {
				b = b->link; /* 令b指向lccinputs链表中第一个元素 */
				if (strcmp(b->str, ".") != 0) {
					ilist = append(concat("-I", b->str), ilist); /* 将lccinputs中的目录列表，插入到ilist中 */
					if (strstr(com[1], "win32") == NULL) /* 如果不是win32系统,则添加目录列表到库链接查找链中 */
						llist[0] = append(concat("-L", b->str), llist[0]); /* 将LCCINPUTS中的目录列表也添加进库搜索路径列表中 */
				} else
					b->str = ""; /* 将当前目录的点字符置为空字符 */
			} while (b != lccinputs); /* 如果当前节点不是lccinputs中的最后一个节点,则继续循环 */
	}
#ifdef WIN32
	if (list = b = path2list(getenv("include")))
		do {
			int n;
			b = b->link;
			n = strlen(b->str);
			if (b->str[n-1] == '\\')
				b->str[n-1] = '/';
			ilist = append(stringf("-I\"%s\"", b->str), ilist);
		} while (b != list);
#endif
}

/* interrupt - catch interrupt signals */
static void interrupt(int n) {
	rm(rmlist);
	exit(n = 100);
}

/* opt - process option in arg */
static void opt(char *arg) { /* arg[0]是字符 '-' */
	switch (arg[1]) {	/* arg[1]是选项字符 */
	case 'W':	/* W选项可以将W字符后的选项传递给: cpp(-Wp),rcc(-Wf),as(-Wa),ld(-Wl),或者是系统专有选项 -Wo */
		if (arg[2] && arg[3])
			switch (arg[2]) {
			case 'o':	/* -Woarg 指定系统专有选项,例如: -Wo-lccdir=dir 指定lcc的安装目录 */
				if (option(&arg[3])) /* 覆盖系统默认配置的LCCDIR目录 */
					return;
				break;
			case 'p':	/* -Wpxxx,传递给预处理程序的选项flag,例如: -Wp-D_ABC_=1 定义_ABC_等于1 ,相当于直接在C语言中定义 #define _ABC_ 1 */
				plist = append(&arg[3], plist); /* 附加到预处理程序的选项列表中 */
				return;
			case 'f': /* -Wfxxx,传递给rcc的选项flag,例如: lcc -S -Wf-g3,# file.c将file.c编译成汇编代码,设定-g3调试级别,并指示汇编的注释字符# ，
						将对应的C代码以注释的形式内嵌到生成的汇编代码中*/
				if (strcmp(&arg[3], "-C") == 0 && !option("-b")) { /* 如果 -Wf-C 这个选项不被支持 */
					break;	/* -C requires that -b is supported */
				}
				clist = append(&arg[3], clist); /* 将选项附加到编译器的选项列表中 */
				if (strcmp(&arg[3], "-unsigned_char=1") == 0) { /* makes plain char an unsigned (1) or signed (0) type;
																	by default, char is signed. */
					/* __CHAR_UNSIGNED__: GCC defines this macro if and only if the data type char is unsigned on the target machine.
 	 	 	 	 	 It exists to cause the standard header file limits.h to work correctly. You should not use this macro yourself;
 	 	 	 	 	  instead, refer to the standard macros defined in limits.h.  */
					plist = append("-D__CHAR_UNSIGNED__", plist); /* TODO: 貌似为了和gcc的cpp保持一致 */
					plist = append("-U_CHAR_IS_SIGNED", plist); /* 看LOG文件的273行的说明... */
				}
#define xx(name,k) \
				if (strcmp(&arg[3], "-wchar_t=" #name) == 0) \
					plist = append("-D_WCHAR_T_SIZE=" #k, plist);
xx(unsigned_char,1)
xx(unsigned_short,2)
xx(unsigned_int,4)
#undef xx
				return;
			case 'a': /* -Waxxx,传递给汇编器as的选项 */
				alist = append(&arg[3], alist); /* 将选项附加到汇编选项列表中 */
				return;
			case 'l': /* -Wlxxx,传递给链接器ld的选项 */
				llist[0] = append(&arg[3], llist[0]); /* 将选项附加到ld的选项列表中 */
				return;
			}
		fprintf(stderr, "%s: %s ignored\n", progname, arg); /* 打印警告信息：未识别的选项 */
		return;
	case 'd':	/* -dn: 产生一个switches的跳转表,它的密度至少是n(n是介于0到1之间的浮点常量). n的默认值是0.5 */
		if (strcmp(arg, "-dynamic") == 0) { /* linux系统不支持-dynamic选项 */
			if (!option(arg)) /* 如果ld不支持-dynamic选项 */
				fprintf(stderr, "%s: %s ignored\n", progname, arg); /* 打印警告信息: 忽略-dynamic选项 */
		} else {
			arg[1] = 's'; /* 将d改为s */
			clist = append(arg, clist); /* 将-s选项附加到rcc的选项列表中 */
		}
		return;
	case 't':	/* -t -tname -tempdir=dir */
		if (strncmp(arg, "-tempdir=", 9) == 0) /* 如果是指定编译临时目录选项 */
			tempdir = arg + 9; /* 将临时目录保存到tempdir中 */
		else	/* 如果是-t或-tname选项 */
			clist = append(arg, clist); /* 则将这个选项附加到rcc的选项列表中 */
		return;
	case 'p':	/* -p -pg ,程序剖析选项, TODO: 在我的ubuntu12-i386机器上加此选项会提示"/usr/bin/ld: cannot find -lgmon" */
		if (option(arg))	/* 如果ld支持此选项 */
			clist = append(arg, clist); /* 则将此选项附加到rcc的选项列表中 */
		else
			fprintf(stderr, "%s: %s ignored\n", progname, arg); /* 若不支持,则打印忽略此选项的提示 */
		return;
	case 'D':	/* -Dname -Dname=def 宏定义选项 */
	case 'U':	/* -Uname 取消宏定义选项 */
	case 'I':	/* -Idir 添加文件包含目录 */
		plist = append(arg, plist); /* 将这些选项附加到cpp的选项列表中去 */
		return;
	case 'B':	/* -Bdir -Bstatic -Bdynamic
					-Bstr Use the compiler strrcc instead of the default version. Note that str often requires a trailing slash.
					(On Sparcs only, -Bstatic and -Bdynamic are passed to the loader) */
#ifdef sparc
		if (strcmp(arg, "-Bstatic") == 0 || strcmp(arg, "-Bdynamic") == 0)
			llist[1] = append(arg, llist[1]);
		else
#endif	
		{
		static char *path;
		if (path)
			error("-B overwrites earlier option", 0); /* 如果命令行上有两个-B选项,那么第二个会覆盖第一个 */
		path = arg + 2;
		if (strstr(com[1], "win32") != NULL) /* 如果是win32系统 */
			com[0] = concat(replace(path, '/', '\\'), concat("rcc", first(suffixes[4])));
		else /* 非win32系统 */
			com[0] = concat(path, "rcc"); /* 使用path/rcc路径的编译器来编译源程序 */
		if (path[0] == 0) /* 如果路径非法 */
			error("missing directory in -B option", 0); /* 则打印警告信息 */
		}
		return;
	case 'h':	/* 打印lcc的usage信息 */
		if (strcmp(arg, "-help") == 0) {
			static int printed = 0;
	case '?': /* 打印lcc的usage信息 */
			if (!printed)
				help();
			printed = 1;
			return;
		}
		break;
	case 's':
		if (strcmp(arg, "-static") == 0) {
			if (!option(arg)) /* 如果ld不支持此选项 */
				fprintf(stderr, "%s: %s ignored\n", progname, arg); /* 打印警告信息：该选项被忽略 */
			return;
		}
		break;
	}
	if (arg[2] == 0)
		switch (arg[1]) {	/* 单字符的选项 */
		case 'S':	/* 汇编选项 */
			Sflag++;
			return;
		case 'O': /* lcc不支持优化选项 */
			fprintf(stderr, "%s: %s ignored\n", progname, arg); /* 打印警告信息: lcc不支持优化选项 */
			return;
		case 'A': case 'n': case 'w': case 'P': /* TODO: 这4个选项分别是干什么的? */
			clist = append(arg, clist);
			return;
		case 'g': case 'b': /* -g在可执行文件中产生符号信息供gdb使用.   TODO: -b选项供程序剖析使用.... */
			if (option(arg)) /* 如果-g或-b选项被支持  */
				clist = append(arg[1] == 'g' ? "-g2" : arg, clist); /* 将其添加进rcc的命令行选项中去 */
			else /* 如果-g,-b选项不被支持 */
				fprintf(stderr, "%s: %s ignored\n", progname, arg); /* 则打印警告信息 */
			return;
		case 'G': /* -g3的调试级别. */
			if (option(arg)) { /* 如果-G选项被支持. */
				clist = append("-g3", clist); /* 将-g3选项添加进rcc的命令行选项列表中去 */
				llist[0] = append("-N", llist[0]); /* TODO: man ld 查看此选项的作用 */
			} else
				fprintf(stderr, "%s: %s ignored\n", progname, arg); /* 如果-G选项不被支持,则打印警告信息 */
			return;
		case 'E': /* 告知lcc仅对源程序做预处理的选项 */
			Eflag++;
			return;
		case 'c': /* 告知lcc仅对源程序编译生成目标文件的选项 */
			cflag++;
			return;
		case 'M': /* 为make程序产生文件依赖规则 */
			Eflag++;	/* -M implies -E */
			plist = append(arg, plist); /* 产生依赖规则 */
			return;
		case 'N': /* -N Do not search any of the standard directories for `#include' files.
		Only those directories specified by subsequent explicit -I options will be searched, in the order given. */
			if (strcmp(basepath(cpp[0]), "cpp") == 0) /* TODO: 对于ubuntu系统来说这里cpp0应该改为cpp */
				plist = append("-nostdinc", plist);
			include[0] = 0;
			ilist = 0;
			return;
		case 'v': /* verbose */
			if (verbose++ == 0) {
				if (strcmp(basepath(cpp[0]), "cpp") == 0) /* TODO: 对于ubuntu系统来说这里cpp0应该改为cpp */
					plist = append(arg, plist);
				clist = append(arg, clist);
				fprintf(stderr, "%s: %s\n",progname, version);
			}
			return;
		}
	if (cflag || Sflag || Eflag) /* 如果 -c , -S , -E 之中任何一个选项被指定了,那么忽略-o选项 */
		fprintf(stderr, "%s: %s ignored\n", progname, arg);
	else { /* 注意: 此处实际处理的是形如 -ofilename 或者 -lpthread 的选项(换句话说,该处只处理选项和选项参数之间没有空格的情况) */
		llist[1] = append(arg, llist[1]); /* 附加库文件(例如: -lpcre )或输出文件(例如: -oxxx)的选项到llist[1]中去 */
		// fprintf(stderr,"arg=%s\n",arg); 这里可以处理类似: lcc -otest test.c -lpthread 中的 -otest 选项
	}
}

/* path2list - convert a colon- or semicolon-separated list to a list */
static List path2list(const char *path) {
	List list = NULL;
	char sep = ':'; /* 默认分隔符是冒号 */

	if (path == NULL)
		return NULL;
	if (strchr(path, ';')) /* 确定路径分隔符到底是分号还是冒号 */
		sep = ';';
	while (*path) {
		char *p, buf[512];
		if (p = strchr(path, sep)) { /* 如果找到了第一个分隔符 */
			assert(p - path < sizeof buf); /* path必须小于512个字符 */
			strncpy(buf, path, p - path); /* 拷贝第一个路径到buf中 */
			buf[p-path] = '\0'; /* 将分隔符替换为空字符 */
		} else { /* 如果没找到第一个分隔符(说明是路径列表字串中的最后一个路径了) */
			assert(strlen(path) < sizeof buf); /* 确保该路径的字符数小于buf的长度 */
			strcpy(buf, path); /* 拷贝之 */
		}
		if (!find(buf, list)) /* 如果在list中没有找到当前的path */
			list = append(strsave(buf), list); /* 则将此path附加到list中去 */
		if (p == 0) /* 如果是最后一个路径(说明已经找出了所有的路径) */
			break; /* 退出循环 */
		path = p + 1; /* path指向分隔符的下一个字符(指望下次循环从此处开始搜索) */
	}
	return list; /* 返回此目录节点构成的 */
}

/* replace - copy str, then replace occurrences of from with to, return the copy */
char *replace(const char *str, int from, int to) {
	char *s = strsave(str), *p = s;

	for ( ; (p = strchr(p, from)) != NULL; p++)
		*p = to;
	return s;
}

/* rm - remove files in list */
static void rm(List list) {
	if (list) {
		List b = list;
		if (verbose)
			fprintf(stderr, "rm");
		do {
			if (verbose)
				fprintf(stderr, " %s", b->str);
			if (verbose < 2)
				remove(b->str);
		} while ((b = b->link) != list);
		if (verbose)
			fprintf(stderr, "\n");
	}
}

/* strsave - return a saved copy of string str */
char *strsave(const char *str) {
	return strcpy(alloc(strlen(str)+1), str);
}

/* stringf - format and return a string */
char *stringf(const char *fmt, ...) {
	char buf[1024];
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = vsprintf(buf, fmt, ap);
	va_end(ap);
	return strsave(buf);
}

/* suffix - if one of tails[0..n-1] holds a proper suffix of name, return its index */
int suffix(char *name, char *tails[], int n) {
	int i, len = strlen(name);

	for (i = 0; i < n; i++) {
		char *s = tails[i], *t;
		/* 在win32平台上是分号分隔的扩展名列表组成的列表,详见etc/win32.c中的suffixes数组*/
		for ( ; t = strchr(s, ';'); s = t + 1) {
			int m = t - s;
			if (len > m && strncmp(&name[len-m], s, m) == 0)
				return i;
		}
		if (*s) {
			int m = strlen(s);
			if (len > m && strncmp(&name[len-m], s, m) == 0)
				return i;
		}
	}
	return -1;
}

/* tempname - generate a temporary file name in tempdir with given suffix */
char *tempname(char *suffix) {
	static int n;
	char *name = stringf("%s/lcc%d%d%s", tempdir, getpid(), n++, suffix); /* 构造一个扩展名为i的临时源文件字串 */

	if (strstr(com[1], "win32") != NULL) /* 如果是win32系统 */
		name = replace(name, '/', '\\'); /* 将unix的路径分隔符替换为windows风格 */
	rmlist = append(name, rmlist); /* 将该文件名附加到将要被删除的文件列表中 */
	return name; /* 返回该文件名 */
}
