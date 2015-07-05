void init(int argc, char *argv[]) {
	{extern void input_init(int, char *[]); input_init(argc, argv);} /* 输入缓冲初始化 */
	{extern void main_init(int, char *[]); main_init(argc, argv);}   /* 主函数选项参数初始化 */
	{extern void type_init(int, char *[]); type_init(argc, argv);}   /* 类型初始化 */
}
