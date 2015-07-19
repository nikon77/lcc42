这是一个对lcc-4.2源代码进行注释和分析的工程.
lcc是Christopher W.Fraser & David R.Hanson在《A Retargetable C Compiler: Design and Implementation》一书中介绍的编译器.
该工程的目标是对这个源码树进行注释和分析,最终将lcc移植到Ubuntu15-amd64系统下.

编译步骤：
1. ./config_on_ubuntu15-amd64.sh clean

2. make all

3. make test

4. make triple

生成的可执行文件生成在bin目录中.

当前已知的问题：
make test阶段，test suite中的paranoia.c和yacc.c未通过编译正确性测试。
