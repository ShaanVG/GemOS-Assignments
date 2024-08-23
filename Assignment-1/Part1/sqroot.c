#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

int main(int argc, char *argv[])
{
	int n=atoi(argv[argc-1]);
	n=(int)sqrt((double)n);
	sprintf(argv[argc-1],"%d",n);
	if(argc>2){
		argv++;
	    if(execv(argv[0],argv)){
		printf("Unable to execute\n");
	    }
	}
	else printf("%d\n", n);
	return 0;
}