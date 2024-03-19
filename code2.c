#include <stdio.h>

int main(void) {
	
	int n;
	scanf("%d",&n);
	int x,d,a;
	for(int i=0;i<n;i++)
	{
		scanf("%d\n",&x);
		if(x<=100)
		{
			d=0;
			a=x;
			printf("%d\n",a);

		}
		if(x>100 && x<=1000)
		{
			d=25;
			a=x-25;
			printf("%d\n",a);

		}
		if(x>1000 && x<=5000)
		{
			d=100;
			a=x-100;
			printf("%d\n",a);

		}
		if(x>5000)
		{
			d=500;
			a=x-500;
			printf("%d\n",a);

		}
	}
	return 0;
}
	
		
