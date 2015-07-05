/* x86s running Linux */

#include <string.h>

static char rcsid[] = "$Id$";

/* 程序的默认安装位置 */
#ifndef LCCDIR
#define LCCDIR "/usr/local/lib/lcc/"
#endif

char *suffixes[] = { ".c", ".i", ".s", ".o", ".out", 0 };

/* 如果环境变量LCCINPUTS被设定了.那么lcc假定这个变量里存放的是冒号或分号分隔的一组目录列表.
 * lcc将在这些目录中查找,那些命令行上没有被给出绝对路径的源文件和目标文件.同时LCCINPUTS中给出的路径也被添加到库文件搜索路径中.
 * 如果LCCINPUTS被指定了,那么他必须包含 `.' ，目的是使当前目录被添加到搜索路径中去.
 * 说白了,相当于PATH变量，来找库文件用的.这个参数类似于gcc里的 -L 选项。
 * 比如我们在gcc命令行上指明了要用libz.so库，实际的命令行为 -lz.
 * 这时候gcc会在-L选项指明的目录列表里依次查找名为libz.so的库，并最终将库链接入可执行文件中去.
 */
char inputs[256] = "";

/*
 * 这个cpp数组指明了lcc所需的预处理器的路径和参数. 例如: "-D__STRICT_ANSI__"参数将被传递给cpp程序.
 * 这些参数中有一些模板.其中:
 * $1将被替换成用户在命令行上所指定的选项,对于预处理器,这个列表中始终包含 -D__LCC__ 选项;
 * $2将被替换成输入文件; $3将被替换成输出文件.
 */
//   /usr/bin/cpp -U__GNUC__ -D_POSIX_SOURCE -D__STRICT_ANSI__ -Dunix -Di386 -Dlinux -D__unix__ -D__i386__ -D__linux__ -D__signed__=signed -v -P -imultilib 32 -m32 -mtune=generic -march=i686 hello.c hello.i
char *cpp[] = { "/usr/bin/cpp",
	"-U__GNUC__", "-D_POSIX_SOURCE", "-D__STRICT_ANSI__",
	"-Dunix", "-Di386", "-Dlinux",
	"-D__unix__", "-D__i386__", "-D__linux__", "-D__signed__=signed",
	"-imultilib 32","-m32","-mtune=generic","-march=i686",
	"$1", "$2", "$3", 0 };

/*
 * The include array is a list of -I options that specify which directives should be searched
 * to satisfy include directives. These directories are searched in the order given.
 * The first directory should be the one to which the ANSI header files were copied
 * as described in UNIX or Windows installation instructions. The driver adds these options
 * to cpp's arguments when it invokes the preprocessor, except when -N is specified.
 */
// /usr/lib/gcc/x86_64-linux-gnu/4.9/include
//  /usr/lib/gcc/x86_64-linux-gnu/4.9/include
// /usr/local/include
//  /usr/lib/gcc/x86_64-linux-gnu/4.9/include-fixed

//  /usr/include

char *include[] = {"-I" LCCDIR "include",
"-I/usr/lib/gcc/x86_64-linux-gnu/4.9/include" ,
"-I/usr/local/include","-I/usr/lib/gcc/x86_64-linux-gnu/4.9/include-fixed",
 "-I/usr/include", 0 };

/* 调用编译器rcc的命令行
 * $1 选项flag(例如: -v)
 * $2 .i源文件(输入文件)
 * $3 .s汇编文件(输出文件)
 */
char *com[] = {LCCDIR "rcc", "-target=x86/linux", "$1", "$2", "$3", 0 };

/* 当前系统的汇编命令
 * $1 as的选项
 * $2 输入的汇编源文件
 * $3 输出的目标文件(.o)
 */
char *as[] = { "/usr/bin/as", "--32", "-o", "$3", "$1", "$2", 0 };

/* 调用ld的命令
 * $1 库文件搜索路径列表
 * $2 所有的 ".o" 文件和库文件的列表
 * $3 是 a.out, 除非 -o 选项被指定
 */
char *ld[] = {
	/*  0 */ "/usr/bin/ld", "-m", "elf_i386", "-dynamic-linker",
	/*  4 */ "/lib/ld-linux.so.2", "-o", "$3",
	/*  7 */ "/usr/lib32/crt1.o", "/usr/lib32/crti.o",
	/*  9 */ "/usr/lib/gcc/x86_64-linux-gnu/4.9/32/crtbegin.o", 
                 "$1", "$2",
	/* 12 */ "-L" LCCDIR,
	/* 13 */ "-llcc",
"-L/usr/lib/gcc/x86_64-linux-gnu/4.9/32",
"-L/usr/lib/gcc/x86_64-linux-gnu/4.9/../../../i386-linux-gnu",
"-L/usr/lib/gcc/x86_64-linux-gnu/4.9/../../../../lib32 -L/lib/../lib32",
"-L/usr/lib/i386-linux-gnu",
"-L/usr/lib/../lib32",
"-L/usr/lib/gcc/x86_64-linux-gnu/4.9",
"-L/usr/lib/gcc/x86_64-linux-gnu/4.9/../../../i386-linux-gnu",
"-L/usr/lib/gcc/x86_64-linux-gnu/4.9/../../.. -L/usr/lib/i386-linux-gnu", 
		"-lgcc","-lc", "-lm",
	/* 18 */ "",
	/* 19 */ "/usr/lib/gcc/x86_64-linux-gnu/4.9/32/crtend.o", 
"/usr/lib/gcc/x86_64-linux-gnu/4.9/../../../../lib32/crtn.o",
	0 };

extern char *concat(char *, char *);
#include <stdio.h>
int option(char *arg) {
	/* 如果命令行上指明了lcc的安装目录，则用此目录覆盖掉默认的lcc安装位置目录 */
  	if (strncmp(arg, "-lccdir=", 8) == 0) {
		cpp[0] = concat(&arg[8], "/gcc/cpp"); /* TODO: 如果make triple出错,把该行注释掉,改成下一行 */
		//cpp[0] = concat("/usr/bin/", "cpp");
		include[0] = concat("-I", concat(&arg[8], "/include"));
		include[1] = concat("-I", concat(&arg[8], "/gcc/include"));
		ld[9]  = concat(&arg[8], "/gcc/crtbegin.o");
		ld[12] = concat("-L", &arg[8]);
		ld[14] = concat("-L", concat(&arg[8], "/gcc"));
		ld[19] = concat(&arg[8], "/gcc/crtend.o");
		com[0] = concat(&arg[8], "/rcc");
	} else if (strcmp(arg, "-p") == 0 || strcmp(arg, "-pg") == 0) {
		return 0; /* TODO: 程序性能剖析?但在我的ub12机器上找不到libgmon库... so...目前暂时假定不支持此选项 */
		ld[7] = "/usr/lib/i386-linux-gnu/gcrt1.o";
		ld[18] = "-lgmon";
	} else if (strcmp(arg, "-b") == 0) /*	Produce code that counts the number of times each expression is executed.
											If loading takes place, arrange for a prof.out file to be written when the object
											program terminates. A listing annotated with execution counts can then be generated
											with bprint(1). lcc warns when -b is unsupported. -Wf-C is similar, but counts only
											the number of function calls. */
		;
	else if (strcmp(arg, "-g") == 0) /* -g Produce additional symbol table information for the local debuggers.
	 	 	 	 	 	 	 	 	 	 Produce debugging information in the operating system's native format
	 	 	 	 	 	 	 	 	 	 (stabs, COFF, XCOFF, or DWARF 2).  GDB can work with this debugging information.
	 	 	 	 	 	 	 	 	 	 -glevel:
	 	 	 	 	 	 	 	 	   */
		;
	else if (strncmp(arg, "-ld=", 4) == 0)
		ld[0] = &arg[4];
	else if (strcmp(arg, "-static") == 0) {
		return 0; /* TODO: 目前暂时假定不支持此选项 */
		ld[3] = "-static";
		ld[4] = "";
	} else
		return 0; /* option()函数返回0表明,此选项不被系统支持 */
	return 1; /* option()函数返回1表明,此选项被系统支持 */
}
