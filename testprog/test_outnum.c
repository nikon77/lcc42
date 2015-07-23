#include <stdio.h>

char * outnum(char *p, int n) {
	if (n >= 10)
		p = outnum(p, n/10);
	*p++ = n % 10 + '0';
	return p;
}

int main(void) {
	char arr[23]={0,};
	printf("arr = %p\n",arr);
	printf("outnum() = %p\n",outnum(arr,32768));
	{
		int i;
		for(i=0;i<23;i++)
			printf("%02x ",arr[i]);
	}
	printf("\n");
	return 0;
}
