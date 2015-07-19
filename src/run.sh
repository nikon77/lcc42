#!/bin/sh
# run .../target/os/tst/foo.s [ remotehost ]

# set -x

# target为编译器的目标编译架构，例如在在我们这里就是: x86
target=`echo $1 | awk -F/ '{ print $(NF-3) }'`

# os为编译器的宿主，在我们这里就是: ubuntu15.04-amd64
os=`echo $1 | awk -F/ '{ print $(NF-2) }'`

# dir为要测试的C源程序所在的目录
dir=$target/$os

# idir目录为lcc编译器要用到的一些专用头文件(浮点数,可变参数,assert宏相关...)
# remotehost在我们的例子中用不到...
case "$1" in
*symbolic/irix*)	idir=include/mips/irix; remotehost=noexecute ;;
*symbolic/osf*)		idir=include/alpha/osf;	remotehost=noexecute ;;
*x86/ubuntu15*)		idir=include/x86/ubuntu15.04-amd64;	remotehost=noexecute ;;
*)			idir=include/$dir;      remotehost=${2-$REMOTEHOST} ;;
esac

# -o 的意思是or
# $target/$os 中包含了编译测试C源程序提前生成的正确的汇编文件(扩展名为sbk)
# 我们测试编译器的正确性时首先要对C源程序生成.s汇编文件,然后将我们自己生成
# 的.s汇编文件和这个.sbk文件进行对比。如果两者一致，就“基本”说明我们的编译器
# 生成了“正确”的代码...
if [ ! -d "$target/$os" -o ! -d "$idir" ]; then
	echo 2>&1 $0: unknown combination '"'$target/$os'"'
	exit 1
fi

# $1 就是编译测试C源程序生成的.s 汇编文件,
# 例如，在我们的例子中它就是: bin/x86/ubuntu15.04-amd64/tst/8q.s
# $1经过basename处理后变成了: 8q
C=`basename $1 .s`

# -. 的意思是: 如果BUILDDIR为空就用当前路径.
# 在我们的例子中它为: bin/x86/ubuntu15.04-amd64
BUILDDIR=${BUILDDIR-.}

# lcc编译器的完整路径
# 例如，在我们的例子中它就是:
# bin/x86/ubuntu15.04-amd64/lcc
LCC="${LCC-${BUILDDIR}/lcc} -Wo-lccdir=$BUILDDIR"

# 在我们的例子中它就是: bin/x86/ubuntu15.04-amd64/tst
TSTDIR=${TSTDIR-${BUILDDIR}/tst}
if [ ! -d $TSTDIR ]; then mkdir -p $TSTDIR; fi

echo ${BUILDDIR}/rcc$EXE -target=$target/$os $1: 1>&2
#$LCC -S -I$idir -Ualpha -Usun -Uvax -Umips -Ux86 \
#	-Wf-errout=$TSTDIR/$C.2 -D$target -Wf-g0 \
#	-Wf-target=$target/$os -o $1 tst/$C.c

# 将tst目录下的c源程序编译成汇编.s文件(.s文件被生成在bin/x86/ubuntu15.04-amd64/tst目录下)
# 编译过程中的错误输出保存到$TSTDIR目录中
$LCC -S -I$idir \
	-Wf-errout=$TSTDIR/$C.2 -D$target -Wf-g0 \
	-Wf-target=$target/$os -o $1 tst/$C.c

# 如果编译失败,那么远程主机就不用执行了...
if [ $? != 0 ]; then remotehost=noexecute; fi

# -r file : if file exist and readable
# .2bk : this is error output file.
# 将我们编译C源程序过程中生成的错误信息和提前预置的错误信息进行对比，看看是否有差异
if [ -r $dir/tst/$C.2bk ]; then
	diff $dir/tst/$C.2bk $TSTDIR/$C.2
fi

# .sbk : this is assembly file
# 将我们编译C源程序过程中生成的汇编文件和提前预置的汇编文件进行对比，看看是否有差异
if [ -r $dir/tst/$C.sbk ]; then
	if diff $dir/tst/$C.sbk $TSTDIR/$C.s; then exit 0; fi
fi

case "$remotehost" in
noexecute)	exit 0 ;;
""|"-")	$LCC -o $TSTDIR/$C$EXE $1; $TSTDIR/$C$EXE <tst/$C.0 >$TSTDIR/$C.1 ;;
*)	rcp $1 $remotehost:
	if expr "$remotehost" : '.*@' >/dev/null ; then
		remotehost="`expr $remotehost : '.*@\(.*\)'` -l `expr $remotehost : '\(.*\)@'`"
	fi
	rsh $remotehost "cc -o $C$EXE $C.s -lm;./$C$EXE;rm -f $C$EXE $C.[so]" <tst/$C.0 >$TSTDIR/$C.1
	;;
esac
if [ -r $dir/tst/$C.1bk ]; then
	diff $dir/tst/$C.1bk $TSTDIR/$C.1
	exit $?
fi
exit 0
