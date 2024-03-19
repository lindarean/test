#include<stdio.h>

int main()
{	
	printf("hello\n");
	fork();
	printf("world\n");
	fork();
	return 0;
}
