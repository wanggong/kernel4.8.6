#include <stdio.h>
#include <string.h>
#include <stdlib.h>



void write_marker(char *str)
{
	FILE *f = fopen("/sys/kernel/debug/tracing/trace_marker" , "r+");
	fwrite(str , strlen(str) , 1 , f);
	fclose(f);

}

void bigstack()
{
	int s[4096];

	write_marker("wgz start malloc\n");
	void *p = malloc(4096*2);

	write_marker("wgz memset\n");
	memset(p,2,4096*2);

	write_marker("wgz memset2\n");
	memset(&s,2,sizeof(s));
	
	
}

int main(int argc , char **argv)
{
	
	scanf("input");
	write_marker("wgz start call bigstack\n");
	bigstack();
	
	printf("done\n");
}
