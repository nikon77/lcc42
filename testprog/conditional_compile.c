#define FOO (-2)

#if FOO > 0
#define BAR (1)
#elif FOO == 0
#define BAR (0)
#else
#define BAR (-1)
#endif

int main(void) {
	int var_1 = FOO;
	int var_2 = BAR;
	return 0;
}
