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
#define		GETACT(st)		(st>>7)&0x1ff	/* 得到ACT(ACT其实就是一个tokentype)，过滤出16位中的高9位 */

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
	CIRC1,	/* 脱字符( '^',Caret or Circ )状态 */
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
	S_EOFSTR, /* 字符串(或字符常量)中出现了文件结束符的状态(非法状态) */
	S_STNL, /* 字符串(或字符常量)中出现了换行符的状态(接受状态,非法状态) */
	S_COMNL, /* C注释中出现换行符.NOTE: C注释是允许跨行的(特殊状态) */
	S_EOFCOM, /* C注释（或C++注释）中遇到了文件结束符后的状态(非法状态) */
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

//#define DEBUG_FSM
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
		for(j=0;j<MAXSTATE;j++) {
			if(j%10==0)
			printf("[%d]",j);
			printf("%04x,",(unsigned short)bigfsm[i][j]);
		}
		printf("\n");
	}
}

#include <assert.h>

void assert_bigfsm_flag() {
	int indexi, indexj, msb, bit7;
	for (indexi = 0; indexi < 256; indexi++) {
		for (indexj = 0; indexj < MAXSTATE; indexj++) {
			msb = (bigfsm[indexi][indexj] & 0x8000) >> 15;
			bit7 = (bigfsm[indexi][indexj] & 0100) >> 6;
			if (msb == 0 && bit7 == 0) assert(0);
			if (msb == 0 && bit7 == 1) assert(0);
			if (msb == 1 && bit7 == 0) assert(0);
			if (msb == 1 && bit7 == 1) assert(0);
		}
	}
}
#endif /* DEBUG_FSM */

/**
 * expandlex - 展开状态机
 */
void expandlex(void) {
	/*const*/ struct fsm *fp;
	int i, j, nstate;

#ifdef DEBUG_FSM
	print_fsm(fsm,sizeof(fsm)/sizeof(fsm[0]));
	exit(0);
#endif

	for (fp = fsm; fp->state>=0; fp++) { /* 对任意非负的状态 */
		for (i=0; fp->ch[i]; i++) { /* 如果其下一字符不为0 */
			nstate = fp->nextstate; /* 获取到它的下一状态(迁移状态) */
			if (nstate >= S_SELF) /* 如果下一状态的值大于S_SELF,说明是接受状态（ACT包装的状态）或非法状态（大于等于32,且小于44） */
				nstate = ~nstate; /* 对下一状态按位取反。为啥要取反？可以使接受状态（或非法状态）小于0,从而使得和非接受状态区分开来。（取反后，MSB为1,BIT7为1） */
			switch (fp->ch[i]) {

			case C_XX:		/* 如果下一字符是随机（任一）字符 */
				for (j=0; j<256; j++)
					bigfsm[j][fp->state] = nstate;	/* 设置当前状态遇到任意字符后的迁移状态为nstate */
				continue;
			case C_ALPH:	/* 如果下一字符是字母或下划线 */
				for (j=0; j<=256; j++)
					if ('a'<=j&&j<='z' || 'A'<=j&&j<='Z'
					  || j=='_')
						bigfsm[j][fp->state] = nstate; /* 设置当前状态遇到字母或下划线后的迁移状态为nstate */
				continue;
			case C_NUM: /* 如果下一字符是阿拉伯数字 */
				for (j='0'; j<='9'; j++)
					bigfsm[j][fp->state] = nstate; /* 设置当前状态遇到阿拉伯数字后的迁移状态为nstate */
				continue;
			default:
				bigfsm[fp->ch[i]][fp->state] = nstate; /* 设置当前状态遇到fp->ch[i]后的迁移状态为nstate */
			}
		}
	}
#ifdef DEBUG_FSM
	assert_bigfsm_flag();
#endif
	/**
	 * 至此：bigfsm数组中每个元素的最高位（MSB：most significant bit）和第7位（从最低位开始数）构成以下了4种情况：
	 * MSB     ：       BIT7
	 *  0      ：        0		正常的状态变迁
	 *  0      ：        1		这种情况不存在
	 *  1      ：        0		这种情况不存在
	 *  1      ：        1		非法状态（大于等于32,且小于44） 或 接受状态（ACT包装的接受状态）
	 */

	/**
	 *  install special cases for:
	 *  ? (trigraphs，三字符序列),
	 *  \ (splicing，续行符),
	 *  runes(古日尔曼字母), TODO：程序中未做处理
	 *  and EOB (end of input buffer)
	 */
	for (i=0; i < MAXSTATE; i++) { /* 对于每一状态i */
		for (j=0; j<0xFF; j++) /* 查看该状态i的下一字符j */
			if (j=='?' || j=='\\') { /* 如果该字符是'?'或'\\' */
				if (bigfsm[j][i]>0) /* 如果该状态i对应于字符j的迁移状态大于0（说明不是接受状态也不是非法状态，此时第7位是0，低6位小于32） */
					bigfsm[j][i] = ~bigfsm[j][i]; /* 将该“迁移状态”各位按位取反,取反后： MSB为1，BIT7也为1 */
				bigfsm[j][i] &= ~QBSBIT; /* 再将该“迁移状态”的第7位清0 */
			}
		bigfsm[EOB][i] = ~S_EOB; /* 将状态i对应于字符EOB的迁移状态设置为S_EOB的取反（取反后，MSB为1,第7位为1） */
		if (bigfsm[EOFC][i] >= 0) /* 如果状态i对应于字符EOFC的迁移状态大于0 */
			bigfsm[EOFC][i] = ~S_EOF; /* 将S_EOF取反（取反后，第7位为1,最高位为1） */
	}
#ifdef DEBUG_FSM
	assert_bigfsm_flag();
#endif
	/**
	 * 综上：bigfsm数组中每个元素的最高位（MSB：most significant bit）和第7位（从最低位开始数）构成以下了4种情况：
	 *   MSB     ：       BIT7
	 *    0      ：        0    正常的状态变迁
	 *    0      ：        1    这种情况不存在
	 *    1      ：        0    借机处理三字符序列，续行符，古日尔曼字母（相当于一个HOOK）
	 *    1      ：        1    非法状态（大于33且小于44） 或 接受状态（ACT包装的S_SELF或S_SELFB）
	 */
#ifdef DEBUG_FSM
	print_bigfsm();
	exit(1);
#endif
}

/**
 * fixlex - 关闭C++的单行注释功能
 */
void fixlex(void) {
	/* do C++ comments? */
	if ( Cplusplus == 0 ) /* 如果没有开启C++注释支持 */
		bigfsm['/'][COM1] = bigfsm['x'][COM1]; /* 那么只是简单的把C++注释当作除法运算符 */
}

/**
 * gettokens - 从输入源的缓冲区中得到一行字符，将其存储成Tokenrow格式保存在内存中
 * @trp: 保存返回的 token row
 * @reset: reset如果为非0，则表示输入缓冲可以被rewind
 * 返回值： TODO
 * NOTE: fill in a row of tokens from input, terminated by NL or END(遇到EOFC的状态)
 * First token is put at trp->lp.
 * Reset is non-zero when the input buffer can be "rewound."
 * The value is a flag indicating that possible macros have
 * been seen in the row.
 *
 * TODO: 注意：这个函数有bug，比如说，如果待编译的C源程序中有一行如果大于32KB的话，
 * gettokens()会在调用fillbuf函数时令程序当掉! 但一般来说没有这么变态的源程序...
 */
int gettokens(Tokenrow *trp, int reset) {
	register int c, state, oldstate;
	register uchar *ip;		/* 输入源缓冲区中当前正在处理的字符的首地址 */
	register Token *tp;		/* 指向当前要填写的Token结构，也称当前Token（Tokenrow的Token数组中最后一个有效Token的下一节点的首地址） */
	register Token *maxp;	/* Tokenrow中Token数组的总大小的下一元素的首地址,即： &trp->bp[trp->max] */
	int runelen;			/* 日尔曼字符长度，例如：非宽字符的日尔曼字符长度为1,宽字符的日尔曼字符长度为2 */
	Source *s = cursource;	/* 获得当前输入源栈顶节点的首地址 */
	int nmac = 0;
	extern char outbuf[];

	/**
	 * 得到Tokenrow的Token数组中最后一个有效Token的下一节点的首地址。
	 * （一般来说，如果Tokenrow中没有有效Token，那么tp其实就是Token数组中的第一个元素的首地址）
	 * 也可以说成是：得到将要被填入Tokenrow的Token首地址，也即：当前Token
	 */
	tp = trp->lp;
	ip = s->inp; /* 拿到输入源缓冲区中当前正在处理的字符的首地址 */
	if (reset) { /* 如果缓冲区可以被“重绕” */
		s->lineinc = 0; /* 为续行符做调整 */
		if (ip >= s->inl) { /* 如果输入源缓冲区中没有有效数据 */
			s->inl = s->inb; /* 设置input last+1为缓冲区的基地址（换句话说，即：置空缓冲区） */
			fillbuf(s); /* 从输入源加载数据到缓冲区中 */
			ip = s->inp = s->inb; /* 设置ip重新指向缓冲区的基地址 */
		} else if (ip >= s->inb+(3*INS/4)) { /* 如果缓冲区中有数据，但当前指针已经超过了输入源缓冲区大小的3/4,那么开始“重绕” */
			memmove(s->inb, ip, 4+s->inl-ip); /* 将缓冲区中还未扫描到的剩余有效数据复制到缓冲区开始 */
			s->inl = s->inb+(s->inl-ip); /* 重新设置缓冲区末尾指针 */
			ip = s->inp = s->inb; /* 重新设置缓冲区的当前指针使其指向缓冲区的基地址 */
		}
	}
	maxp = &trp->bp[trp->max]; /* 拿到Tokenrow中Token数组的总大小的下一元素的首地址 */
	runelen = 1; /* 设置日尔曼字符长度为1 */
	for (;;) {
	   continue2:
		if (tp >= maxp) { /* 如果Tokenrow已满 */
			trp->lp = tp; /* 保存当前token */
			tp = growtokenrow(trp); /* 增大Tokenrow的容量 */
			maxp = &trp->bp[trp->max]; /* 更新Tokenrow中Token数组的总大小的下一元素的首地址 */
		}
		tp->type = UNCLASS; /* 初始为未归类的token type */
		tp->hideset = 0; /* TODO: 什么是hideset？ */
		tp->t = ip; /* 指向当前Token的字符串首地址，注意：该字符串并不是以空字符结尾的，该字符串的长度是以tp->len标定的. */
		tp->wslen = 0; /* 空白符的长度 */
		tp->flag = 0;
		state = START;	/* 设置当前状态为START */
		for (;;) {
			oldstate = state; /* 将当前状态保存到oldstate中 */
			c = *ip; /* 从输入源缓冲区取出一个字符 */
			if ((state = bigfsm[c][state]) >= 0) { /* 如果迁移状态不为接受状态 */
				ip += runelen; /* ip指向缓冲区中下一字符 */
				runelen = 1;
				continue; /* 重新开始下一循环，但不 创建（或切换到下一Token结构）新的Token结构 */
			}
			state = ~state; /* 此时的state是接受状态,对其取反后还原为原来的ACT合成的 tokentype（高9位）+接受状态（低7位）的形式 */
		reswitch:
			switch (state&0177) { /* 从state中过滤出接受状态 */
			case S_SELF:
				ip += runelen; /* ip指向输入源缓冲中的下一字符 */
				runelen = 1; /* 日尔曼字符长度为1 */
			case S_SELFB:
				tp->type = GETACT(state); /* 保存tokentype */
				tp->len = ip - tp->t; /* 保存单词的字串长度 */
				tp++; /* tp指向下一Token结构 */
				goto continue2; /* 开始下一Token的识别 */

			case S_NAME:	/* like S_SELFB but with nmac check */
				tp->type = NAME; /* 设置当前Token的类型 */
				tp->len = ip - tp->t; /* 设置当前Token的字符长度 */
				nmac |= quicklook(tp->t[0], tp->len>1?tp->t[1]:0); /* TODO: 这是干嘛的？ */
				tp++; /* tp指向下一Token结构 */
				goto continue2; /* 开始下一Token的识别 */

			case S_WS: /* 空白符 */
				tp->wslen = ip - tp->t; /* 例如有一个序列： abc 123 ccc ，当扫描到abc后面的空格时，实际上这个空格的字串长度被填写到 `123' 这个单词的Token结构中去了。 */
				tp->t = ip; /* 指向该空白符之后的字符 */
				state = START; /* 重置状态为开始状态 */
				continue; /* 重新开始下一循环，但不 创建（或切换到下一Token结构）新的Token结构 */

			default: /* 其实这个有点类似于硬件的异常处理的错误恢复机制... */
				/**
				 * 如果state的第7位为0
				 * 第一次进入default分支时，state的QBSBIT(BIT7)位肯定为1,但越过if分支后马上该位被清零.
				 * 但如果判定是三字符序列或续行符,马上流程还会回到这里的if判断.所以我说：`其实这个有点类似于硬件的异常处理的错误恢复机制'...
				 */
				if ((state&QBSBIT)==0) { /* 如果state的QBSBIT位为0,说明此时已经过了异常处理阶段，开始恢复正常的状态变迁 */
					ip += runelen;
					runelen = 1;
					continue;
				}
				state &= ~QBSBIT; /* 对state的第7位清零 */
				s->inp = ip; /* 更新输入缓冲区的当前指针 */
				if (c == '?') { 	/* 检查三字符序列（check trigraph） */
					if (trigraph(s)) { /* 如果输入源中的当前序列是三字符序列 */
						state = oldstate; /* 将旧状态恢复 */
						continue; /* 重新开始下一循环，但不 创建（或切换到下一Token结构）新的Token结构 */
					}
					goto reswitch; /* 如果不是三字符序列，那么就是普通的状态变迁，跳转到reswitch即可 */
				}
				if (c=='\\') { /* line-folding,续行符 */
					if (foldline(s)) { /* 如果输入源中的当前序列是续行符 */
						s->lineinc++; /* 行号自加1（其实输入源的缓冲区是行缓冲区） */
						state = oldstate; /* 将旧状态恢复 */
						continue; /* 重新开始下一循环，但不 创建（或切换到下一Token结构）新的Token结构 */
					}
					goto reswitch; /* 如果不是三字符序列，那么就是普通的状态变迁，跳转到reswitch即可 */
				}
				error(WARNING, "Lexical botch in cpp"); /* 遇到了模糊不清的token序列... */
				ip += runelen;
				runelen = 1;
				continue;

			case S_EOB: /* 缓冲区结尾： 一般来说，很可能是遇到了一行的末尾（因为lcc目前是行缓冲，最多只能处理一行为INS大小的c源程序） */
				s->inp = ip; /* 设置输入源的当前指针为当前ip所指处，以便调用fillbuf */
				fillbuf(cursource); /* 重新状态缓冲区 */
				state = oldstate; /* 恢复旧状态 */
				continue; /* 重新开始下一循环，但不 创建（或切换到下一Token结构）新的Token结构 */

			case S_EOF: /* 遇到了EOFC */
				tp->type = END; /* token type 为 END */
				tp->len = 0;
				s->inp = ip;
				if (tp!=trp->bp && (tp-1)->type!=NL && cursource->fd!=NULL)
					error(WARNING,"No newline at end of file");
				trp->lp = tp+1;
				return nmac;

			case S_STNL: /* 字符串(或字符常量)中出现了换行符的状态(接受状态,非法状态) */
				error(ERROR, "Unterminated string or char const"); /* 打印错误消息 */
			case S_NL: /* 换行状态（遇到了换行符） */
				tp->t = ip;
				tp->type = NL;
				tp->len = 1;
				tp->wslen = 0;
				s->lineinc++; /* 行号加1 */
				s->inp = ip+1;
				trp->lp = tp+1;
				return nmac; /* 从gettokens函数返回 */

			case S_EOFSTR: /* 字符串(或字符常量)中出现了文件结束符的状态(非法状态) */
				error(FATAL, "EOF in string or char constant"); /* 打印错误信息，并结束进程 */
				break;

			case S_COMNL: /* C注释中出现换行符.NOTE: C注释是允许跨行的(接受状态) */
				s->lineinc++;
				state = COM2;
				ip += runelen;
				runelen = 1;
				if (ip >= s->inb+(7*INS/8)) { /* very long comment */
					memmove(tp->t, ip, 4+s->inl-ip);
					s->inl -= ip-tp->t;
					ip = tp->t+1;
				}
				continue; /* 重新开始下一循环，但不 创建（或切换到下一Token结构）新的Token结构 */

			case S_EOFCOM: /* C注释（或C++注释）中遇到了文件结束符后的状态(非法状态) */
				error(WARNING, "EOF inside comment");
				--ip;
			case S_COMMENT: /* C注释被接受的状态 */
				++ip;
				tp->t = ip;
				tp->t[-1] = ' ';
				tp->wslen = 1;
				state = START;
				continue; /* 重新开始下一循环，但不 创建（或切换到下一Token结构）新的Token结构 */
			}
			break;
		}
		ip += runelen;
		runelen = 1;
		tp->len = ip - tp->t;
		tp++;
	}
}

/**
 * trigraph - 判断输入源中碰到的当前序列是否是三字符序列
 * @s: 输入源
 * 返回值：返回0,说明不是三字符序列;不为0,返回值即为三字符序列所代表的字符值
 */
int trigraph(Source *s) {
	int c;

	while (s->inp+2 >= s->inl && fillbuf(s)!=EOF) /* 尽量确保缓冲区中,字符'?'之后的两个字符在缓冲区中 */
		;
	if (s->inp[1] != '?') /* 如果第一个'?'之后跟的不是'?' */
		return 0; /* 返回0（说明不是三字符序列） */
	c = 0; /* 如果序列是一个三字符序列，c将会被更新为三字符序列所代表的字符 */
	switch(s->inp[2]) {
	case '=':
		c = '#'; break; /* ??=代表# */
	case '(':
		c = '['; break; /* ??(代表[ */
	case '/':
		c = '\\'; break; /* ??/代表\ */
	case ')':
		c = ']'; break; /* ??)代表] */
	case '\'':
		c = '^'; break; /* ??'代表^ */
	case '<':
		c = '{'; break; /* ??<代表{ */
	case '!':
		c = '|'; break; /* ??!代表| */
	case '>':
		c = '}'; break; /* ??>代表} */
	case '-':
		c = '~'; break; /* ??-代表~ */
	}
	if (c) { /* 如果c不为0（说明序列是一个三字符序列） */
		*s->inp = c; /* 将三字符序列所代表的字符拷贝至第一个字符处 */
		memmove(s->inp+1, s->inp+3, s->inl-s->inp+2); /* 输入缓冲区的内容整体前移两个字符 */
		s->inl -= 2; /* inl指针前进两个字符 */
	}
	return c;
}

int foldline(Source *s) {
	while (s->inp+1 >= s->inl && fillbuf(s)!=EOF)
		;
	if (s->inp[1] == '\n') {
		memmove(s->inp, s->inp+2, s->inl-s->inp+3);
		s->inl -= 2;
		return 1;
	}
	return 0;
}

/**
 * fillbuf - 填充新数据到输入源s的缓冲区中
 * @s：输入源s
 * 返回值： 如果fillbuf发现输入源s中已经没有更多的新数据,那么返回EOF（-1）;否则返回0
 * 注意：每次最多填充缓冲区总容量的1/8到输入源的缓冲区中。
 * 什么时候才会调用fillbuf呢？答：只有当缓冲区中没有有效数据时（例如遇到EOB）才会调用此函数。
 * TODO： 如果输入源缓冲区中有有效数据时，调用该函数会发生什么情况呢？
 * 答曰：会继续往输入源的缓冲区中附加数据直至缓冲区被塞满，因此当输入源的缓冲区满的时候，就是cpp程序当掉的时候！
 * 换句话说lcc不能处理一行大于INS-INS/8的C源程序，而gcc在处理很大的C文件时是没有问题的.
 */
int fillbuf(Source *s) {
	int n, nr;

	nr = INS/8; /* 缓冲区大小的1/8 */
	/**
	 * TODO：下面这个判断只是针对文件输入源的缓冲区来说的，换句话说：lcc最多只能处理一行小于INS-INS/8个字节的C源程序.
	 * 如果我们写一个c源程序，令其中一行大于28K，那么cpp程序就会打印"Input buffer overflow"并当掉!
	 * 有兴趣可以看看：testprog目录下的 generate_any_size_c_source_file.c 程序.
	 * 如果命令行上的参数字串大于28K，那么这里就是一个bug！但实际上大多数操作系统配置（或shell配置）命令行字串不能大于4KB。
	 * 如果我们把INS设置为4096,然后重编译cpp，然后运行testprog目录下的optarg_test.sh
	 * 就会发现cpp程序会在if语句中当掉!
	 */
	if ((char *)s->inl+nr > (char *)s->inb+INS) /* 若缓冲区的可用空间小于缓冲区总容量的1/8 */
		error(FATAL, "Input buffer overflow"); /* 则打印错误信息，并结束进程。 */
	if (s->fd==NULL || (n=fread((char *)s->inl, 1, INS/8, s->fd)) <= 0) /* 读取INS/8个字节到缓冲区中（s->fd不为NULL时才调用fread） */
		n = 0; /* 如果输入源是字符串 或者 如果输入源是文件且从文件读取数据失败（fread返回值<=0）那么设置读取的字节数n为0 */
	if ((*s->inp&0xff) == EOB) /* 在输入中遇到了哨兵字符（sentinel character appears in input） */
		*s->inp = EOFC;
	s->inl += n;
	s->inl[0] = s->inl[1]= s->inl[2]= s->inl[3] = EOB;
	if (n == 0) {
		s->inl[0] = s->inl[1]= s->inl[2]= s->inl[3] = EOFC;
		return EOF; /* 输入源s中已经没有更多的新数据供填充到缓冲区，那么返回-1 */
	}
	return 0; /* 如果成功的往输入源缓冲区中填充了新数据，就返回0 */
}

/**
 * setsource - 在输入源栈的栈顶添加一个新的输入源节点
 * @name：输入源的文件名
 * @fd：输入源的文件指针
 * @str：预先往输入源中放置的待处理的字符串
 * 返回值： 返回新分配的输入源节点的首地址
 * 注意：
 * 如果fd等于NULL且str不为空，说明要压入的输入源只是一个缓冲区.
 * 如果fd不等于NULL且str为空，说明要压入的输入源是一个文件，文件名为name.
 */
Source * setsource(char *name, FILE *fd, char *str)
{
	Source *s = new(Source);
	int len; /* 如果str不为空，那么len为str的长度;否则，len为0 */

	s->line = 1; /* 设置当前行号为1 */
	s->lineinc = 0; /* 为续行符作调整 */
	s->fd = fd;	/* 设置source的文件指针 */
	s->filename = name; /* 设置文件的名字 */
	s->next = cursource; /* 头插法插入节点（此链表是一个栈） */
	s->ifdepth = 0; /* 条件编译指令的陷入深度 */
	cursource = s;  /* 设置当前源为新分配的输入源节点 */
	/* slop at right for EOB */
	if (str) { /* 如果str不为空（说明要压入的输入源只是一个缓冲区） */
		len = strlen(str); /* 计算该str的字符串长度 */
		s->inb = domalloc(len+4); /* 分配输入缓冲（之所以+4是因为，要在str后面放置4个哨兵字符） */
		s->inp = s->inb; /* 设置输入源缓冲区的当前指针为新分配空间的基地址 */
		strncpy((char *)s->inp, str, len); /* 将str复制到新分配的输入缓冲中去 */
	} else { /* 如果str为空（说明要压入的输入源是一个文件，文件名为name） */
		s->inb = domalloc(INS+4); /* 分配输入缓冲区 */
		s->inp = s->inb; /* 设置输入源缓冲区的当前指针为新分配空间的基地址 */
		len = 0; /* 设置len为0 */
	}
	s->inl = s->inp+len; /* 设置s->inl使其指向输入源缓冲区中最后一个有效字符的下一个字符 */
	s->inl[0] = s->inl[1] = EOB; /* 设置两个EOB（End Of Buffer）哨兵字符 */
	return s; /* 返回新分配的输入源节点首地址 */
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
