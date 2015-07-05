#!/bin/bash
# 此编译脚本使得lcc-4.2在ubuntu-15.04-desktop-amd64系统上编译成功
export CURRENTDIR=`pwd`
export BUILDBIN=${CURRENTDIR}/bin
export DEST_ARCH=x86
export OS=ubuntu15-amd64
export BUILDDIR=$BUILDBIN/${DEST_ARCH}-$OS
export TSTDIR=$BUILDBIN/${DEST_ARCH}-$OS/tst
export LCCDIR="$BUILDDIR/"

echo BUILDDIR=$BUILDDIR > custom.mk
echo LCCDIR=$LCCDIR >> custom.mk
echo TSTDIR=$TSTDIR >> custom.mk
echo HOSTFILE=etc/$OS.c >> custom.mk

build_fatal_error() {
	echo
	echo
	echo "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
	echo "FATAL ERROR:" $1
	echo "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
	echo
	echo
	exit 1
}

copy_include_files() {
	# 拷贝lcc自带的标准头文件到build目录
	mkdir $BUILDDIR/include && \
	cp -p -R include/${DEST_ARCH}/linux/* $BUILDDIR/include/ || \
	build_fatal_error "copy include files failed!"
}

if [ $# -ne 0 ] && [ $# -ne 1 ];then
	echo "$0 [clean]"
	exit 1
fi

if [ $# -eq 1 ] && [ $1 == "clean" ];then
	rm -rf $BUILDBIN || build_fatal_error "[1]delete $BUILDBIN failed!"
	mkdir -p $BUILDDIR || build_fatal_error "[2]create $BUILDDIR failed!"
	mkdir -p $TSTDIR || build_fatal_error "[3]create $TSTDIR failed!"
	copy_include_files
else # argc == 0
	# if the file $BUILDBIN exist but is not a directory
	if [ -e $BUILDBIN ] && [ ! -d $BUILDBIN ];then
		rm -rf $BUILDBIN || build_fatal_error "[4]delete $BUILDBIN failed!"
		mkdir -p $BUILDDIR || build_fatal_error "[5]create $BUILDDIR failed!"
		mkdir -p $TSTDIR || build_fatal_error "[6]create $TSTDIR failed!"
		copy_include_files
	elif [ ! -e $BUILDBIN ];then  # $BUILDBIN do not exist 
		mkdir -p $BUILDDIR || build_fatal_error "[7]create $BUILDDIR failed!"
		mkdir -p $TSTDIR || build_fatal_error "[8]create $TSTDIR failed!"
		copy_include_files
	else # $BUILDBIN exist and is a directory
		if [ ! -e "$BUILDDIR/include" ];then
			rm -rf $BUILDDIR/include || build_fatal_error "[9]delete $BUILDBIN/include failed"
			copy_include_files
		fi
	fi
fi
