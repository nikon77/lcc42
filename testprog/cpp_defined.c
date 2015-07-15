#include <stdio.h>

#define FOO 1
#define BAR

#if defined(FOO)
int foo = 1;
#else
int foo = 0;
#endif

#if defined(FOO) && defined(BAR)
int foo_bar = 1;
#else
int foo_bar = 0;
#endif

int main(void) {
	printf("foo = %d , foo_bar = %d\n",foo,foo_bar);
	return 0;
}
