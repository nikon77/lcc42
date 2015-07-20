#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cpp.h"

extern	int		getopt(int, char *const *, const char *);
extern	char	*optarg;
extern	int		optind;
extern	int		verbose;
extern	int		Cplusplus;
Nlist	*kwdefined;	/* 指向defined关键字的Nlist结构 */

#define		NLSIZE		128				/* hash桶的大小 */
static		Nlist		*nlist[NLSIZE];	/* hash桶 */

struct	kwtab {
	char	*kw;	/* 关键字字串 */
	int		val;	/* 关键字的枚举值; enum kwtype 类型 */
	int		flag;	/* 关键字的flag,常见的值有: ISKW,ISDEFINED,ISUNCHANGE,ISMAC */
} kwtab[] = {
	"if",		KIF,		ISKW,
	"ifdef",	KIFDEF,		ISKW,
	"ifndef",	KIFNDEF,	ISKW,
	"elif",		KELIF,		ISKW,
	"else",		KELSE,		ISKW,
	"endif",	KENDIF,		ISKW,
	"include",	KINCLUDE,	ISKW,
	"define",	KDEFINE,	ISKW,
	"undef",	KUNDEF,		ISKW,
	"line",		KLINE,		ISKW,
	"error",	KERROR,		ISKW,
	"pragma",	KPRAGMA,	ISKW,
	"eval",		KEVAL,		ISKW,
	"defined",	KDEFINED,	ISDEFINED+ISUNCHANGE,
	"ident",	KPRAGMA,	ISKW,	/* treat like pragma (ignored) */
	"__LINE__",	KLINENO,	ISMAC+ISUNCHANGE,
	"__FILE__",	KFILE,		ISMAC+ISUNCHANGE,
	"__DATE__",	KDATE,		ISMAC+ISUNCHANGE,
	"__TIME__",	KTIME,		ISMAC+ISUNCHANGE,
	"__STDC__",	KSTDC,		ISMAC+ISUNCHANGE,
	NULL
};

unsigned long	namebit[077+1]; /* 共有64个long型。TODO：这个数组到底是干啥的？ */
Nlist 	*np; /* TODO: 工程中没有任何地方引用这个变量... */

/**
 * setup_kwtab - 安装关键字hash表
 */
void setup_kwtab(void) {
	struct kwtab *kp;
	Nlist *np;
	Token t;
	static Token deftoken[1] = {{ NAME, 0, 0, 0, 7, (uchar*)"defined" }};
	static Tokenrow deftr = { deftoken, deftoken, deftoken+1, 1 };

	for (kp=kwtab; kp->kw; kp++) { /* 遍历kwtab */
		t.t = (uchar*)kp->kw;	/* 拿到关键字的字符串 */
		t.len = strlen(kp->kw);	/* 得到关键字字符串的长度 */
		np = lookup(&t, 1);		/* 将关键字插入hash表（注意：是将Token结构转化为Nlist结构，存储到hash表中的） */
		np->flag = kp->flag;	/* 设置hash表节点中关键字的flag */
		np->val = kp->val;		/* 设置hash表节点中关键字的val */
		if (np->val == KDEFINED) { /* 如果是defined关键字 */
			kwdefined = np;	/* 使kwdefined指向defined关键字的Nlist结构首地址 */
			np->val = NAME; /* 设置为：标识符名，token */
			np->vp = &deftr; /* np->vp指向defined关键字的宏展开的值 */
			np->ap = 0; /* 设置参数名的列表暂时为0 */
		}
	}
}

/**
 * lookup - 在hash表中查找token或将token（转化为Nlist结构）插入到hash表
 * @install: 当install=0时，表示查询hash表;当install=1时，表示将token插入hash表
 */
Nlist * lookup(Token *tp, int install) {
	unsigned int h;	/* 存储字符串的hash值 */
	Nlist *np;
	uchar *cp, *cpe; /* cp指向字符串的开始;cpe指向字符串的结尾 */

	h = 0; /* 令hash值初值为0 */
	for (cp=tp->t, cpe=cp+tp->len; cp<cpe; )
		h += *cp++; /* 通过字符值累加来获取字符串的hash值 */
	h %= NLSIZE; /* 拿着这个hash值对NLSIZE求模，获得hash桶的索引 */
	np = nlist[h]; /* 获取hash桶中索引为h的链表的表头 */
	while (np) { /* 在该链表中查找名为 tp->t 的节点 */
		if ( *tp->t==*np->name && tp->len==np->len
		 && strncmp((char*)tp->t, (char*)np->name, tp->len)==0 ) /* 如果链表中当前节点的字串和待查找的字串相同 */
			return np; /* 那么直接返回此节点的首地址 */
		np = np->next; /* 继续查找链表中的下一元素 */
	}
	if (install) { /* 如果之前在hash表中没找到此节点且 install != 0 那么将该Token结构转化为Nlist结构插入hash表中 */
		np = new(Nlist); /* 分配一个Nlist结构 */
		np->vp = NULL; /* 初始化该Nlist结构 */
		np->ap = NULL;
		np->flag = 0;
		np->val = 0;
		np->len = tp->len; /* 将字串长度保存到Nlist结构中 */
		np->name = newstring(tp->t, tp->len, 0); /* 将字串拷贝一份备份到Nlist结构中 */
		np->next = nlist[h]; /* 将Nlist结构插入到索引为h的链表中 */
		nlist[h] = np;
		quickset(tp->t[0], tp->len>1? tp->t[1]:0); /* XXX:注意，这里不是逗号表达式，quickset是个宏！TODO: namebit到底是干啥的？ */
		return np; /* 返回新插入的Nlist节点的首地址 */
	}
	return NULL; /* 如果之前在hash表中没找到此节点且install参数为0,那么返回NULL，说明没找到 */
}
