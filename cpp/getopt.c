//#include	<stdio.h>
//#include	<string.h>
//
//#define EPR                 fprintf(stderr,
//#define ERR(str, chr)       if(opterr){EPR "%s%c\n", str, chr);}
//
//int     opterr_x = 1;			/* option error，配合上面的ERR宏 */
//int     optind_x = 1;			/* The variable optind is the index of the next element to be processed in argv. */
//int		optopt_x;		/* getopt()函数返回时其中保存了选项flag字符，例如：如果选项为-U,那么optopt就保存字符'U' */
//char    *optarg_x;	/* getopt()函数返回时其中保存了该option后面的argument字符串 */
///**
// * getopt - 得到选项(详情请参考 man 3 getopt)
// * @argc: 命令行上的选项个数
// * @argv: 选项列表
// * @opts: 所支持的选项集合（opts is a string containing the legitimate option characters.
// * If such a character is followed by a colon(冒号), the option requires an argument.)
// * 返回值：-1表示选项结束，'?'表示非法选项; 若成功则返回 option flag character
// * NOTE: 这里要说一下选项（option）和参数（argument）的区别。当然我们是从shell命令行的角度来谈的。
// * option具体就是指类似于(-I,-D,-U,-N)这样的option flag字符.当然这些选项后面还可以跟argument，例如:
// * -I选项后面可以跟一个参数: /usr/include 。通常来说，参数不是必须的，例如这里的-N选项就不必后跟参数.
// * 通常我们把选项和参数合称为 option characters（注意：不包括前面的横杠dash字符）
// * TODO: 该函数的行为和系统函数getopt函数的行为不一样……需要修复bug么？它如果被修复了，会影响源码的其余部分么？
// * so...暂时屏蔽该函数...使用系统自带的函数
// 经验证，使用系统提供的getopt函数不会影响源码的其余部分，因此下一版中getopt将使用早期gcc源码中提供的文件……
// */
//int getopt_x (int argc, char *const argv[], const char *opts)
//{
//	static int sp = 1; /* sp初始设置为option characters中的 option flag字符的index(即1) */
//	int c; /* 保存选项flag字符，例如: 如果选项为-U,那么c就保存字符'U' */
//	char *cp;
//
//	if (sp == 1)
//		if (optind >= argc ||
//		   argv[optind][0] != '-' || argv[optind][1] == '\0')
//			return -1; /* 如果遇到当前argv[optind]的第一个字符不是横杠 '-' 或者当前argv[optind]只有一个字符，那么直接返回-1 */
//		else if (strcmp(argv[optind], "--") == 0) { /* 如果遇到选项结束标记 */
//			optind++; /* 选项index自加1 */
//			return -1; /* 返回-1 */
//		}
//	optopt = c = argv[optind][sp]; /* 保存option flag字符 */
//	if (c == ':' || (cp=strchr(opts, c)) == 0) { /* 如果没找到选项字符或者选项字符为':' */
//		ERR (": illegal option -- ", c); /* 打印警告，提示非法选项 */
//		if (argv[optind][++sp] == '\0') { /* 如果该option没有参数 */
//			optind++; /* 选项index自加1 */
//			sp = 1;  /* 重置sp为1 */
//		}
//		return '?'; /* 返回'?'字符。该option有参数和没参数时都返回问号，所不同的是如果该option没有参数的话会自加optind */
//	}
//	if (*++cp == ':') { /* 如果该option需要有argument */
//		if (argv[optind][sp+1] != '\0') /* 如果是形如: -I/usr/include 这样的选项,即: option和argument之间没有空格 */
//			optarg = &argv[optind++][sp+1]; /* 那么保存argument字符串 */
//		else if (++optind >= argc) { /* 如果此选项需要参数，但只给出了选项flag字符，后面却没有argument */
//			ERR (": option requires an argument -- ", c); /* 那么打印警告信息 */
//			sp = 1; /* 重置sp为1 */
//			return '?'; /* 返回'?'字符 */
//		} else /* 如果此选项需要参数,而且命令行上也给出了argument */
//			optarg = argv[optind++]; /* 那么保存此argument */
//		sp = 1; /* 重置sp为1 */
//	} else { /* 如果该option不需要有argument */
//		if (argv[optind][++sp] == '\0') { /* 而且命令行上给出的选项也正好是两个字符: dash character + option flag */
//			sp = 1; /* 重置sp为1 */
//			optind++; /* 切换到命令行上argv的下一元素 */
//		}
//		optarg = 0; /* 该option没有argument */
//	}
//	return c; /* 返回在命令行上找到的 option flag character */
//}
