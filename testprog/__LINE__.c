#include <stdio.h>

int main(void) {
	printf("current source file line number is: %d\n",__LINE__);
#line 100
	printf("current source file line number is: %d\n",__LINE__);
# 500
	printf("current source file line number is: %d\n",__LINE__);
	return 0;
}
