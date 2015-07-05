/* $Id$ */
#include <stdio.h>
#define	INS	32768		/* input buffer */
#define	OBS	4096		/* outbut buffer */
#define	NARG	32		/* Max number arguments to a macro */
#define	NINCLUDE 32		/* Max number of include directories (-I) */
#define	NIF	32		/* depth of nesting of #if */
#ifndef EOF
#define	EOF	(-1)
#endif
#ifndef NULL
#define NULL	0
#endif

#ifndef __alpha
typedef unsigned char uchar;
#endif

enum toktype { END, UNCLASS, NAME, NUMBER, STRING, CCON, NL, WS, DSHARP,
		EQ, NEQ, LEQ, GEQ, LSH, RSH, LAND, LOR, PPLUS, MMINUS,
		ARROW, SBRA, SKET, LP, RP, DOT, AND, STAR, PLUS, MINUS,
		TILDE, NOT, SLASH, PCT, LT, GT, CIRC, OR, QUEST,
		COLON, ASGN, COMMA, SHARP, SEMIC, CBRA, CKET,
		ASPLUS, ASMINUS, ASSTAR, ASSLASH, ASPCT, ASCIRC, ASLSH,
		ASRSH, ASOR, ASAND, ELLIPS,
		DSHARP1, NAME1, DEFINED, UMINUS };

enum kwtype { KIF, KIFDEF, KIFNDEF, KELIF, KELSE, KENDIF, KINCLUDE, KDEFINE,
		KUNDEF, KLINE, KERROR, KPRAGMA, KDEFINED,
		KLINENO, KFILE, KDATE, KTIME, KSTDC, KEVAL };

#define	ISDEFINED	01	/* has #defined value */
#define	ISKW		02	/* is PP keyword */
#define	ISUNCHANGE	04	/* can't be #defined in PP */
#define	ISMAC		010	/* builtin macro, e.g. __LINE__ */

#define	EOB	0xFE		/* sentinel for end of input buffer */
#define	EOFC	0xFD		/* sentinel for end of input file */
#define	XPWS	1		/* token flag: white space to assure token sep. */

/**
 * 描述一个token的结构
 */
typedef struct token {
	unsigned char	type;		/* token的类型 */
	unsigned char 	flag;		/* token的flag */
	unsigned short	hideset;	/* TODO: 隐藏集合? */
	unsigned int	wslen;		/* TODO: white space length? */
	unsigned int	len;		/* TODO: tokend的长度? */
	uchar	*t;					/* TODO: 干嘛的? */
} Token;

typedef struct tokenrow {
	Token	*tp;		/* 当前正被扫描的token(current one to scan) */
	Token	*bp;		/* base (allocated value) */
	Token	*lp;		/* last+1 token used */
	int	max;		/* number allocated */
} Tokenrow;

/**
 * 描述一个输入的源文件
 */
typedef struct source {
	char	*filename;		/* 源文件的文件名 */
	int	line;				/* 当前的行号 */
	int	lineinc;			/* TODO: adjustment for \\n lines */
	uchar	*inb;			/* 输入缓冲区 */
	uchar	*inp;			/* 输入指针 */
	uchar	*inl;			/* TODO: 输入结束? end of input */
	FILE*	fd;				/* 输入源文件的FILE指针(input source) */
	int	ifdepth;			/* conditional nesting in include */
	struct	source *next;	/* stack for #include */
} Source;

typedef struct nlist {
	struct nlist *next;
	uchar	*name;
	int	len;
	Tokenrow *vp;		/* value as macro */
	Tokenrow *ap;		/* list of argument names, if any */
	char	val;		/* value as preprocessor name */
	char	flag;		/* is defined, is pp name */
} Nlist;

typedef	struct	includelist {
	char	deleted;
	char	always;
	char	*file;
} Includelist;

#define	new(t)	(t *)domalloc(sizeof(t))
#define	quicklook(a,b)	(namebit[(a)&077] & (1<<((b)&037)))
#define	quickset(a,b)	namebit[(a)&077] |= (1<<((b)&037))
extern	unsigned long namebit[077+1];

enum errtype { WARNING, ERROR, FATAL }; /* 错误级别 */

void	expandlex(void);
void	fixlex(void);
void	setup(int, char **);
int	gettokens(Tokenrow *, int);
int	comparetokens(Tokenrow *, Tokenrow *);
Source	*setsource(char *, FILE *, char *);
void	unsetsource(void);
void	puttokens(Tokenrow *);
void	process(Tokenrow *);
void	*domalloc(int);
void	dofree(void *);
void	error(enum errtype, char *, ...);
void	flushout(void);
int	fillbuf(Source *);
int	trigraph(Source *);
int	foldline(Source *);
Nlist	*lookup(Token *, int);
void	control(Tokenrow *);
void	dodefine(Tokenrow *);
void	doadefine(Tokenrow *, int);
void	doinclude(Tokenrow *);
void	doif(Tokenrow *, enum kwtype);
void	expand(Tokenrow *, Nlist *);
void	builtin(Tokenrow *, int);
int	gatherargs(Tokenrow *, Tokenrow **, int *);
void	substargs(Nlist *, Tokenrow *, Tokenrow **);
void	expandrow(Tokenrow *, char *);
void	maketokenrow(int, Tokenrow *);
Tokenrow *copytokenrow(Tokenrow *, Tokenrow *);
Token	*growtokenrow(Tokenrow *);
Tokenrow *normtokenrow(Tokenrow *);
void	adjustrow(Tokenrow *, int);
void	movetokenrow(Tokenrow *, Tokenrow *);
void	insertrow(Tokenrow *, int, Tokenrow *);
void	peektokens(Tokenrow *, char *);
void	doconcat(Tokenrow *);
Tokenrow *stringify(Tokenrow *);
int	lookuparg(Nlist *, Token *);
long	eval(Tokenrow *, int);
void	genline(void);
void	setempty(Tokenrow *);
void	makespace(Tokenrow *);
char	*outnum(char *, int);
int	digit(int);
uchar	*newstring(uchar *, int, int);
int	checkhideset(int, Nlist *);
void	prhideset(int);
int	newhideset(int, Nlist *);
int	unionhideset(int, int);
void	iniths(void);
void	setobjname(char *);
#define	rowlen(tokrow)	((tokrow)->lp - (tokrow)->bp)

extern	char *outp;
extern	Token	nltoken;
extern	Source *cursource;
extern	char *curtime;
extern	int incdepth;
extern	int ifdepth;
extern	int ifsatisfied[NIF];
extern	int Mflag;
extern	int skipping;
extern	int verbose;
extern	int Cplusplus;
extern	Nlist *kwdefined;
extern	Includelist includelist[NINCLUDE];
extern	char wd[];
