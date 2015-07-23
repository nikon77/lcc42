#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cpp.h"

Includelist includelist[NINCLUDE]; /* 头文件包含目录列表(数组) */

extern char *objname;

void doinclude(Tokenrow *trp) {
	char fname[256], iname[256];
	Includelist *ip;
	int angled, len, i;
	FILE *fd;

	trp->tp += 1;
	if (trp->tp >= trp->lp)
		goto syntax;
	if (trp->tp->type != STRING && trp->tp->type != LT) {
		len = trp->tp - trp->bp;
		expandrow(trp, "<include>");
		trp->tp = trp->bp + len;
	}
	if (trp->tp->type == STRING) {
		len = trp->tp->len - 2;
		if (len > sizeof(fname) - 1)
			len = sizeof(fname) - 1;
		strncpy(fname, (char*) trp->tp->t + 1, len);
		angled = 0;
	} else if (trp->tp->type == LT) {
		len = 0;
		trp->tp++;
		while (trp->tp->type != GT) {
			if (trp->tp > trp->lp || len + trp->tp->len + 2 >= sizeof(fname))
				goto syntax;
			strncpy(fname + len, (char*) trp->tp->t, trp->tp->len);
			len += trp->tp->len;
			trp->tp++;
		}
		angled = 1;
	} else
		goto syntax;
	trp->tp += 2;
	if (trp->tp < trp->lp || len == 0)
		goto syntax;
	fname[len] = '\0';
	if (fname[0] == '/') {
		fd = fopen(fname, "r");
		strcpy(iname, fname);
	} else
		for (fd = NULL, i = NINCLUDE - 1; i >= 0; i--) {
			ip = &includelist[i];
			if (ip->file == NULL || ip->deleted || (angled && ip->always == 0))
				continue;
			if (strlen(fname) + strlen(ip->file) + 2 > sizeof(iname))
				continue;
			strcpy(iname, ip->file);
			strcat(iname, "/");
			strcat(iname, fname);
			if ((fd = fopen(iname, "r")) != NULL)
				break;
		}
	if (Mflag > 1 || !angled && Mflag == 1) {
		fwrite(objname, 1, strlen(objname), stdout);
		fwrite(iname, 1, strlen(iname), stdout);
		fwrite("\n", 1, 1, stdout);
	}
	if (fd != NULL) {
		if (++incdepth > 10)
			error(FATAL, "#include too deeply nested");
		setsource((char*) newstring((uchar*) iname, strlen(iname), 0), fd,
				NULL);
		genline();
	} else {
		trp->tp = trp->bp + 2;
		error(ERROR, "Could not find include file %r", trp);
	}
	return;
	syntax: error(ERROR, "Syntax error in #include");
	return;
}

/**
 * 为当前输入源产生一个行控制指令信息
 * Generate a line directive for cursource
 * TODO: 一般来说像gcc或者tcc之类的编译器产生的行控制指令是类似如下这样的：
 * # 3 "cpp_line_control.c" 2
 * 而像lcc这样的编译器产生的行控制指令是类似如下这样的：
 * #line 2 "cpp_line_control.c"
 * 也就是说lcc产生的行控制信息中多了一个 line （而这其实是可选的单词）而且也少了flag信息，具体请参考testprog/c预处理小知识.txt一文
 */
void genline(void) {
	static Token ta = { UNCLASS };
	static Tokenrow tr = { &ta, &ta, &ta + 1, 1 };
	uchar *p;

	ta.t = p = (uchar*) outp;
	strcpy((char*) p, "#line "); /* 将“#line ”复制到输出缓冲区 */
	p += sizeof("#line ") - 1;
	p = (uchar*) outnum((char*) p, cursource->line); /* 输出当前文件的行号 */
	*p++ = ' ';
	*p++ = '"';
	strcpy((char*) p, cursource->filename); /* 输出当前文件的文件名 */
	p += strlen((char*) p);
	*p++ = '"';
	*p++ = '\n';
	ta.len = (char*) p - outp;
	outp = (char*) p; /* 更新输出缓冲区的当前指针 */
	tr.tp = tr.bp; /* 将当前token设置为token row中的第一个token的首地址,以便接下来调用puttokens函数输出token row */
	puttokens(&tr); /* 输出 token row */
}

/**
 * setobjname - 设定目标文件名（xx.c -> xx.o）
 * @f: c源程序名
 * "xx.o" 存储在全局变量objname中
 */
void setobjname(char *f) {
	int n = strlen(f);
	objname = (char*) domalloc(n + 5);
	strcpy(objname, f);
	if (objname[n - 2] == '.') { /* 如果是filename.c */
		strcpy(objname + n - 1, "o: "); /* 则变成filename.o */
	} else { /* 如果是filename(没有扩展名...) */
		strcpy(objname + n, "obj: ");
	}
}
