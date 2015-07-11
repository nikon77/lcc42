#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <assert.h>
#include "cpp.h"

#define		OUTS	16384		/* 输出缓冲区的大小 */
char		outbuf[OUTS];		/* 输出缓冲区 */
char		*outp = outbuf;		/* 输出缓冲区的指针 */
Source		*cursource;			/* 当前的输入源 */
int			nerrs;				/* 记录预处理中错误数 */

struct	token nltoken = { NL, 0, 0, 0, 1, (uchar*)"\n" }; /* 换行符的token结构 */
char	*curtime;			/* 当前时间的字串 */
int		incdepth;			/* 头文件包含的深度 */
int		ifdepth;			/* 条件编译语句的内嵌深度 */
int		ifsatisfied[NIF];	/* 条件编译深度数组，例如: ifsatisfied[4],表明深度为4的#if被满足了... */
int		skipping;			/* TODO: 略过谁？*/

char rcsid[] = ": version 4.2";	/* 版本字串 */

int main(int argc, char **argv)
{
	Tokenrow tr;			/* token row */
	time_t t;				/* 保存当前时间的整数值 */
	char ebuf[BUFSIZ];		/* stderr buffer */

	setbuf(stderr, ebuf);	/* 设定标准错误的输出缓冲区为ebuf */
	t = time(NULL);			/* 获得当前时间的整数值 */
	curtime = ctime(&t);	/* 获得当前时间的字串值(例如: Sat Jul  4 12:27:13 2014)保存到全局变量curtime中 */
	maketokenrow(3, &tr);	/* 建立一个 token row */
	expandlex();			/* 展开状态机 */
	setup(argc, argv);		/* 建立关键字hash表;处理命令行选项、参数;输出行控制信息 */
	fixlex();				/* 使lex兼容C++的单行注释 */
	iniths();				/* 初始化hideset */
	genline();				/* 产生一个line control信息 */
	process(&tr);
	flushout();				/* flush output buffer to stdout */
	fflush(stderr);			/* flush stderr */
	exit(nerrs > 0);		/* 退出进程 */
	return 0;
}

void
process(Tokenrow *trp)
{
	int anymacros = 0;

	for (;;) {
		if (trp->tp >= trp->lp) {
			trp->tp = trp->lp = trp->bp;
			outp = outbuf;
			anymacros |= gettokens(trp, 1);
			trp->tp = trp->bp;
		}
		if (trp->tp->type == END) {
			if (--incdepth>=0) {
				if (cursource->ifdepth)
					error(ERROR,
					 "Unterminated conditional in #include");
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
		if (trp->tp->type==SHARP) {
			trp->tp += 1;
			control(trp);
		} else if (!skipping && anymacros)
			expandrow(trp, NULL);
		if (skipping)
			setempty(trp);
		puttokens(trp);
		anymacros = 0;
		cursource->line += cursource->lineinc;
		if (cursource->lineinc>1) {
			genline();
		}
	}
}
	
void
control(Tokenrow *trp)
{
	Nlist *np;
	Token *tp;

	tp = trp->tp;
	if (tp->type!=NAME) {
		if (tp->type==NUMBER)
			goto kline;
		if (tp->type != NL)
			error(ERROR, "Unidentifiable control line");
		return;			/* else empty line */
	}
	if ((np = lookup(tp, 0))==NULL || (np->flag&ISKW)==0 && !skipping) {
		error(WARNING, "Unknown preprocessor control %t", tp);
		return;
	}
	if (skipping) {
		if ((np->flag&ISKW)==0)
			return;
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
	case KDEFINE:
		dodefine(trp);
		break;

	case KUNDEF:
		tp += 1;
		if (tp->type!=NAME || trp->lp - trp->bp != 4) {
			error(ERROR, "Syntax error in #undef");
			break;
		}
		if ((np = lookup(tp, 0)) != NULL)
			np->flag &= ~ISDEFINED;
		break;

	case KPRAGMA:
		return;

	case KIFDEF:
	case KIFNDEF:
	case KIF:
		if (++ifdepth >= NIF)
			error(FATAL, "#if too deeply nested");
		++cursource->ifdepth;
		ifsatisfied[ifdepth] = 0;
		if (eval(trp, np->val))
			ifsatisfied[ifdepth] = 1;
		else
			skipping = ifdepth;
		break;

	case KELIF:
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

	case KELSE:
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

	case KENDIF:
		if (ifdepth==0 || cursource->ifdepth==0) {
			error(ERROR, "#endif with no #if");
			return;
		}
		--ifdepth;
		--cursource->ifdepth;
		if (trp->lp - trp->bp != 3)
			error(WARNING, "Syntax error in #endif");
		break;

	case KERROR:
		trp->tp = tp+1;
		error(WARNING, "#error directive: %r", trp);
		break;

	case KLINE:
		trp->tp = tp+1;
		expandrow(trp, "<line>");
		tp = trp->bp+2;
	kline:
		if (tp+1>=trp->lp || tp->type!=NUMBER || tp+3<trp->lp
		 || (tp+3==trp->lp && ((tp+1)->type!=STRING)||*(tp+1)->t=='L')){
			error(ERROR, "Syntax error in #line");
			return;
		}
		cursource->line = atol((char*)tp->t)-1;
		if (cursource->line<0 || cursource->line>=32768)
			error(WARNING, "#line specifies number out of range");
		tp = tp+1;
		if (tp+1<trp->lp)
			cursource->filename=(char*)newstring(tp->t+1,tp->len-2,0);
		return;

	case KDEFINED:
		error(ERROR, "Bad syntax for control line");
		break;

	case KINCLUDE:
		doinclude(trp);
		trp->lp = trp->bp;
		return;

	case KEVAL:
		eval(trp, np->val);
		break;

	default:
		error(ERROR, "Preprocessor control `%t' not yet implemented", tp);
		break;
	}
	setempty(trp);
	return;
}

void * domalloc(int size)
{
	void *p = malloc(size);

	if (p==NULL)
		error(FATAL, "Out of memory from malloc");
	return p;
}

void
dofree(void *p)
{
	free(p);
}

void error(enum errtype type, char *string, ...)
{
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
