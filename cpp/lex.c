#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cpp.h"

/**
 *   词法有限状态机编码(lexical FSM encoding)
 *
 *   当在状态state，并且在ch数组中的任意字符到来时,那么就进入nextstate.
 *   when in state state, and one of the characters
 *   in ch arrives, enter nextstate.
 *
 *   当 States >= S_SELF 时，要么进入了终态，要么至少需要进一步的处理。
 *   States >= S_SELF are either final, or at least require special action.
 *
 *   In 'fsm' there is a line for each state X charset X nextstate.
 *   List chars that overwrite previous entries later (e.g. C_ALPH
 *   can be overridden by '_' by a later entry; and C_XX is the
 *   the universal set, and should always be first.
 *
 *   那些高于S_SELF的状态，在大表(bigfsm)中呈现的是负值。
 *   States above S_SELF are represented in the big table as negative values.
 *
 *   S_SELF和S_SELFB将终态token的类型编码在高位中（高9位）。
 *   S_SELF and S_SELFB encode the resulting token type in the upper bits.
 *
 *   S_SELF和S_SELFB的区别在于:S_SELF没有lookahead字符,而S_SELFB有lookahead字符.
 *   These actions differ in that S_SELF doesn't have a lookahead char,S_SELFB does.
 *
 *   The encoding is blown out into a big table for time-efficiency.
 *   Entries have
 *      nextstate: 6 bits; ?\ marker: 1 bit; tokentype: 9 bits.
 */

#define		MAXSTATE		32				/* 最大的状态  */
#define		ACT(tok,act)	((tok<<7)+act)	/* 合成ACT(tok是toktype类型,占据高9位;act是action,类型是emun state) */
#define		QBSBIT			0100			/* 8进制，因此是第7位 */
#define		GETACT(st)		(st>>7)&0x1ff	/* 得到ACT(ACT其实就是一个token)，过滤出16位中的高9位 */

/* 字符类(character classes) */
#define		C_WS	1	/* 空白符(white space) */
#define		C_ALPH	2	/* 字母(Alpha),注意: 下划线也是字母 */
#define		C_NUM	3	/* 数字(Number) */
#define		C_EOF	4	/* 文件结束符(End Of File) */
#define		C_XX	5	/* 随机字符 */

/* 状态集合 */
enum state {
	START=0,/* 开始状态 */
	NUM1,	/* 阿拉伯数字状态( [0-9] ) */
	NUM2,	/* 浮点指数状态('e' or 'E') */
	NUM3, 	/* 浮点小数状态('.') */
	ID1,	/* 标识符状态( [a-zA-Z_] ) */
	ST1,	/* 宽字符状态('L') */
	ST2,	/* 双引号字符串状态( '"' ) */
	ST3,	/* 字符串转义状态( '\\' ) */
	COM1,	/* C注释状态1 ( '/' ) */
	COM2,	/* C注释状态2 ( "*" ) */
	COM3,	/* C注释状态3 ( "*" ) */
	COM4,	/* C++单行注释状态 ( // ) */
	CC1,	/* 单引号字符常量状态('\'') */
	CC2,	/* 字符常量转义状态( '\\' ) */
	WS1,	/* 空白符状态. ' '空格;'\t'横向制表符;'\v'纵向制表符 */
	PLUS1,	/* 加号状态('+') */
	MINUS1,	/* 减号状态('-') */
	STAR1,	/* 星号字符状态('*') */
	SLASH1,	/* TODO: 这个状态没在其他地方被引用... */
	PCT1,	/* '%' 百分号字符状态 */
	SHARP1, /* '#' Sharp字符状态 */
	CIRC1,	/* 脱字符('^',Caret)状态 */
	GT1,	/* '>' 大于号状态 */
	GT2,	/* 右移运算符状态( >> ) */
	LT1,	/* 小于号状态('<') */
	LT2,	/* 左移运算符状态( << )   */
	OR1,	/* 或状态('|') */
	AND1,	/* 与状态('&') */
	ASG1,	/* 赋值符号状态('=') */
	NOT1,	/* 逻辑非状态('!') */
	DOTS1,	/* 成员运算符状态 ( '.' ) */
	S_SELF=MAXSTATE, /* 接受状态,不需要再lookahead字符 */
	S_SELFB, /* 接受状态,需要进一步lookahead字符 */
	S_EOF,	/* 文件结束状态(接受状态) */
	S_NL,	/* 换行状态(接受状态) */
	S_EOFSTR, /* 字符串(或字符常量)中出现了文件结束符的状态(接受状态,非法状态) */
	S_STNL, /* 字符串(或字符常量)中出现了换行符的状态(接受状态,非法状态) */
	S_COMNL, /* C注释中出现换行符.NOTE: C注释是允许跨行的(接受状态) */
	S_EOFCOM, /* C注释（或C++注释）中遇到了文件结束符后的状态(接受状态，非法状态) */
	S_COMMENT, /* C注释被接受的状态 */
	S_EOB,	/* End Of Buffer 状态 */
	S_WS,	/* 空白字符被接受状态 */
	S_NAME	/* 标识符被接受状态 */
};

int	tottok; /* TODO: 这个变量在工程中没被使用... */
int	tokkind[256]; /* TODO: 这个变量在工程中没被使用... */

/**
 * fsm - 有限状态机(finite state machine)
 */
struct	fsm {
	int	state;			/* 如果在这个状态(if in this state) */
	uchar	ch[4];		/* 并且查看如果有在ch数组中的任意字符到来时(and see one of these characters) */
	int	nextstate;		/* 进入nextstate,TODO: 什么是 +ve ? (enter this state if +ve) */
};

/**
 * 有ACT的nextstate基本都是接受状态，开始有ACTION了
 */
/*const*/ struct fsm fsm[] = {
	/* 开始状态 */
	START,	{ C_XX },	ACT(UNCLASS,S_SELF),
	START,	{ ' ', '\t', '\v' },	WS1,
	START,	{ C_NUM },	NUM1,
	START,	{ '.' },	NUM3,
	START,	{ C_ALPH },	ID1,
	START,	{ 'L' },	ST1,
	START,	{ '"' },	ST2,
	START,	{ '\'' },	CC1,
	START,	{ '/' },	COM1,
	START,	{ EOFC },	S_EOF,
	START,	{ '\n' },	S_NL,
	START,	{ '-' },	MINUS1,
	START,	{ '+' },	PLUS1,
	START,	{ '<' },	LT1,
	START,	{ '>' },	GT1,
	START,	{ '=' },	ASG1,
	START,	{ '!' },	NOT1,
	START,	{ '&' },	AND1,
	START,	{ '|' },	OR1,
	START,	{ '#' },	SHARP1,
	START,	{ '%' },	PCT1,
	START,	{ '[' },	ACT(SBRA,S_SELF),
	START,	{ ']' },	ACT(SKET,S_SELF),
	START,	{ '(' },	ACT(LP,S_SELF),
	START,	{ ')' },	ACT(RP,S_SELF),
	START,	{ '*' },	STAR1,
	START,	{ ',' },	ACT(COMMA,S_SELF),
	START,	{ '?' },	ACT(QUEST,S_SELF),
	START,	{ ':' },	ACT(COLON,S_SELF),
	START,	{ ';' },	ACT(SEMIC,S_SELF),
	START,	{ '{' },	ACT(CBRA,S_SELF),
	START,	{ '}' },	ACT(CKET,S_SELF),
	START,	{ '~' },	ACT(TILDE,S_SELF),
	START,	{ '^' },	CIRC1,

	/* 看见一个数字 */
	NUM1,	{ C_XX },	ACT(NUMBER,S_SELFB),
	NUM1,	{ C_NUM, C_ALPH, '.' },	NUM1,
	NUM1,	{ 'E', 'e' },	NUM2,
	NUM1,	{ '_' },	ACT(NUMBER,S_SELFB),

	/* saw possible start of exponent, digits-e */
	NUM2,	{ C_XX },	ACT(NUMBER,S_SELFB),
	NUM2,	{ '+', '-' },	NUM1,
	NUM2,	{ C_NUM, C_ALPH },	NUM1,
	NUM2,	{ '_' },	ACT(NUMBER,S_SELFB),

	/* saw a '.', which could be a number or an operator */
	NUM3,	{ C_XX },	ACT(DOT,S_SELFB),
	NUM3,	{ '.' },	DOTS1,
	NUM3,	{ C_NUM },	NUM1,

	DOTS1,	{ C_XX },	ACT(UNCLASS, S_SELFB),
	DOTS1,	{ C_NUM },	NUM1,
	DOTS1,	{ '.' },	ACT(ELLIPS, S_SELF),

	/* saw a letter or _ */
	ID1,	{ C_XX },	ACT(NAME,S_NAME),
	ID1,	{ C_ALPH, C_NUM },	ID1,

	/* saw L (start of wide string?) */
	ST1,	{ C_XX },	ACT(NAME,S_NAME),
	ST1,	{ C_ALPH, C_NUM },	ID1,
	ST1,	{ '"' },	ST2,
	ST1,	{ '\'' },	CC1,

	/* saw " beginning string */
	ST2,	{ C_XX },	ST2,
	ST2,	{ '"' },	ACT(STRING, S_SELF),
	ST2,	{ '\\' },	ST3,
	ST2,	{ '\n' },	S_STNL,
	ST2,	{ EOFC },	S_EOFSTR,

	/* saw \ in string */
	ST3,	{ C_XX },	ST2,
	ST3,	{ '\n' },	S_STNL,
	ST3,	{ EOFC },	S_EOFSTR,

	/* saw ' beginning character const */
	CC1,	{ C_XX },	CC1,
	CC1,	{ '\'' },	ACT(CCON, S_SELF),
	CC1,	{ '\\' },	CC2,
	CC1,	{ '\n' },	S_STNL,
	CC1,	{ EOFC },	S_EOFSTR,

	/* saw \ in ccon */
	CC2,	{ C_XX },	CC1,
	CC2,	{ '\n' },	S_STNL,
	CC2,	{ EOFC },	S_EOFSTR,

	/* saw /, perhaps start of comment */
	COM1,	{ C_XX },	ACT(SLASH, S_SELFB),
	COM1,	{ '=' },	ACT(ASSLASH, S_SELF),
	COM1,	{ '*' },	COM2,
	COM1,	{ '/' },	COM4,

	/* saw / then *, start of comment */
	COM2,	{ C_XX },	COM2,
	COM2,	{ '\n' },	S_COMNL,
	COM2,	{ '*' },	COM3,
	COM2,	{ EOFC },	S_EOFCOM,

	/* saw the * possibly ending a comment */
	COM3,	{ C_XX },	COM2,
	COM3,	{ '\n' },	S_COMNL,
	COM3,	{ '*' },	COM3,
	COM3,	{ '/' },	S_COMMENT,

	/* // comment */
	COM4,	{ C_XX },	COM4,
	COM4,	{ '\n' },	S_NL,
	COM4,	{ EOFC },	S_EOFCOM,

	/* saw white space, eat it up */
	WS1,	{ C_XX },	S_WS,
	WS1,	{ ' ', '\t', '\v' },	WS1,

	/* saw -, check --, -=, -> */
	MINUS1,	{ C_XX },	ACT(MINUS, S_SELFB),
	MINUS1,	{ '-' },	ACT(MMINUS, S_SELF),
	MINUS1,	{ '=' },	ACT(ASMINUS,S_SELF),
	MINUS1,	{ '>' },	ACT(ARROW,S_SELF),

	/* saw +, check ++, += */
	PLUS1,	{ C_XX },	ACT(PLUS, S_SELFB),
	PLUS1,	{ '+' },	ACT(PPLUS, S_SELF),
	PLUS1,	{ '=' },	ACT(ASPLUS, S_SELF),

	/* saw <, check <<, <<=, <= */
	LT1,	{ C_XX },	ACT(LT, S_SELFB),
	LT1,	{ '<' },	LT2,
	LT1,	{ '=' },	ACT(LEQ, S_SELF),
	LT2,	{ C_XX },	ACT(LSH, S_SELFB),
	LT2,	{ '=' },	ACT(ASLSH, S_SELF),

	/* saw >, check >>, >>=, >= */
	GT1,	{ C_XX },	ACT(GT, S_SELFB),
	GT1,	{ '>' },	GT2,
	GT1,	{ '=' },	ACT(GEQ, S_SELF),
	GT2,	{ C_XX },	ACT(RSH, S_SELFB),
	GT2,	{ '=' },	ACT(ASRSH, S_SELF),

	/* = */
	ASG1,	{ C_XX },	ACT(ASGN, S_SELFB),
	ASG1,	{ '=' },	ACT(EQ, S_SELF),

	/* ! */
	NOT1,	{ C_XX },	ACT(NOT, S_SELFB),
	NOT1,	{ '=' },	ACT(NEQ, S_SELF),

	/* & */
	AND1,	{ C_XX },	ACT(AND, S_SELFB),
	AND1,	{ '&' },	ACT(LAND, S_SELF),
	AND1,	{ '=' },	ACT(ASAND, S_SELF),

	/* | */
	OR1,	{ C_XX },	ACT(OR, S_SELFB),
	OR1,	{ '|' },	ACT(LOR, S_SELF),
	OR1,	{ '=' },	ACT(ASOR, S_SELF),

	/* # */
	SHARP1,	{ C_XX },	ACT(SHARP, S_SELFB),
	SHARP1,	{ '#' },	ACT(DSHARP, S_SELF),

	/* % */
	PCT1,	{ C_XX },	ACT(PCT, S_SELFB),
	PCT1,	{ '=' },	ACT(ASPCT, S_SELF),

	/* * */
	STAR1,	{ C_XX },	ACT(STAR, S_SELFB),
	STAR1,	{ '=' },	ACT(ASSTAR, S_SELF),

	/* ^ */
	CIRC1,	{ C_XX },	ACT(CIRC, S_SELFB),
	CIRC1,	{ '=' },	ACT(ASCIRC, S_SELF),

	-1
};

/**
 *  bigfsm - 大状态机(big finite state machine)
 *  第一个索引是'字符'，第二个索引是'状态'(first index is char, second is state)
 *  increase #states to power of 2 to encourage use of shift
 */
short	bigfsm[256][MAXSTATE];

#ifdef DEBUG_FSM
void print_fsm(struct fsm ar[],int n) {
	int i,j;
	for(i=0;i<n;i++) {
		printf("state=%02d,",ar[i].state);
		printf("ch[4]:");
		for(j=0;j<4;j++) {
			if(ar[i].ch[j]>=32 && ar[i].ch[j]<128) {
				printf("  '%c'",ar[i].ch[j]);
			} else {
				printf(" 0x%02x",ar[i].ch[j]);
			}
		}
		printf(", nextstate = 0x%04x\n",(unsigned short)(ar[i].nextstate));
	}
}

void print_bigfsm() {
	int i,j;
	printf("bigfsm:\n");
	for(i=0; i<256;i++) {
		printf("%3d:",i);
		for(j=0;j<32;j++) {
			if(j%10==0)
			printf("[%d]",j);
			printf("%04x,",(unsigned short)bigfsm[i][j]);
		}
		printf("\n");
	}
}
#endif /* DEBUG_FSM */

/**
 * expandlex - 展开状态机
 */
void expandlex(void)
{
	/*const*/ struct fsm *fp;
	int i, j, nstate;

#ifdef TEST_LCPP
	print_fsm(fsm,sizeof(fsm)/sizeof(fsm[0]));
	exit(0);
#endif

	for (fp = fsm; fp->state>=0; fp++) { /* 对于任意大于0的状态 */
		for (i=0; fp->ch[i]; i++) { /* 如果其下一字符不为0 */
			nstate = fp->nextstate; /* 获取到它的下一状态 */
			if (nstate >= S_SELF) /* 如果下一状态的值大于S_SELF（说明是接受状态或非法状态） */
				nstate = ~nstate; /* 对下一状态按位取反（取反后，第7位由0变成了1）。为啥要取反？答曰：可以使接受状态（或非法状态）小于0,从而使得和非接受状态区分开来。 */
			switch (fp->ch[i]) {

			case C_XX:		/* 如果下一字符是随机（任一）字符 */
				for (j=0; j<256; j++)
					bigfsm[j][fp->state] = nstate;	/* 设置当前状态遇到任意字符后从fp->state跳转到nstate */
				continue;
			case C_ALPH:	/* 如果下一字符是字母或下划线 */
				for (j=0; j<=256; j++)
					if ('a'<=j&&j<='z' || 'A'<=j&&j<='Z'
					  || j=='_')
						bigfsm[j][fp->state] = nstate; /* 设置当前状态，遇到字母或下划线后从fp->state跳转到nstate */
				continue;
			case C_NUM: /* 如果下一字符是阿拉伯数字 */
				for (j='0'; j<='9'; j++)
					bigfsm[j][fp->state] = nstate; /* 设置当前状态，遇到阿拉伯数字后从状态fp->state跳转道nstate */
				continue;
			default:
				bigfsm[fp->ch[i]][fp->state] = nstate;
			}
		}
	}
	/**
	 *  install special cases for:
	 *  ? (trigraphs，三字符序列),
	 *  \ (splicing，续行符),
	 *  runes(古日尔曼字母),
	 *  and EOB (end of input buffer)
	 */
	for (i=0; i < MAXSTATE; i++) { /* 对于每一状态i */
		for (j=0; j<0xFF; j++) /* 对于每一行（每一字符j） */
			if (j=='?' || j=='\\') { /* 如果该字符是'?'或'\\' */
				if (bigfsm[j][i]>0) /* 如果该状态i对应于字符j的下一状态大于0（说明是非接受状态，而且第7位是0） */
					bigfsm[j][i] = ~bigfsm[j][i]; /* 将该“下一状态”各位按位取反（将该“下一状态”变成接受状态，而且第7位从0变成了1） */
				bigfsm[j][i] &= ~QBSBIT; /* 将该“下一状态”的第7位清0。 */
			}
		bigfsm[EOB][i] = ~S_EOB; /* 状态i的下一字符是EOB，将S_EOB取反。取反后，第7位为1 */
		if (bigfsm[EOFC][i]>=0) /* 如果状态i对应于字符EOFC的状态大于0 */
			bigfsm[EOFC][i] = ~S_EOF; /* 将S_EOF取反。取反后，第7位为0 */
	}
	/* 综上：第7位实际上起到了判断一个状态是否是接受状态的标识位。如果第7位为1,接受状态;反之，若为0,非接受状态。 */
#ifdef DEBUG_FSM
	print_bigfsm();
	exit(1);
#endif
}

/**
 * fixlex - 添加C++的单行注释功能
 */
void fixlex(void)
{
	/* do C++ comments? */
	if (Cplusplus==0) /* 如果没有开启C++注释支持 */
		bigfsm['/'][COM1] = bigfsm['x'][COM1]; /* 那么只是简单的把C++注释当作除法运算符 */
}

/**
 * gettokens - 从输入源的缓冲区中得到一个 token row
 * @trp: 保存返回的 token row
 * @reset: reset如果为非0，则表示输入缓冲可以被rewind
 * 返回值： TODO
 * NOTE: fill in a row of tokens from input, terminated by NL or END
 * First token is put at trp->lp.
 * Reset is non-zero when the input buffer can be "rewound."
 * The value is a flag indicating that possible macros have
 * been seen in the row.
 */
int gettokens(Tokenrow *trp, int reset)
{
	register int c, state, oldstate;
	register uchar *ip;
	register Token *tp, *maxp;
	int runelen;
	Source *s = cursource; /* 从当前输入源得到token */
	int nmac = 0;
	extern char outbuf[];

	tp = trp->lp; /* 拿到token row中第一个token结构的首地址 */
	ip = s->inp; /* 拿到输入源缓冲区中当前正在处理的字符的首地址 */
	if (reset) { /* 如果如果缓冲区可以被“重绕” */
		s->lineinc = 0; /* TODO:  */
		if (ip>=s->inl) { /* 如果缓冲区中无数据 */
			s->inl = s->inb;
			fillbuf(s); /* 从输入源加载数据到缓冲区中 */
			ip = s->inp = s->inb;
		} else if (ip >= s->inb+(3*INS/4)) { /* 如果缓冲区中有数据，但当前指针已经超过了buffer大小的3/4,那么开始“重绕” */
			memmove(s->inb, ip, 4+s->inl-ip); /* 将缓冲区中的有效数据复制到缓冲区开始 */
			s->inl = s->inb+(s->inl-ip); /* 重新设置缓冲区末尾指针 */
			ip = s->inp = s->inb; /* 重新设置缓冲区开始指针和当前指针 */
		}
	}
	maxp = &trp->bp[trp->max]; /* 拿到token row最后一个元素的下一个元素的首地址(哨兵地址） */
	runelen = 1;
	for (;;) {
	   continue2:
		if (tp>=maxp) {
			trp->lp = tp;
			tp = growtokenrow(trp);
			maxp = &trp->bp[trp->max];
		}
		tp->type = UNCLASS;
		tp->hideset = 0;
		tp->t = ip;
		tp->wslen = 0;
		tp->flag = 0;
		state = START;
		for (;;) {
			oldstate = state;
			c = *ip;
			if ((state = bigfsm[c][state]) >= 0) {
				ip += runelen;
				runelen = 1;
				continue;
			}
			state = ~state;
		reswitch:
			switch (state&0177) {
			case S_SELF:
				ip += runelen;
				runelen = 1;
			case S_SELFB:
				tp->type = GETACT(state);
				tp->len = ip - tp->t;
				tp++;
				goto continue2;

			case S_NAME:	/* like S_SELFB but with nmac check */
				tp->type = NAME;
				tp->len = ip - tp->t;
				nmac |= quicklook(tp->t[0], tp->len>1?tp->t[1]:0);
				tp++;
				goto continue2;

			case S_WS:
				tp->wslen = ip - tp->t;
				tp->t = ip;
				state = START;
				continue;

			default:
				if ((state&QBSBIT)==0) {
					ip += runelen;
					runelen = 1;
					continue;
				}
				state &= ~QBSBIT;
				s->inp = ip;
				if (c=='?') { 	/* check trigraph */
					if (trigraph(s)) {
						state = oldstate;
						continue;
					}
					goto reswitch;
				}
				if (c=='\\') { /* line-folding */
					if (foldline(s)) {
						s->lineinc++;
						state = oldstate;
						continue;
					}
					goto reswitch;
				}
				error(WARNING, "Lexical botch in cpp");
				ip += runelen;
				runelen = 1;
				continue;

			case S_EOB:
				s->inp = ip;
				fillbuf(cursource);
				state = oldstate;
				continue;

			case S_EOF:
				tp->type = END;
				tp->len = 0;
				s->inp = ip;
				if (tp!=trp->bp && (tp-1)->type!=NL && cursource->fd!=NULL)
					error(WARNING,"No newline at end of file");
				trp->lp = tp+1;
				return nmac;

			case S_STNL:
				error(ERROR, "Unterminated string or char const");
			case S_NL:
				tp->t = ip;
				tp->type = NL;
				tp->len = 1;
				tp->wslen = 0;
				s->lineinc++;
				s->inp = ip+1;
				trp->lp = tp+1;
				return nmac;

			case S_EOFSTR:
				error(FATAL, "EOF in string or char constant");
				break;

			case S_COMNL:
				s->lineinc++;
				state = COM2;
				ip += runelen;
				runelen = 1;
				if (ip >= s->inb+(7*INS/8)) { /* very long comment */
					memmove(tp->t, ip, 4+s->inl-ip);
					s->inl -= ip-tp->t;
					ip = tp->t+1;
				}
				continue;

			case S_EOFCOM:
				error(WARNING, "EOF inside comment");
				--ip;
			case S_COMMENT:
				++ip;
				tp->t = ip;
				tp->t[-1] = ' ';
				tp->wslen = 1;
				state = START;
				continue;
			}
			break;
		}
		ip += runelen;
		runelen = 1;
		tp->len = ip - tp->t;
		tp++;
	}
}

/* have seen ?; handle the trigraph it starts (if any) else 0 */
int trigraph(Source *s)
{
	int c;

	while (s->inp+2 >= s->inl && fillbuf(s)!=EOF)
		;
	if (s->inp[1]!='?')
		return 0;
	c = 0;
	switch(s->inp[2]) {
	case '=':
		c = '#'; break;
	case '(':
		c = '['; break;
	case '/':
		c = '\\'; break;
	case ')':
		c = ']'; break;
	case '\'':
		c = '^'; break;
	case '<':
		c = '{'; break;
	case '!':
		c = '|'; break;
	case '>':
		c = '}'; break;
	case '-':
		c = '~'; break;
	}
	if (c) {
		*s->inp = c;
		memmove(s->inp+1, s->inp+3, s->inl-s->inp+2);
		s->inl -= 2;
	}
	return c;
}

int foldline(Source *s)
{
	while (s->inp+1 >= s->inl && fillbuf(s)!=EOF)
		;
	if (s->inp[1] == '\n') {
		memmove(s->inp, s->inp+2, s->inl-s->inp+3);
		s->inl -= 2;
		return 1;
	}
	return 0;
}

int fillbuf(Source *s)
{
	int n, nr;

	nr = INS/8;
	if ((char *)s->inl+nr > (char *)s->inb+INS)
		error(FATAL, "Input buffer overflow");
	if (s->fd==NULL || (n=fread((char *)s->inl, 1, INS/8, s->fd)) <= 0)
		n = 0;
	if ((*s->inp&0xff) == EOB) /* sentinel character appears in input */
		*s->inp = EOFC;
	s->inl += n;
	s->inl[0] = s->inl[1]= s->inl[2]= s->inl[3] = EOB;
	if (n==0) {
		s->inl[0] = s->inl[1]= s->inl[2]= s->inl[3] = EOFC;
		return EOF;
	}
	return 0;
}

/**
 * setsource - 设置输入源
 * @name：输入源的文件名
 * @fd：输入源的文件指针
 * @str：预先往输入源中放置的待处理的字符串
 * 返回值： 返回新分配的输入源节点的首地址
 * Push down to new source of characters.
 * If fd!=NULL and str==NULL, then from a file `name';
 * if fd==NULL and str, then from the string.
 */
Source * setsource(char *name, FILE *fd, char *str)
{
	Source *s = new(Source);
	int len;

	s->line = 1; /* 设置当前行号为1 */
	s->lineinc = 0; /* TODO: 啥意思？ */
	s->fd = fd;	/* 设置source的文件指针 */
	s->filename = name; /* 设置文件的名字 */
	s->next = cursource; /* 头插法插入节点（此链表是一个单向链表） */
	s->ifdepth = 0; /* 条件编译指令的陷入深度 */
	cursource = s;  /* 设置当前源为新分配的节点 */
	/* slop at right for EOB */
	if (str) { /* 如果str不为空 */
		len = strlen(str); /* 计算该str的字符串长度 */
		s->inb = domalloc(len+4); /* 分配输入缓冲（之所以+4是因为，要在str后面放置几个哨兵字符） */
		s->inp = s->inb; /* 矫正输入缓冲的基地址指针和当前指针 */
		strncpy((char *)s->inp, str, len); /* 将str复制到新分配的输入缓冲中去 */
	} else { /* 如果str为空 */
		s->inb = domalloc(INS+4); /* 分配输入缓冲区 */
		s->inp = s->inb; /* 矫正输入缓冲的基地址指针和当前指针 */
		len = 0;
	}
	s->inl = s->inp+len; /* s->inl指向输入缓冲中最后一个有效字符的下一个字符 */
	s->inl[0] = s->inl[1] = EOB; /* 设置End Of Buffer的哨兵字符 */
	return s; /* 返回新创立的源节点 */
}

/**
 * unsetsource - 取消栈顶的输入源节点
 */
void unsetsource(void)
{
	Source *s = cursource;

	if (s->fd != NULL) {
		fclose(s->fd);
		dofree(s->inb);
	}
	cursource = s->next;
	dofree(s);
}
