#include <stdio.h>
#ifdef __LCC__
char *str="__LCC__ is defined!\n";
#else
char *str="__LCC__ is not defined!\n";
#endif

#ifdef __LCC__
#ifndef __STDC__
#define __STDC__
#endif
#endif

int main(void) {
	printf("%s",str);
	return 0;
}
