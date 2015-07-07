#include <stdio.h>
#include <time.h>
int main(void) {
	time_t t;
	printf("hello,world!\n");
	t = time(NULL);
	printf("time is %s",ctime(&t));
	return 0;
}
