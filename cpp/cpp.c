#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <assert.h>
#include "cpp.h"

#define		OUTS	16384		/* 输出缓冲区的大小 */
char		outbuf[OUTS];		/* 输出缓冲区 */
char		*outp = outbuf;		/* 输出缓冲区的当前指针 */
Source		*cursource;			/* 当前输入源(指向的输入源栈的栈顶元素) */
int			nerrs;				/* 记录预处理中错误数 */

struct	token nltoken = { NL, 0, 0, 0, 1, (uchar*)"\n" }; /* 换行符的token结构 */
char	*curtime;			/* 当前时间的字串 */
int		incdepth;			/* 头文件包含的深度,默认0 */
int		ifdepth;			/* 当前条件编译语句的嵌套深度 */
int		ifsatisfied[NIF];	/* 条件编译深度数组，例如: ifsatisfied[4],表明深度为4的#if被满足了... */
int		skipping;			/* TODO: 略过谁？*/

char rcsid[] = ": version 4.2";	/* 版本字串 */

int main(int argc, char **argv) {
	Tokenrow tr;			/* token row */
	time_t t;				/* 保存当前时间的整数值 */
	char ebuf[BUFSIZ];		/* stderr buffer（stderr默认是没有buffer的，我们为其建立一个buffer） */

	setbuf(stderr, ebuf);	/* 设定标准错误的输出缓冲区为ebuf */
	t = time(NULL);			/* 获得当前时间的整数值 */
	curtime = ctime(&t);	/* 获得当前时间的字串值(例如: Sat Jul  4 12:27:13 2003)保存到全局变量curtime中 */
	maketokenrow(3, &tr);	/* 建立一个 token row（默认分配3个Token结构） */
	expandlex();			/* 展开状态机 */
	setup(argc, argv);		/* 建立关键字hash表;处理命令行选项参数;将输入源文件压栈 */
	fixlex();				/* 适时的关闭兼容C++单行注释兼容特性 */
	iniths();				/* 初始化hideset，TODO: 啥是hideset？ */
	genline();				/* 产生一个行控制(line control)信息 */
	process(&tr);			/* 开始处理源程序 */
	flushout();				/* flush output buffer to stdout */
	fflush(stderr);			/* flush stderr buffer */
	exit(nerrs > 0);		/* 退出进程 */
	return 0;
}

/**
 * process - 处理c源程序的总函数
 * @trp: 用来存储c源程序中的一行Token
 */
void process(Tokenrow *trp) {
	int anymacros = 0;

	for (;;) {
		if (trp->tp >= trp->lp) { /* 如果当前token row中没有有效数据 */
			trp->tp = trp->lp = trp->bp; /* 重置token row的当前token指针 */
			outp = outbuf; /* 重置输出缓冲区的当前指针 */
			anymacros |= gettokens(trp, 1); /* 得到一行Token */
			trp->tp = trp->bp;
		}
		if (trp->tp->type == END) { /* 如果遇到EOFC结束符 */
			if (--incdepth >= 0) {
				if (cursource->ifdepth)
					error(ERROR,"Unterminated conditional in #include");
				unsetsource();
				cursource->line += cursource->lineinc;
				trp->tp = trp->lp;
				genline();
				continue;
			}
			if (ifdepth)
				error(ERROR, "Unterminated #if/#ifdef/#ifndef");
			break;
		}
		if (trp->tp->type==SHARP) { /* 如果当前Token是'#'字符 */
			trp->tp += 1; /* tp移动到token row中的下一个token */
			control(trp); /* 处理预处理指令部分（宏定义，条件编译，头文件包含） */
		} else if (!skipping && anymacros)
			expandrow(trp, NULL);
		if (skipping) /* 如果当前是忽略状态 */
			setempty(trp); /* 置空当前行 */
		puttokens(trp); /* 输出当前行 */
		anymacros = 0;
		cursource->line += cursource->lineinc;
		if (cursource->lineinc>1) {
			genline();
		}
	}
}

/**
 * control - 处理预处理控制指令（头文件包含指令，行控制指令，条件编译指令）
 * @trp: 一行源程序的Token
 * 返回值：无
 */
void control(Tokenrow *trp) {
	Nlist *np;
	Token *tp;

	tp = trp->tp; /* 获得Tokenrow中的当前Token(指向预处理控制指令关键字) */
	if (tp->type!=NAME) { /* 如果当前Token不是标识符 */
		if (tp->type==NUMBER) /* 如果当前Token是数字 */
			goto kline; /* 跳转，处理行控制指令 */
		if (tp->type != NL) /* 如果该Token既不是标识符，也不是数字 */
			error(ERROR, "Unidentifiable control line"); /* 则，打印信息`无法识别的控制指令行' */
		return; /* 该行处理完毕，函数返回（该行为空行，无控制信息，else empty line) */
	}
	/* 如果当前Token是标识符但标识符不在标识符hash表中 或 标识符在hash表中但不是关键字且不能略过（TODO：啥是略过？） */
	if ((np = lookup(tp, 0))==NULL || (np->flag&ISKW)==0 && !skipping) {
		error(WARNING, "Unknown preprocessor control %t", tp); /* 打印信息，无法识别的预处理控制指令 */
		return; /* 该行处理完毕 */
	}
	if (skipping) { /* TODO: 啥是skipping? */
		if ((np->flag&ISKW)==0) /* 如果np不是关键字 */
			return; /* 函数返回 */
		switch (np->val) {
		case KENDIF:
			if (--ifdepth<skipping)
				skipping = 0;
			--cursource->ifdepth;
			setempty(trp);
			return;

		case KIFDEF:
		case KIFNDEF:
		case KIF:
			if (++ifdepth >= NIF)
				error(FATAL, "#if too deeply nested");
			++cursource->ifdepth;
			return;

		case KELIF:
		case KELSE:
			if (ifdepth<=skipping)
				break;
			return;

		default:
			return;
		}
	}
	switch (np->val) {
	case KDEFINE: /* #define，定义宏 */
		dodefine(trp); /* 定义宏 */
		break;

	case KUNDEF: /* #undef */
		tp += 1; /* tp指向宏名 */
		if (tp->type!=NAME || trp->lp - trp->bp != 4) { /* 如果tp不是标识符类型 或者 lp-bp!=4 (lp和bp之间有4个Token) */
			error(ERROR, "Syntax error in #undef"); /* 打印错误信息 */
			break;
		}
		if ((np = lookup(tp, 0)) != NULL) /* 如果在hash表中找到了tp所指向的宏名 */
			np->flag &= ~ISDEFINED; /* 清零ISDEFINED标志位 */
		break;

	case KPRAGMA: /* #pragma */
		return;

	case KIFDEF: /* #ifdef */
	case KIFNDEF: /* #ifndef */
	case KIF: /* #if */
		if (++ifdepth >= NIF) /* 全局条件编译语句的嵌套深度值加1 */
			error(FATAL, "#if too deeply nested"); /* 如果嵌套深度值大于NIF，则打印错误信息 `#if中嵌入太深' */
		++cursource->ifdepth; /* 当前输入源的条件编译语句的嵌套深度值加1 */
		ifsatisfied[ifdepth] = 0; /* 设定条件编译语句的嵌套深度值对应的if语句还未被满足 */
		if (eval(trp, np->val))
			ifsatisfied[ifdepth] = 1;
		else
			skipping = ifdepth;
		break;

	case KELIF: /* #elif */
		if (ifdepth==0) {
			error(ERROR, "#elif with no #if");
			return;
		}
		if (ifsatisfied[ifdepth]==2)
			error(ERROR, "#elif after #else");
		if (eval(trp, np->val)) {
			if (ifsatisfied[ifdepth])
				skipping = ifdepth;
			else {
				skipping = 0;
				ifsatisfied[ifdepth] = 1;
			}
		} else
			skipping = ifdepth;
		break;

	case KELSE: /* #else */
		if (ifdepth==0 || cursource->ifdepth==0) {
			error(ERROR, "#else with no #if");
			return;
		}
		if (ifsatisfied[ifdepth]==2)
			error(ERROR, "#else after #else");
		if (trp->lp - trp->bp != 3)
			error(ERROR, "Syntax error in #else");
		skipping = ifsatisfied[ifdepth]? ifdepth: 0;
		ifsatisfied[ifdepth] = 2;
		break;

	case KENDIF: /* #endif */
		if (ifdepth==0 || cursource->ifdepth==0) {
			error(ERROR, "#endif with no #if");
			return;
		}
		--ifdepth;
		--cursource->ifdepth;
		if (trp->lp - trp->bp != 3)
			error(WARNING, "Syntax error in #endif");
		break;

	case KERROR: /* #error */
		trp->tp = tp+1;
		error(WARNING, "#error directive: %r", trp);
		break;

	case KLINE: /* #line */
		trp->tp = tp+1;
		expandrow(trp, "<line>");
		tp = trp->bp+2;
	kline: /* 行控制信息处理（line control）*/
		if (tp+1>=trp->lp || tp->type!=NUMBER || tp+3<trp->lp
		 || (tp+3==trp->lp && ((tp+1)->type!=STRING)||*(tp+1)->t=='L')) { /* 如果行控制语法有误 */
			/* 上面的if判断中共有5种语法错误检查:
			 * 1. 如果只有`# 123\n'
			 * 2. 如果当前token不是数字类型
			 * 3. 如果有类似`# 123 "file.c" 2 3'的行，那么这种行不被lcc的预处理程序支持。TODO:lcc不支持行控制中的flag语法.
			 * 4. 如果在`# linenum filename'中，filename不是字符串
			 * 5. 如果filename是宽字符字符串
			 */
			error(ERROR, "Syntax error in #line"); /* 打印错误信息 */
			return; /* 该函数返回 */
		}
		cursource->line = atol((char*)tp->t)-1; /* 更新当前输入源的行号信息 */
		if (cursource->line<0 || cursource->line>=32768) /* 如果转化后的行号小于0或者行号大于32768 */
			error(WARNING, "#line specifies number out of range"); /* 打印错误信息 */
		tp = tp+1; /* 指针移动到filename位置 */
		if (tp+1<trp->lp) /* 如果filename存在（因为filename后通常紧跟一个换行符token） */
			cursource->filename=(char*)newstring(tp->t+1,tp->len-2,0); /* 保存输入源的文件名 */
		return; /* 该行处理完毕，函数返回 */

	case KDEFINED: /* #defined */
		error(ERROR, "Bad syntax for control line"); /* 打印语法错误提示 */
		break;

	case KINCLUDE: /* #include */
		doinclude(trp);
		trp->lp = trp->bp;
		return;

	case KEVAL: /* #eval */
		eval(trp, np->val);
		break;

	default: /* # other */
		error(ERROR, "Preprocessor control `%t' not yet implemented", tp); /* 未实现的预处理控制指令 */
		break;
	}
	setempty(trp); /* 置空Tokenrow */
	return;
}

void * domalloc(int size) {
	void *p = malloc(size);

	if (p==NULL)
		error(FATAL, "Out of memory from malloc");
	return p;
}

void dofree(void *p) {
	free(p);
}

void error(enum errtype type, char *string, ...) {
	va_list ap;
	char *cp, *ep;
	Token *tp;
	Tokenrow *trp;
	Source *s;
	int i;

	fprintf(stderr, "cpp: ");
	for (s=cursource; s; s=s->next)
		if (*s->filename)
			fprintf(stderr, "%s:%d ", s->filename, s->line);
	va_start(ap, string);
	for (ep=string; *ep; ep++) {
		if (*ep=='%') {
			switch (*++ep) {

			case 's':
				cp = va_arg(ap, char *);
				fprintf(stderr, "%s", cp);
				break;
			case 'd':
				i = va_arg(ap, int);
				fprintf(stderr, "%d", i);
				break;
			case 't':
				tp = va_arg(ap, Token *);
				fprintf(stderr, "%.*s", tp->len, tp->t);
				break;

			case 'r':
				trp = va_arg(ap, Tokenrow *);
				for (tp=trp->tp; tp<trp->lp&&tp->type!=NL; tp++) {
					if (tp>trp->tp && tp->wslen)
						fputc(' ', stderr);
					fprintf(stderr, "%.*s", tp->len, tp->t);
				}
				break;

			default:
				fputc(*ep, stderr);
				break;
			}
		} else
			fputc(*ep, stderr);
	}
	va_end(ap);
	fputc('\n', stderr);
	if (type==FATAL)
		exit(1);
	if (type!=WARNING)
		nerrs = 1;
	fflush(stderr);
}
