# $Id$

# 附加上自定义的Makefile文件
CUSTOM=custom.mk
-include $(CUSTOM)

# 静态库的扩展名
A=.a

# 目标文件的扩展名
O=.o

# 可执行文件的扩展名
E=

# 编译器
CC=gcc

# 编译器的选项(当前只有调试选项)
ifdef LCCDIR
CFLAGS=-g -DLCCDIR=\"$(LCCDIR)\" -m32
else
CFLAGS=-g -m32
endif

# 链接器的选项(当前只有调试选项)
LDFLAGS=-g -m32

# 链接器
LD=$(CC)

# 生成静态库的归档器
# r选项: 把文件files插入archive(替换).如果档案中已有的某个成员与插入文件的名称相同,此成员将被删除.
# u选项: 仅仅插入比档案中已有成员更新的文件
# v选项: 输出verbose信息.
AR=ar ruv

# Add an index to the archive, or update it if it already exists.
RANLIB=ranlib

# 文件差异比较工具
DIFF=diff

# 删除文件命令
RM=rm -f

# 目标文件输出目录
B=$(BUILDDIR)/

# 特定平台的测试用例目录
T=$(TSTDIR)/

# 第一个目标
# - 告诉make系统，如果目标中第一个命令执行失败，他会忽略掉这个错误，继续执行该目标中后续的命令
# @ 告诉make系统,禁止make打印此命令行.(但打印该命令行产生的输出信息)
what:
	-@echo make all rcc lburg lcpp lcc bprint liblcc triple clean clobber

# 程序要输出的几个目标
# rcc: 真正的C Compiler,即(Real C Compiler)
# lburg: the code-generator generator(代码产生器的产生器)
# lcpp: c preprocessor program(C预处理器)
# lcc: compile driver program(编译总驱动程序)
# bprint: xxx
# liblcc: lcc library
all::	config rcc lburg lcpp lcc bprint liblcc

config::
	./config_on_ubuntu15-amd64.sh
# rcc程序是真正的C Compiler,即(Real C Compiler)
# 它的依赖文件: 
# src/main.c  src/alloc.c  src/bind.c   src/dag.c  dagcheck.c  src/decl.c 
# src/enode.c  src/error.c  src/expr.c  src/event.c  src/init.c  src/inits.c  src/input.c  
# src/lex.c  src/list.c  src/main.c  src/output.c  src/prof.c  src/profio.c  src/simp.c  
# src/stmt.c  src/string.c  src/sym.c  src/trace.c  src/tree.c  src/types.c  src/null.c  
# src/symbolic.c  src/gen.c  src/bytecode.c  src/stab.c
# alpha.c  mips.c  sparc.c  x86.c  x86linux.c
# 其中 dagcheck.c 位于BUILDDIR目录中,它是由命令行: lburg src/dagcheck.md 生成.
# 其中 alpha.c 位于BUILDDIR目录中,它是由命令行: lburg src/alpha.md 生成.
# 其中 mips.c 位于BUILDDIR目录中,它是由命令行: lburg src/mips.md 生成.
# 其中 sparc.c 位于BUILDDIR目录中,它是由命令行: lburg src/sparc.md 生成.
# 其中 x86.c 位于BUILDDIR目录中,它是由命令行: lburg src/x86.md 生成.
# 其中 x86linux.c 位于BUILDDIR目录中,它是由命令行: lburg src/x86linux.md 生成.
rcc:	$Brcc$E

# the code-generator generator(代码产生器的产生器)
lburg:	$Blburg$E

# C预处理程序
# 它依赖于: cpp.c  cpp.h  eval.c  getopt.c  hideset.c  include.c  lex.c  macro.c  nlist.c  tokens.c  unix.c
# 这些文件均位于cpp目录下
lcpp:	$Blcpp$E

# lcc是一个编译总驱动程序(driver).它依赖两个源文件: etc/lcc.c 和 etc/$OS.c(在ubuntu上就是linux.c)
lcc:	$Blcc$E

# the profile printer
# 该程序依赖: etc/bprint.c  src/profio.c
bprint:	$Bbprint$E

# the runtime library
# 依赖: assert.c  bbexit.c  yynull.c
liblcc:	$Bliblcc$A

RCCOBJS=$Balloc$O \
	$Bbind$O \
	$Bdag$O \
	$Bdagcheck$O \
	$Bdecl$O \
	$Benode$O \
	$Berror$O \
	$Bexpr$O \
	$Bevent$O \
	$Binit$O \
	$Binits$O \
	$Binput$O \
	$Blex$O \
	$Blist$O \
	$Bmain$O \
	$Boutput$O \
	$Bprof$O \
	$Bprofio$O \
	$Bsimp$O \
	$Bstmt$O \
	$Bstring$O \
	$Bsym$O \
	$Btrace$O \
	$Btree$O \
	$Btypes$O \
	$Bnull$O \
	$Bsymbolic$O \
	$Bgen$O \
	$Bbytecode$O \
	$Balpha$O \
	$Bmips$O \
	$Bsparc$O \
	$Bstab$O \
	$Bx86$O \
	$Bx86linux$O

$Brcc$E::	$Bmain$O $Blibrcc$A $(EXTRAOBJS)
		$(LD) $(LDFLAGS) -o $@ $Bmain$O $(EXTRAOBJS) $Blibrcc$A $(EXTRALIBS)

$Blibrcc$A:	$(RCCOBJS)
		$(AR) $@ $(RCCOBJS); $(RANLIB) $@ || true

$(RCCOBJS):	src/c.h src/ops.h src/token.h src/config.h

$Balloc$O:	src/alloc.c;	$(CC) $(CFLAGS) -c -Isrc -o $@ src/alloc.c
$Bbind$O:	src/bind.c;	$(CC) $(CFLAGS) -c -Isrc -o $@ src/bind.c
$Bdag$O:	src/dag.c;	$(CC) $(CFLAGS) -c -Isrc -o $@ src/dag.c
$Bdecl$O:	src/decl.c;	$(CC) $(CFLAGS) -c -Isrc -o $@ src/decl.c
$Benode$O:	src/enode.c;	$(CC) $(CFLAGS) -c -Isrc -o $@ src/enode.c
$Berror$O:	src/error.c;	$(CC) $(CFLAGS) -c -Isrc -o $@ src/error.c
$Bevent$O:	src/event.c;	$(CC) $(CFLAGS) -c -Isrc -o $@ src/event.c
$Bexpr$O:	src/expr.c;	$(CC) $(CFLAGS) -c -Isrc -o $@ src/expr.c
$Bgen$O:	src/gen.c;	$(CC) $(CFLAGS) -c -Isrc -o $@ src/gen.c
$Binit$O:	src/init.c;	$(CC) $(CFLAGS) -c -Isrc -o $@ src/init.c
$Binits$O:	src/inits.c;	$(CC) $(CFLAGS) -c -Isrc -o $@ src/inits.c
$Binput$O:	src/input.c;	$(CC) $(CFLAGS) -c -Isrc -o $@ src/input.c
$Blex$O:	src/lex.c;	$(CC) $(CFLAGS) -c -Isrc -o $@ src/lex.c
$Blist$O:	src/list.c;	$(CC) $(CFLAGS) -c -Isrc -o $@ src/list.c
$Bmain$O:	src/main.c;	$(CC) $(CFLAGS) -c -Isrc -o $@ src/main.c
$Bnull$O:	src/null.c;	$(CC) $(CFLAGS) -c -Isrc -o $@ src/null.c
$Boutput$O:	src/output.c;	$(CC) $(CFLAGS) -c -Isrc -o $@ src/output.c
$Bprof$O:	src/prof.c;	$(CC) $(CFLAGS) -c -Isrc -o $@ src/prof.c
$Bprofio$O:	src/profio.c;	$(CC) $(CFLAGS) -c -Isrc -o $@ src/profio.c
$Bsimp$O:	src/simp.c;	$(CC) $(CFLAGS) -c -Isrc -o $@ src/simp.c
$Bstmt$O:	src/stmt.c;	$(CC) $(CFLAGS) -c -Isrc -o $@ src/stmt.c
$Bstring$O:	src/string.c;	$(CC) $(CFLAGS) -c -Isrc -o $@ src/string.c
$Bsym$O:	src/sym.c;	$(CC) $(CFLAGS) -c -Isrc -o $@ src/sym.c
$Bsymbolic$O:	src/symbolic.c;	$(CC) $(CFLAGS) -c -Isrc -o $@ src/symbolic.c
$Bbytecode$O:	src/bytecode.c;	$(CC) $(CFLAGS) -c -Isrc -o $@ src/bytecode.c
$Btrace$O:	src/trace.c;	$(CC) $(CFLAGS) -c -Isrc -o $@ src/trace.c
$Btree$O:	src/tree.c;	$(CC) $(CFLAGS) -c -Isrc -o $@ src/tree.c
$Btypes$O:	src/types.c;	$(CC) $(CFLAGS) -c -Isrc -o $@ src/types.c
$Bstab$O:	src/stab.c src/stab.h;	$(CC) $(CFLAGS) -c -Isrc -o $@ src/stab.c

$Bdagcheck$O:	$Bdagcheck.c;	$(CC) $(CFLAGS) -c -Isrc -o $@ $Bdagcheck.c
$Balpha$O:	$Balpha.c;	$(CC) $(CFLAGS) -c -Isrc -o $@ $Balpha.c
$Bmips$O:	$Bmips.c;	$(CC) $(CFLAGS) -c -Isrc -o $@ $Bmips.c
$Bsparc$O:	$Bsparc.c;	$(CC) $(CFLAGS) -c -Isrc -o $@ $Bsparc.c
$Bx86$O:	$Bx86.c;	$(CC) $(CFLAGS) -c -Isrc -o $@ $Bx86.c
$Bx86linux$O:	$Bx86linux.c;	$(CC) $(CFLAGS) -c -Isrc -o $@ $Bx86linux.c

$Bdagcheck.c:	$Blburg$E src/dagcheck.md; $Blburg src/dagcheck.md $@
$Balpha.c:	$Blburg$E src/alpha.md;    $Blburg src/alpha.md    $@
$Bmips.c:	$Blburg$E src/mips.md;     $Blburg src/mips.md     $@
$Bsparc.c:	$Blburg$E src/sparc.md;    $Blburg src/sparc.md    $@
$Bx86.c:	$Blburg$E src/x86.md;      $Blburg src/x86.md      $@
$Bx86linux.c:	$Blburg$E src/x86linux.md; $Blburg src/x86linux.md $@

$Bbprint$E:	$Bbprint$O;		$(LD) $(LDFLAGS) -o $@ $Bbprint$O 
$Bops$E:	$Bops$O;		$(LD) $(LDFLAGS) -o $@ $Bops$O 

$Bbprint$O:	etc/bprint.c src/profio.c;	$(CC) $(CFLAGS) -c -Isrc -o $@ etc/bprint.c
$Bops$O:	etc/ops.c src/ops.h;		$(CC) $(CFLAGS) -c -Isrc -o $@ etc/ops.c

$Blcc$E:	$Blcc$O $Bhost$O;	$(LD) $(LDFLAGS) -o $@ $Blcc$O $Bhost$O 

$Blcc$O:	etc/lcc.c;		$(CC) $(CFLAGS) -c -o $@ etc/lcc.c
$Bhost$O:	$(HOSTFILE);	$(CC) $(CFLAGS) -c -o $@ $(HOSTFILE)

LIBOBJS=$Bassert$O $Bbbexit$O $Byynull$O

$Bliblcc$A:	$(LIBOBJS);	$(AR) $@ $Bassert$O $Bbbexit$O $Byynull$O; $(RANLIB) $@ || true

$Bassert$O:	lib/assert.c;	$(CC) $(CFLAGS) -c -o $@ lib/assert.c
$Byynull$O:	lib/yynull.c;	$(CC) $(CFLAGS) -c -o $@ lib/yynull.c
$Bbbexit$O:	lib/bbexit.c;	$(CC) $(CFLAGS) -c -o $@ lib/bbexit.c

$Blburg$E:	$Blburg$O $Bgram$O;	$(LD) $(LDFLAGS) -o $@ $Blburg$O $Bgram$O 

$Blburg$O $Bgram$O:	lburg/lburg.h

$Blburg$O:	lburg/lburg.c;	$(CC) $(CFLAGS) -c -Ilburg -o $@ lburg/lburg.c
$Bgram$O:	lburg/gram.c;	$(CC) $(CFLAGS) -c -Ilburg -o $@ lburg/gram.c

CPPOBJS=$Bcpp$O $Blexer$O $Bnlist$O $Btokens$O $Bmacro$O $Beval$O \
	$Binclude$O $Bhideset$O $Bgetopt$O $Bunix$O

$Blcpp$E:	$(CPPOBJS)
		$(LD) $(LDFLAGS) -o $@ $(CPPOBJS) 

$(CPPOBJS):	cpp/cpp.h

$Bcpp$O:	cpp/cpp.c;	$(CC) $(CFLAGS) -c -Icpp -o $@ cpp/cpp.c
$Blexer$O:	cpp/lex.c;	$(CC) $(CFLAGS) -c -Icpp -o $@ cpp/lex.c
$Bnlist$O:	cpp/nlist.c;	$(CC) $(CFLAGS) -c -Icpp -o $@ cpp/nlist.c
$Btokens$O:	cpp/tokens.c;	$(CC) $(CFLAGS) -c -Icpp -o $@ cpp/tokens.c
$Bmacro$O:	cpp/macro.c;	$(CC) $(CFLAGS) -c -Icpp -o $@ cpp/macro.c
$Beval$O:	cpp/eval.c;	$(CC) $(CFLAGS) -c -Icpp -o $@ cpp/eval.c
$Binclude$O:	cpp/include.c;	$(CC) $(CFLAGS) -c -Icpp -o $@ cpp/include.c
$Bhideset$O:	cpp/hideset.c;	$(CC) $(CFLAGS) -c -Icpp -o $@ cpp/hideset.c
$Bgetopt$O:	cpp/getopt.c;	$(CC) $(CFLAGS) -c -Icpp -o $@ cpp/getopt.c
$Bunix$O:	cpp/unix.c;	$(CC) $(CFLAGS) -c -Icpp -o $@ cpp/unix.c

test:	$T8q.s \
	$Tarray.s \
	$Tcf.s \
	$Tcq.s \
	$Tcvt.s \
	$Tfields.s \
	$Tfront.s \
	$Tincr.s \
	$Tinit.s \
	$Tlimits.s \
	$Tparanoia.s \
	$Tsort.s \
	$Tspill.s \
	$Tstdarg.s \
	$Tstruct.s \
	$Tswitch.s \
	$Twf1.s \
	$Tyacc.s

$T8q.s:	tst/8q.c tst/8q.0 all;	@env BUILDDIR=$(BUILDDIR) TSTDIR=$(TSTDIR) src/run.sh $@
$Tarray.s:	tst/array.c tst/array.0 all;	@env BUILDDIR=$(BUILDDIR) TSTDIR=$(TSTDIR) src/run.sh $@
$Tcf.s:	tst/cf.c tst/cf.0 all;	@env BUILDDIR=$(BUILDDIR) TSTDIR=$(TSTDIR) src/run.sh $@
$Tcq.s:	tst/cq.c tst/cq.0 all;	@env BUILDDIR=$(BUILDDIR) TSTDIR=$(TSTDIR) src/run.sh $@
$Tcvt.s:	tst/cvt.c tst/cvt.0 all;	@env BUILDDIR=$(BUILDDIR) TSTDIR=$(TSTDIR) src/run.sh $@
$Tfields.s:	tst/fields.c tst/fields.0 all;	@env BUILDDIR=$(BUILDDIR) TSTDIR=$(TSTDIR) src/run.sh $@
$Tfront.s:	tst/front.c tst/front.0 all;	@env BUILDDIR=$(BUILDDIR) TSTDIR=$(TSTDIR) src/run.sh $@
$Tincr.s:	tst/incr.c tst/incr.0 all;	@env BUILDDIR=$(BUILDDIR) TSTDIR=$(TSTDIR) src/run.sh $@
$Tinit.s:	tst/init.c tst/init.0 all;	@env BUILDDIR=$(BUILDDIR) TSTDIR=$(TSTDIR) src/run.sh $@
$Tlimits.s:	tst/limits.c tst/limits.0 all;	@env BUILDDIR=$(BUILDDIR) TSTDIR=$(TSTDIR) src/run.sh $@
$Tparanoia.s:	tst/paranoia.c tst/paranoia.0 all;	@env BUILDDIR=$(BUILDDIR) TSTDIR=$(TSTDIR) src/run.sh $@
$Tsort.s:	tst/sort.c tst/sort.0 all;	@env BUILDDIR=$(BUILDDIR) TSTDIR=$(TSTDIR) src/run.sh $@
$Tspill.s:	tst/spill.c tst/spill.0 all;	@env BUILDDIR=$(BUILDDIR) TSTDIR=$(TSTDIR) src/run.sh $@
$Tstdarg.s:	tst/stdarg.c tst/stdarg.0 all;	@env BUILDDIR=$(BUILDDIR) TSTDIR=$(TSTDIR) src/run.sh $@
$Tstruct.s:	tst/struct.c tst/struct.0 all;	@env BUILDDIR=$(BUILDDIR) TSTDIR=$(TSTDIR) src/run.sh $@
$Tswitch.s:	tst/switch.c tst/switch.0 all;	@env BUILDDIR=$(BUILDDIR) TSTDIR=$(TSTDIR) src/run.sh $@
$Twf1.s:	tst/wf1.c tst/wf1.0 all;	@env BUILDDIR=$(BUILDDIR) TSTDIR=$(TSTDIR) src/run.sh $@
$Tyacc.s:	tst/yacc.c tst/yacc.0 all;	@env BUILDDIR=$(BUILDDIR) TSTDIR=$(TSTDIR) src/run.sh $@

testclean:
	$(RM) $T8q$E $T8q.s $T8q.2 $T8q.1
	$(RM) $Tarray$E $Tarray.s $Tarray.2 $Tarray.1
	$(RM) $Tcf$E $Tcf.s $Tcf.2 $Tcf.1
	$(RM) $Tcq$E $Tcq.s $Tcq.2 $Tcq.1
	$(RM) $Tcvt$E $Tcvt.s $Tcvt.2 $Tcvt.1
	$(RM) $Tfields$E $Tfields.s $Tfields.2 $Tfields.1
	$(RM) $Tfront$E $Tfront.s $Tfront.2 $Tfront.1
	$(RM) $Tincr$E $Tincr.s $Tincr.2 $Tincr.1
	$(RM) $Tinit$E $Tinit.s $Tinit.2 $Tinit.1
	$(RM) $Tlimits$E $Tlimits.s $Tlimits.2 $Tlimits.1
	$(RM) $Tparanoia$E $Tparanoia.s $Tparanoia.2 $Tparanoia.1
	$(RM) $Tsort$E $Tsort.s $Tsort.2 $Tsort.1
	$(RM) $Tspill$E $Tspill.s $Tspill.2 $Tspill.1
	$(RM) $Tstdarg$E $Tstdarg.s $Tstdarg.2 $Tstdarg.1
	$(RM) $Tstruct$E $Tstruct.s $Tstruct.2 $Tstruct.1
	$(RM) $Tswitch$E $Tswitch.s $Tswitch.2 $Tswitch.1
	$(RM) $Twf1$E $Twf1.s $Twf1.2 $Twf1.1
	$(RM) $Tyacc$E $Tyacc.s $Tyacc.2 $Tyacc.1

# 对于一个没有依赖而只有命令行的双冒号规则，当引用此目标时，规则的命令将会被无条件执行。
# 而普通规则，当规则的目标文件存在时，此规则的命令永远不会被执行（目标文件永远是最新的）。
# 更详细的参见我的EverNote中《makefile双冒号规则》一文
clean::		testclean
		$(RM) $B*$O
		$(RM) $Bdagcheck.c $Balpha.c $Bmips.c $Bx86.c $Bsparc.c $Bx86linux.c
		$(RM) $Brcc1$E $Brcc1$E $B1rcc$E $B2rcc$E
		$(RM) $B*.ilk
		./config_on_ubuntu15-amd64.sh clean

clobber::	clean
		$(RM) $Brcc$E $Blburg$E $Blcpp$E $Blcc$E $Bcp$E $Bbprint$E $B*$A
		$(RM) $B*.pdb $B*.pch

RCCSRCS=src/alloc.c \
	src/bind.c \
	src/dag.c \
	src/decl.c \
	src/enode.c \
	src/error.c \
	src/expr.c \
	src/event.c \
	src/init.c \
	src/inits.c \
	src/input.c \
	src/lex.c \
	src/list.c \
	src/main.c \
	src/output.c \
	src/prof.c \
	src/profio.c \
	src/simp.c \
	src/stmt.c \
	src/string.c \
	src/sym.c \
	src/trace.c \
	src/tree.c \
	src/types.c \
	src/null.c \
	src/symbolic.c \
	src/bytecode.c \
	src/gen.c \
	src/stab.c \
	$Bdagcheck.c \
	$Balpha.c \
	$Bmips.c \
	$Bsparc.c \
	$Bx86linux.c \
	$Bx86.c

# -A : warn about nonANSI usage; 2nd -A warns more
# -d0.6 : set switch statement density to 0.6
# -Woarg : specify system-specific `arg'
# -Idir : add `dir' to the beginning of the list of #include directories
C=$Blcc -A -d0.6 -Wo-lccdir=$(BUILDDIR) -Isrc -I$(BUILDDIR)
triple:	$B2rcc$E
	strip $B1rcc$E $B2rcc$E
	dd if=$B1rcc$E of=$Brcc1$E bs=512 skip=1
	dd if=$B2rcc$E of=$Brcc2$E bs=512 skip=1
	if cmp $Brcc1$E $Brcc2$E; then \
		mv $B2rcc$E $Brcc$E; \
		$(RM) $B1rcc$E $Brcc[12]$E; fi

# -B : -Bdir/  use the compiler named `dir/rcc'
$B1rcc$E:	$Brcc$E $Blcc$E $Blcpp$E
		$C -o $@ -B$B $(RCCSRCS)
$B2rcc$E:	$B1rcc$E
		$C -o $@ -B$B1 $(RCCSRCS)
