#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>

unsigned long space_calculator(char* path){
    //printf("%s\n", path);
	struct stat f;
	if(lstat(path, &f)==-1){
		perror("stat()");
		exit(0);
	}
	long long int total=f.st_size;
	//printf("%lld\n", total);
	if(!S_ISDIR(f.st_mode)){
		printf("hi\n");
		return total;
	}
    DIR *dir=opendir(path);
	if(dir==NULL){
		printf("Unable to execute\n");
		exit(-1);
	}
	strcat(path, "/");
    struct dirent* ptr=readdir(dir);
	while(ptr!=NULL){
		char *path1=(char *)malloc(sizeof(char)*4096);
		strcpy(path1, path);
		strcat(path1, ptr->d_name);
		struct stat buf;
		if(lstat(path1, &buf)==-1){
			printf("%s\n", path1);
			perror("stat()");
			exit(-1);
		}
		//printf("%ld", buf.st_size);
		if(S_ISLNK(buf.st_mode)){
			char* buf=(char *)(malloc(sizeof(char)*4096));
			readlink(path1, buf, 4096);
			char* path2=(char *)malloc(sizeof(char)*4096);
			strcat(path2, path);
			strcat(path2, buf);
			total+=space_calculator(path1);
			free(path2);
			free(buf);
		}
		else if(S_ISDIR(buf.st_mode)){
			//printf("%s\n", ptr->d_name);
			//printf("%s %ld\n", path1, buf.st_size);
			if(strcmp(ptr->d_name, ".")==0 || strcmp(ptr->d_name, "..")==0){
			ptr=readdir(dir);
			continue;
		    }
            total+=space_calculator(path1);
		}
		else{
			total+=(long long int)buf.st_size;
		}
		ptr=readdir(dir);
	}
    return total;
}

int main(int argc, char *argv[])
{
	if(argc==1){
		printf("Unable to execute\n");
		exit(-1);
	}
	int fd[2];
	if(pipe(fd)==-1){
		printf("Unable to execute\n");
		exit(-1);
	}
	char* path=(char *)malloc(sizeof(char)*4096);
	path[0]='.'; path[1]='/';
	strcat(path,argv[1]);
	struct stat f;
	if(stat(path, &f)==-1){
		perror("stat()");
		exit(-1);
	}
	unsigned long total=f.st_size;
	strcat(path, "/");
	DIR *dir=opendir(argv[1]);
	if(dir==NULL){
		printf("Unable to execute\n");
		exit(-1);
	}
	struct dirent* ptr=readdir(dir);
	while(ptr!=NULL){
		char *path1=(char *)malloc(sizeof(char)*4096);
		strcpy(path1, path);
		strcat(path1, ptr->d_name);
		struct stat buf;
		if(lstat(path1, &buf)==-1){
            printf("%s\n", path1);
			perror("stat()");
			exit(-1);
		}
		//printf("%ld", buf.st_size);
		if(S_ISLNK(buf.st_mode)){
			char* buf=(char *)(malloc(sizeof(char)*4096));
			readlink(path1, buf, 4096);
			char* path2=(char *)malloc(sizeof(char)*4096);
			strcat(path2, path);
			strcat(path2, buf);
			total+=space_calculator(path1);
			free(path2);
			free(buf);
		}
		else if(S_ISDIR(buf.st_mode)){
			//printf("%s\n", ptr->d_name);
			//printf("%s %ld\n", path1, buf.st_size);
			if(strcmp(ptr->d_name, ".")==0 || strcmp(ptr->d_name, "..")==0){
			ptr=readdir(dir);
			continue;
		    }
			//total+=(long long int)buf.st_size;
			int f=fork();
			if(f==0){
				// printf("Entered child\n");
				// strcat(path, ptr->d_name);
				// dir=opendir(path);
				// strcat(path, "/");
				total=space_calculator(path1);
				//printf("%lld\n", total);
				dup2(fd[1],1);
                break;
			}
			else{
				wait(NULL);
				//printf("Parent\n");
				char buffer[BUFSIZ];
				unsigned long x;
				read(fd[0], buffer, sizeof(buffer));
				//printf("%s:-%s\n", path1, buffer);
				x=atoi(buffer);
				//printf("%lld\n", x);
				total+=x;
				//printf("%lld\n", total);
			}
		}
		else{
			total+=(unsigned long int)buf.st_size;
		}
		ptr=readdir(dir);
		free(path1);
	}
	printf("%ld\n", total);
	exit(0);
	return 0;
}
