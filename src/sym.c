#include "c.h"
#include <stdio.h>

static char rcsid[] = "$Id$";

#define equalp(x) v.x == p->sym.u.c.v.x

/**
 * 符号表结构.
 */
struct table {
	int level; /* 指明本符号表的嵌套级别 */
	Table previous; /* 指向外层符号表 */
	struct entry {
		struct symbol sym; /* 符号结构 */
		struct entry *link; /* 指向下一个struct entry,单链表,最后一个元素的link域为NULL */
	} *buckets[256]; /* hash桶,每个指针指向一个Hash链 */
	/* 指向由当前及其外层作用域中所有符号组成的列表的头(该列表是通过symbol的up域连接起来的) */
	/* e.g.: 1 <- 2 <- 3 <- all */
	Symbol all;
};

/**
 * 一个符号表中的散列表的大小.
 */
#define HASHSIZE NELEMS(((Table)0)->buckets)

static struct table
	cns = { CONSTANTS },
	ext = { GLOBAL },
	ids = { GLOBAL },
	tys = { GLOBAL };

Table constants   = &cns; /* 指向常量符号表(CONSTANTS),常量有: 整型常量,浮点常量,字符串常量. */
Table externals   = &ext; /* 指向声明为extern的标识符的符号表(用于警告外部标识符冲突,GLOBAL) */
Table identifiers = &ids; /* 指向当前嵌套级别的标识符符号表 */
Table globals     = &ids; /* 指向具有文件作用域的标识符的符号表(GLOBAL) */
Table types       = &tys; /* 指向当前类型符号表(GLOBALS) */
Table labels; /* 指向编译器定义的内部标号的符号表(虽然Table结构有层次,但labels表只在函数中使用,退出函数即删除,所以labels表没有层次结构) */
int level = GLOBAL; /* 当前的level级别  */
static int tempid;
List loci, symbols;


Table newtable(int arena) {
	Table new;

	NEW0(new, arena);
	return new;
}

/**
 * table - 在tp符号表上创建一个子符号表(内嵌符号表)
 * @tp: 要在哪一个符号表上构建子符号表
 * @level: 内嵌符号表的级别
 * 返回值: 新的符号表首地址
 */
Table table(Table tp, int level) {
	Table new = newtable(FUNC);
	new->previous = tp;
	new->level = level;
	if (tp)
		new->all = tp->all;
	return new;
}

/**
 * 从符号表tp的lev级里,对每个符号依次调用apply函数
 * @tp: 符号表
 * @lev: 符号可见性级别
 * @apply: 符号处理函数
 * @cl: 传递给符号处理函数apply的第二个参数
 * 返回值: 无
 */
void foreach(Table tp, int lev, void (*apply)(Symbol, void *), void *cl) {
	assert(tp);
	/* 确保退出while循环后 tp->level == lev */
	while (tp && tp->level > lev)
		tp = tp->previous;
	if (tp && tp->level == lev) {
		Symbol p;
		Coordinate sav;
		sav = src; // 先将当前的编译坐标保存到sav里
		for (p = tp->all; p && p->scope == lev; p = p->up) {
			src = p->src;
			(*apply)(p, cl);
		}
		src = sav; // 恢复当前的编译坐标
	}
}

/**
 * enterscope - 进入新的标号可见性级别
 * 返回值: 无
 */
void enterscope(void) {
	if (++level == LOCAL)
		tempid = 0;
}

/**
 * 退出作用域时level将递减,相应的identifiers和types表也随之撤销
 */
void exitscope(void) {
	rmtypes(level); // 删除类型表中当前level的所有类型
	/* 将类型符号表types上移一层 */
	if (types->level == level)
		types = types->previous;
	/* 撤销当前的标识符表使其指向更外一层 */
	if (identifiers->level == level) {
		if (Aflag >= 2) { /* 如果警告标志大于2 */
			int n = 0;
			Symbol p;
			for (p = identifiers->all; p && p->scope == level; p = p->up)
				if (++n > 127) {
					/* 警告在一个代码块内声明了过多的符号. */
					warning("more than 127 identifiers declared in a block\n");
					break;
				}
		}
		identifiers = identifiers->previous; // 标识符表上移一层
	}
	assert(level >= GLOBAL);
	--level;
}

/**
 * install - 为给定的name分配一个符号结构,并把该符号结构加入到与给定作用域层数相对应的表中.
 * @name: 符号名
 * @tpp: 要将符号名安装到的符号表
 * @level: 符号作用域级别
 * @arena: 内存区
 * 返回值: 新创建的Symbol指针.
 * PS: level==0:表示的是name应当建立在*tpp指向的当前级别的符号表中
 *     level >= tp->level : 表示的是新插入的符号作用域位于更内层(数字更大)
 */
Symbol install(const char *name, Table *tpp, int level, int arena) {
	Table tp = *tpp;
	struct entry *p;
	/* 以这个字符串的首地址做散列值 */
	unsigned h = (unsigned long)name&(HASHSIZE-1);

	/* level == 0 : 表示的是name应当建立在*tpp表示的表格中
	 * level >= tp->level : 表示的是新插入的符号作用域位于更内层(数字更大)
	 */
	assert(level == 0 || level >= tp->level);

	/* 如果新插入符号的作用域级别更小(数字更大),则创建一个内层符号表 */
	if (level > 0 && tp->level < level)
		tp = *tpp = table(tp, level);
	NEW0(p, arena);
	p->sym.name = (char *)name;
	p->sym.scope = level; // TODO: BUG 当时我没看明白..不一定是bug
	p->sym.up = tp->all;
	tp->all = &p->sym;
	p->link = tp->buckets[h];
	tp->buckets[h] = p;
	return &p->sym;
}
Symbol relocate(const char *name, Table src, Table dst) {
	struct entry *p, **q;
	Symbol *r;
	unsigned h = (unsigned long)name&(HASHSIZE-1);

	for (q = &src->buckets[h]; *q; q = &(*q)->link)
		if (name == (*q)->sym.name)
			break;
	assert(*q);
	/*
	 Remove the entry from src's hash chain
	  and from its list of all symbols.
	*/
	p = *q;
	*q = (*q)->link;
	for (r = &src->all; *r && *r != &p->sym; r = &(*r)->up)
		;
	assert(*r == &p->sym);
	*r = p->sym.up;
	/*
	 Insert the entry into dst's hash chain
	  and into its list of all symbols.
	  Return the symbol-table entry.
	*/
	p->link = dst->buckets[h];
	dst->buckets[h] = p;
	p->sym.up = dst->all;
	dst->all = &p->sym;
	return &p->sym;
}

/**
 * (逐层向外)查找一个符号是否在符号表里
 * @name: 一个符号
 * @tp: 符号表
 * 返回值: 在,则返回此符号的Symbol结构指针,不在,则返回NULL
 */
Symbol lookup(const char *name, Table tp) {
	struct entry *p;
	/* 以这个字符串的首地址做散列值 */
	unsigned h = (unsigned long)name&(HASHSIZE-1);

	assert(tp);
	do
		for (p = tp->buckets[h]; p; p = p->link)
			if (name == p->sym.name) // 比较两个字符串的首地址
				return &p->sym;
	while ((tp = tp->previous) != NULL);
	return NULL;
}

/**
 * genlabel - 产生一个标号.
 * @n: 一个标号递增数字
 * 返回值: 一个数字标号
 */
int genlabel(int n) {
	static int label = 1;

	label += n;
	return label - n;
}

/**
 * findlabel - 找寻一个label的符号结构,如果需要,则会建立该符号结构.
 * @lab: label的标号
 * 返回值: 返回label的符号结构
 */
Symbol findlabel(int lab) {
	struct entry *p;
	unsigned h = lab&(HASHSIZE-1);

	/* 在哈希链表里查找标号 */
	for (p = labels->buckets[h]; p; p = p->link)
		if (lab == p->sym.u.l.label)
			return &p->sym;
	/* 如果没找到,则新创建一个entry结构  */
	NEW0(p, FUNC);
	p->sym.name = stringd(lab);
	p->sym.scope = LABELS;
	p->sym.up = labels->all;
	labels->all = &p->sym;

	/* 头插法插入链表元素 */
	p->link = labels->buckets[h];
	labels->buckets[h] = p;
	p->sym.generated = 1; // 表示在编译器内部产生的符号
	p->sym.u.l.label = lab; // 保存标号的值
	(*IR->defsymbol)(&p->sym); /* 通知编译器后端 */
	return &p->sym;
}

/**
 * constant - 在常量表中添加类型为ty值为v的常量(相同的常量在常量表里是唯一的)
 * @ty: 常量的类型
 * @v: 常量的值
 * 返回值: 返回一个指向符号结构的指针
 */
Symbol constant(Type ty, Value v) {
	struct entry *p;
	unsigned h = v.u&(HASHSIZE-1);
	static union { int x; char endian; } little = { 1 };

	ty = unqual(ty);
	for (p = constants->buckets[h]; p; p = p->link)
		if (eqtype(ty, p->sym.type, 1))
			switch (ty->op) {
			case INT:      if (equalp(i)) return &p->sym; break;
			case UNSIGNED: if (equalp(u)) return &p->sym; break;
			case FLOAT:
				if (v.d == 0.0) {
					float z1 = v.d, z2 = p->sym.u.c.v.d;
					char *b1 = (char *)&z1, *b2 = (char *)&z2;
					if (z1 == z2
					&& (!little.endian && b1[0] == b2[0]
					||   little.endian && b1[sizeof (z1)-1] == b2[sizeof (z2)-1]))
						return &p->sym;
				} else if (equalp(d))
					return &p->sym;
				break;
			case FUNCTION: if (equalp(g)) return &p->sym; break;
			case ARRAY:
			case POINTER:  if (equalp(p)) return &p->sym; break;
			default: assert(0); // 不可达路径
			}
	/* 如果没找到,则新创建一个entry结构  */
	NEW0(p, PERM);
	p->sym.name = vtoa(ty, v); // value to string
	p->sym.scope = CONSTANTS;
	p->sym.type = ty;
	p->sym.sclass = STATIC;
	p->sym.u.c.v = v;
	p->link = constants->buckets[h];
	p->sym.up = constants->all;
	constants->all = &p->sym;
	constants->buckets[h] = p;
	if (ty->u.sym && !ty->u.sym->addressed)
		(*IR->defsymbol)(&p->sym);
	p->sym.defined = 1;
	return &p->sym;
}

/**
 * intconst - 在constants符号表中添加一个整型常量
 * 封装了建立和通知整型常量的功能.
 * @n: 要添加的整型常量
 * 返回值: 常量在contants表中的Symbol结构.
 */
Symbol intconst(int n) {
	Value v;

	v.i = n; // v.i 是有符号整型
	return constant(inttype, v);
}

/**
 * 根据给定的存储类别,类型,作用域,产生一个内部标识符并初始化.
 * scls: 存储类别
 * ty: 类型
 * lev: 作用域
 * 返回值: 返回新创建的标识符的Symbol指针.
 */
Symbol genident(int scls, Type ty, int lev) {
	Symbol p;

	NEW0(p, lev >= LOCAL ? FUNC : PERM);
	p->name = stringd(genlabel(1));
	p->scope = lev;
	p->sclass = scls;
	p->type = ty;
	p->generated = 1; /* 表明是编译器产生的符号 */
	if (lev == GLOBAL)
		(*IR->defsymbol)(p); /* 通知编译后端产生了全局变量 */
	return p;
}

/**
 * 产生一个临时变量,并返回其Symbol指针.
 * scls: 存储类别
 * ty: 类型
 * lev: 作用域
 * 返回值: 返回新创建的标识符的Symbol指针.
 */
Symbol temporary(int scls, Type ty) {
	Symbol p;

	NEW0(p, FUNC);
	p->name = stringd(++tempid);
	p->scope = level < LOCAL ? LOCAL : level;
	p->sclass = scls;
	p->type = ty;
	p->temporary = 1; /* 表明是编译器产生的内部临时变量 */
	p->generated = 1;
	return p;
}

/**
 * newtemp - 仅供编译后端调用的用来产生临时变量的函数
 * @sclass: 临时变量的存储类别
 * @tc: 类型后缀
 * 返回值: 新产生的临时变量的符号结构指针
 */
Symbol newtemp(int sclass, int tc, int size) {
	Symbol p = temporary(sclass, btot(tc, size));

	(*IR->local)(p);
	p->defined = 1; /* 定义了 */
	return p;
}

Symbol allsymbols(Table tp) {
	return tp->all;
}

void locus(Table tp, Coordinate *cp) {
	loci    = append(cp, loci);
	symbols = append(allsymbols(tp), symbols);
}

void use(Symbol p, Coordinate src) {
	Coordinate *cp;

	NEW(cp, PERM);
	*cp = src;
	p->uses = append(cp, p->uses);
}

/**
 * findtype - 在标识符表里查找用typedef关键字定义的类型ty
 * @ty: 类型
 * 返回值: 该类型的符号结构的首地址
 */
Symbol findtype(Type ty) {
	Table tp = identifiers;
	int i;
	struct entry *p;

	assert(tp);
	do
		for (i = 0; i < HASHSIZE; i++)
			for (p = tp->buckets[i]; p; p = p->link)
				if (p->sym.type == ty && p->sym.sclass == TYPEDEF)
					return &p->sym;
	while ((tp = tp->previous) != NULL);
	return NULL;
}

/**
 * mkstr - 将一个字符串常量添加到常量表里
 * @str: 字符串
 * 返回值: 符号结构的首地址.
 */
Symbol mkstr(char *str) {
	Value v;
	Symbol p;

	v.p = str;
	p = constant(array(chartype, strlen(v.p) + 1, 0), v);
	if (p->u.c.loc == NULL)
		p->u.c.loc = genident(STATIC, p->type, GLOBAL);
	return p;
}

/**
 * mksymbol - 为name产生一个符号结构,如果sclass等于EXTERN则将name加入global符号表
 * @sclass: 存储级别
 * @name: 名字
 * @ty: 类型
 * 返回值: 符号结构指针
 */
Symbol mksymbol(int sclass, const char *name, Type ty) {
	Symbol p;

	if (sclass == EXTERN)
		p = install(string(name), &globals, GLOBAL, PERM);
	else {
		NEW0(p, PERM);
		p->name = string(name);
		p->scope = GLOBAL;
	}
	p->sclass = sclass;
	p->type = ty;
	(*IR->defsymbol)(p); /* 通知后端来定义符号 */
	p->defined = 1;
	return p;
}

/**
 * vtoa - 返回类型为ty,值为v的常量的字符串形式
 * @ty: 类型
 * @v:  值
 * 返回值: 常量的字符串表示
 */
char *vtoa(Type ty, Value v) {
	char buf[50];

	ty = unqual(ty); // 去掉类型限定符(const或volatile)
	switch (ty->op) {
	case INT:      return stringd(v.i);
	case UNSIGNED: return stringf((v.u&~0x7FFF) ? "0x%X" : "%U", v.u);
	case FLOAT:    return stringf("%g", (double)v.d);
	case ARRAY:
		if (ty->type == chartype || ty->type == signedchar
		||  ty->type == unsignedchar)
			return v.p;
		return stringf("%p", v.p);
	case POINTER:  return stringf("%p", v.p);
	case FUNCTION: return stringf("%p", v.g);
	}
	assert(0); return NULL;
}
