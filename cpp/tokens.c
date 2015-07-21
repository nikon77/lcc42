#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cpp.h"

static char wbuf[2*OBS]; /* 输出Token字串的缓冲区 */
static char *wbp = wbuf; /* 输出Token字串的缓冲区指针 */

/*
 * 1 for tokens that don't need whitespace when they get inserted
 * by macro expansion
 */
static const char wstab[] = {
	0,	/* END */
	0,	/* UNCLASS */
	0,	/* NAME */
	0,	/* NUMBER */
	0,	/* STRING */
	0,	/* CCON */
	1,	/* NL */
	0,	/* WS */
	0,	/* DSHARP */
	0,	/* EQ */
	0,	/* NEQ */
	0,	/* LEQ */
	0,	/* GEQ */
	0,	/* LSH */
	0,	/* RSH */
	0,	/* LAND */
	0,	/* LOR */
	0,	/* PPLUS */
	0,	/* MMINUS */
	0,	/* ARROW */
	1,	/* SBRA */
	1,	/* SKET */
	1,	/* LP */
	1,	/* RP */
	0,	/* DOT */
	0,	/* AND */
	0,	/* STAR */
	0,	/* PLUS */
	0,	/* MINUS */
	0,	/* TILDE */
	0,	/* NOT */
	0,	/* SLASH */
	0,	/* PCT */
	0,	/* LT */
	0,	/* GT */
	0,	/* CIRC */
	0,	/* OR */
	0,	/* QUEST */
	0,	/* COLON */
	0,	/* ASGN */
	1,	/* COMMA */
	0,	/* SHARP */
	1,	/* SEMIC */
	1,	/* CBRA */
	1,	/* CKET */
	0,	/* ASPLUS */
 	0,	/* ASMINUS */
 	0,	/* ASSTAR */
 	0,	/* ASSLASH */
 	0,	/* ASPCT */
 	0,	/* ASCIRC */
 	0,	/* ASLSH */
	0,	/* ASRSH */
 	0,	/* ASOR */
 	0,	/* ASAND */
	0,	/* ELLIPS */
	0,	/* DSHARP1 */
	0,	/* NAME1 */
	0,	/* DEFINED */
	0,	/* UMINUS */
};

/**
 * maketokenrow - 建立一个Token Row
 * @size: 该Token Row的大小
 * @trp: Tokenrow结构
 * 注意：Tokenrow是可以动态扩展的
 */
void maketokenrow(int size, Tokenrow *trp) {
	trp->max = size; /* 设置token row中Token节点的个数 */
	if (size>0)
		trp->bp = (Token *)domalloc(size*sizeof(Token)); /* 分配Token节点 */
	else
		trp->bp = NULL;
	trp->tp = trp->bp;	/* 由于Token数组中还没有有效Token（当前只是分配了Token节点），故 tp == bp */
	trp->lp = trp->bp;	/* 由于Token数组中还没有有效Token（当前只是分配了Token节点），故 lp == bp */
}

/**
 * growtokenrow - 增大Tokenrow数组
 * @trp：要被增大的Tokenrow
 * 返回值：要写入Tokenrow的Token数组的新Token指针
 */
#include <assert.h>
Token * growtokenrow(Tokenrow *trp) {
	int ncur = trp->tp - trp->bp;
	int nlast = trp->lp - trp->bp;

	assert(ncur == 0); /* 这一行是我加的，换句话说： trp->tp变量压根没啥用... */
	trp->max = 3*trp->max/2 + 1; /* 增大1.5倍 */
	trp->bp = (Token *)realloc(trp->bp, trp->max*sizeof(Token)); /* 扩大Tokenrow中的Token数组 */
	if (trp->bp == NULL) /* 如果扩大数组失败 */
		error(FATAL, "Out of memory from realloc"); /* 打印错误消息并退出进程 */
	trp->lp = &trp->bp[nlast];
	trp->tp = &trp->bp[ncur];
	return trp->lp; /* 返回扩大后的当前Token指针 */
}

/*
 * Compare a row of tokens, ignoring the content of WS; return !=0 if different
 */
int comparetokens(Tokenrow *tr1, Tokenrow *tr2)
{
	Token *tp1, *tp2;

	tp1 = tr1->tp;
	tp2 = tr2->tp;
	if (tr1->lp-tp1 != tr2->lp-tp2)
		return 1;
	for (; tp1<tr1->lp ; tp1++, tp2++) {
		if (tp1->type != tp2->type
		 || (tp1->wslen==0) != (tp2->wslen==0)
		 || tp1->len != tp2->len
		 || strncmp((char*)tp1->t, (char*)tp2->t, tp1->len)!=0)
			return 1;
	}
	return 0;
}

/*
 * replace ntok tokens starting at dtr->tp with the contents of str.
 * tp ends up pointing just beyond the replacement.
 * Canonical whitespace is assured on each side.
 */
void insertrow(Tokenrow *dtr, int ntok, Tokenrow *str) {
	int nrtok = rowlen(str);

	dtr->tp += ntok;
	adjustrow(dtr, nrtok-ntok);
	dtr->tp -= ntok;
	movetokenrow(dtr, str);
	makespace(dtr);
	dtr->tp += nrtok;
	makespace(dtr);
}

/*
 * make sure there is WS before trp->tp, if tokens might merge in the output
 */
void makespace(Tokenrow *trp)
{
	uchar *tt;
	Token *tp = trp->tp;

	if (tp >= trp->lp)
		return;
	if (tp->wslen) {
		if (tp->flag&XPWS
		 && (wstab[tp->type] || trp->tp>trp->bp && wstab[(tp-1)->type])) {
			tp->wslen = 0;
			return;
		}
		tp->t[-1] = ' ';
		return;
	}
	if (wstab[tp->type] || trp->tp>trp->bp && wstab[(tp-1)->type])
		return;
	tt = newstring(tp->t, tp->len, 1);
	*tt++ = ' ';
	tp->t = tt;
	tp->wslen = 1;
	tp->flag |= XPWS;
}

/*
 * Copy an entire tokenrow into another, at tp.
 * It is assumed that there is enough space.
 *  Not strictly conforming.
 */
void movetokenrow(Tokenrow *dtr, Tokenrow *str)
{
	int nby;

	/* nby = sizeof(Token) * (str->lp - str->bp); */
	nby = (char *)str->lp - (char *)str->bp;
	memmove(dtr->tp, str->bp, nby);
}

/*
 * Move the tokens in a row, starting at tr->tp, rightward by nt tokens;
 * nt may be negative (left move).
 * The row may need to be grown.
 * Non-strictly conforming because of the (char *), but easily fixed
 */
void adjustrow(Tokenrow *trp, int nt)
{
	int nby, size;

	if (nt==0)
		return;
	size = (trp->lp - trp->bp) + nt;
	while (size > trp->max)
		growtokenrow(trp);
	/* nby = sizeof(Token) * (trp->lp - trp->tp); */
	nby = (char *)trp->lp - (char *)trp->tp;
	if (nby)
		memmove(trp->tp+nt, trp->tp, nby);
	trp->lp += nt;
}

/*
 * Copy a row of tokens into the destination holder, allocating
 * the space for the contents.  Return the destination.
 */
Tokenrow * copytokenrow(Tokenrow *dtr, Tokenrow *str)
{
	int len = rowlen(str);

	maketokenrow(len, dtr);
	movetokenrow(dtr, str);
	dtr->lp += len;
	return dtr;
}

/*
 * Produce a copy of a row of tokens.  Start at trp->tp.
 * The value strings are copied as well.  The first token
 * has WS available.
 */
Tokenrow * normtokenrow(Tokenrow *trp)
{
	Token *tp;
	Tokenrow *ntrp = new(Tokenrow); /* 新建一个Tokenrow节点 */
	int len;

	len = trp->lp - trp->tp; /* 获得trp中有效Token的个数 */
	if (len<=0) /* 如果个数小于0 */
		len = 1; /* 令个数为1 */
	maketokenrow(len, ntrp); /* 在新分配的Tokenrow中分配len个Token元素 */
	for (tp=trp->tp; tp < trp->lp; tp++) { /* 遍历trp中的Token元素 */
		*ntrp->lp = *tp; /* 将tp指向的当前节点复制给lp指向的位置 */
		if (tp->len) { /* 如果当前Token节点的字串长度不为0 */
			ntrp->lp->t = newstring(tp->t, tp->len, 1); /* 将tp中的字符串复制到lp中 */
			*ntrp->lp->t++ = ' '; /* 将该字符串结尾附加一个空格字符 */
			if (tp->wslen) /* TODO: 这个wslen到底是干啥的？ */
				ntrp->lp->wslen = 1;
		}
		ntrp->lp++; /* lp移动到下一节点 */
	}
	if (ntrp->lp > ntrp->bp) /* TODO: 这个wslen到底是干啥的？ */
		ntrp->bp->wslen = 0;
	return ntrp; /* 返回新建的Tokenrow节点的首地址 */
}

/*
 * Debugging
 */
void peektokens(Tokenrow *trp, char *str)
{
	Token *tp;

	tp = trp->tp; /* 得到token row中当前Token结构的首地址 */
	flushout();
	if (str)
		fprintf(stderr, "%s ", str);
	if (tp<trp->bp || tp>trp->lp)
		fprintf(stderr, "(tp offset %d) ", tp-trp->bp);
	for (tp=trp->bp; tp<trp->lp && tp<trp->bp+32; tp++) {
		if (tp->type!=NL) {
			int c = tp->t[tp->len];
			tp->t[tp->len] = 0;
			fprintf(stderr, "%s", tp->t);
			tp->t[tp->len] = c;
		}
		if (tp->type==NAME) {
			fprintf(stderr, tp==trp->tp?"{*":"{");
			prhideset(tp->hideset);
			fprintf(stderr, "} ");
		} else
			fprintf(stderr, tp==trp->tp?"{%x*} ":"{%x} ", tp->type);
	}
	fprintf(stderr, "\n");
	fflush(stderr);
}

void puttokens(Tokenrow *trp)
{
	Token *tp;
	int len;
	uchar *p;

	if (verbose) /* 如果开启了打印详细信息的选项 */
		peektokens(trp, "");
	tp = trp->bp; /* 拿到token row中第一个Token的首地址 */
	for (; tp < trp->lp; tp++) { /* 遍历token row中的token */
		len = tp->len+tp->wslen; /* 拿到当前Token的字符串长度。TODO：wslen是啥东西？ */
		p = tp->t - tp->wslen; /* 拿到当前Token的字符串首地址 */
		while (tp<trp->lp-1 && p+len == (tp+1)->t - (tp+1)->wslen) {
			tp++;
			len += tp->wslen+tp->len;
		}
		if (len > OBS/2) {	/* handle giant token */
			if (wbp > wbuf)
				fwrite(wbuf, 1, wbp-wbuf, stdout);
			fwrite((char *)p, 1, len, stdout);
			wbp = wbuf;
		} else {	
			memcpy(wbp, p, len); /* 将当前Token的字符串拷贝至wbp中 */
			wbp += len; /* 更新wbp的指针到新位置 */
		}
		if (wbp >= &wbuf[OBS]) {
			fwrite(wbuf, 1, OBS, stdout);
			if (wbp > &wbuf[OBS])
				memcpy(wbuf, wbuf+OBS, wbp - &wbuf[OBS]);
			wbp -= OBS;
		}
	}
	trp->tp = tp; /* 更新token row中的当前Token为token row中最后一个元素的下一元素的首地址 */
	if (cursource->fd==stdin)
		flushout();
}

void flushout(void)
{
	if (wbp > wbuf) {
		fwrite(wbuf, 1, wbp-wbuf, stdout);
		fflush(stdout);
		wbp = wbuf;
	}
}

/*
 * turn a row into just a newline
 */
void setempty(Tokenrow *trp)
{
	trp->tp = trp->bp;
	trp->lp = trp->bp+1;
	*trp->bp = nltoken;
}

/*
 * 将数值n转化为字符串输出
 */
char * outnum(char *p, int n) {
	if (n >= 10)
		p = outnum(p, n/10);
	*p++ = n % 10 + '0';
	return p;
}

/*
 * allocate and initialize a new string from s, of length l, at offset o
 * Null terminated.
 */
uchar * newstring(uchar *s, int l, int o)
{
	uchar *ns = (uchar *)domalloc(l+o+1);

	ns[l+o] = '\0';
	return (uchar*)strncpy((char*)ns+o, (char*)s, l) - o;
}
