#!/bin/bash
# 此编译脚本使得lcc-4.2在ubuntu-15.04-desktop-amd64系统上编译成功
export CURRENTDIR=`pwd`
export BUILDBIN=${CURRENTDIR}/bin
export ARCH=x86 # 这里的架构不是指lcc自身程序ELF文件所在的架构,而是lcc编译器编译源码最终连接出的程序的架构
export OS=ubuntu15-amd64
export BUILDDIR=$BUILDBIN/$ARCH-$OS
export TSTDIR=$BUILDBIN/$ARCH-$OS/tst
export LCCDIR="$BUILDDIR/"

echo BUILDDIR=$BUILDDIR > custom.mk
echo LCCDIR=$LCCDIR >> custom.mk
echo TSTDIR=$TSTDIR >> custom.mk
echo HOSTFILE=etc/$OS.c >> custom.mk

build_fatal_error() {
	echo
	echo
	echo "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
	echo "FATAL ERROR:" $1
	echo "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
	echo
	echo
	exit 1
}

copy_file_and_create_link() {
	
	# 拷贝平台专有的头文件到build目录
	# 如果你正在linux上安装lcc,你必须在build目录中创建一个名叫gcc的链接文件
	# 来指向gcc的库目录(因为lcc使用gcc的C预处理和大多数gcc的头文件)
	mkdir $BUILDDIR/include && \
	cp -p -R include/$ARCH/linux/* $BUILDDIR/include/ || \
	build_fatal_error "copy file and create link failed!"
}

if [ $# -ne 0 ] && [ $# -ne 1 ];then
	echo "$0 [clean]"
	exit 1
fi

if [ $# -eq 1 ] && [ $1 == "clean" ];then
	rm -rf $BUILDBIN || build_fatal_error "[1]delete $BUILDBIN failed!"
	mkdir -p $BUILDDIR || build_fatal_error "[2]create $BUILDDIR failed!"
	mkdir -p $TSTDIR || build_fatal_error "[3]create $TSTDIR failed!"
	copy_file_and_create_link
else # argc == 1
	# if the file $BUILDBIN exist but is not a directory
	if [ -e $BUILDBIN ] && [ ! -d $BUILDBIN ];then
		rm -rf $BUILDBIN || build_fatal_error "[4]delete $BUILDBIN failed!"
		mkdir -p $BUILDDIR || build_fatal_error "[5]create $BUILDDIR failed!"
		mkdir -p $TSTDIR || build_fatal_error "[6]create $TSTDIR failed!"
		copy_file_and_create_link
	elif [ ! -e $BUILDBIN ];then  # $BUILDBIN do not exist 
		mkdir -p $BUILDDIR || build_fatal_error "[7]create $BUILDDIR failed!"
		mkdir -p $TSTDIR || build_fatal_error "[8]create $TSTDIR failed!"
		copy_file_and_create_link
	else # $BUILDBIN exist and is a directory
		if [ ! -e "$BUILDDIR/include" ];then
			rm -rf $BUILDDIR/include || build_fatal_error "[9]delete $BUILDBIN/include failed"
			copy_file_and_create_link
		fi
	fi
fi
