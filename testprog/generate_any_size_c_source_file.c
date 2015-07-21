#include <stdio.h>

int code_len = 100*1024; // 100KB c source code

int main(void) {
	char *str="int a%04d=10;";
	int str_len = 13;
	int str_nr = code_len / str_len;
	int i;
	for(i=0;i<str_nr;i++) {
		printf(str,i);
	}
	printf("\nint main(void) { return 0; }\n");
	return 0;
}
