#include "c.h"
#undef yy
#define yy \
xx(alpha/osf,    alphaIR) \
xx(mips/irix,    mipsebIR) \
xx(sparc/sun,    sparcIR) \
xx(sparc/solaris,solarisIR) \
xx(x86/win32,    x86IR) \
xx(x86/linux,    x86linuxIR) \
xx(x86/ubuntu15.04-amd64,    x86linuxIR) \
xx(symbolic/osf, symbolic64IR) \
xx(symbolic/irix,symbolicIR) \
xx(symbolic,     symbolicIR) \
xx(bytecode,     bytecodeIR) \
xx(null,         nullIR)

#undef xx
#define xx(a,b) extern Interface b;
yy
/* 类型为Interface,名为x86linuxIR的结构体变量在文件x86linux.md中被定义 */
Binding bindings[] = {
#undef xx
#define xx(a,b) #a, &b,
yy
	NULL, NULL
};
#undef yy
#undef xx
