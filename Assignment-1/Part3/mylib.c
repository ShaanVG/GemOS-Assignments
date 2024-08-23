#include <stdio.h>
#include <sys/mman.h>
#include <stdlib.h>

void* head=NULL;

void *memalloc(unsigned long size) 
{
	if(size<=0) return NULL;
	if(size%8!=0)
	size+=(8-size%8);
	if(size<16) size=16;
	//printf("memalloc() called\n");
	if(head==NULL){
		head=mmap(NULL, 4096*1024, PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0); /*gcc shutup*/
		*(unsigned long *)head=4096*1024;
		*(void **)(head+8)=NULL;
		*(void **)(head+16)=NULL;
		//printf("head: %p\n", head);
	}
	void* temp=head;
	while(temp!=NULL && *(unsigned long *)temp<size+8){
	    temp=*(void **)(temp+8);
	}
	if(temp==NULL){
		unsigned long x;
		if((size+8)%(4096*1024)!=0)
		x=(size+8)/(4096*1024)+1;
		else
		x=(size+8)/(4096*1024);
		void* ret=mmap(NULL, (x)*(4096*1024), PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0); /*gcc shutup*/
		*(unsigned long *)ret=size+8;
		//printf("%p\n", ret);
		void* new=ret+size+8;
		//printf("%p\n", new+16);
		//printf("%d\n", size);
		//printf("%lu\n", (x)*(4096*1024)-(size+8));
		if((x)*4096*1024-(size+8)<24){
			*(unsigned long *)ret=(x)*(4096*1024);
			return (void *)(ret+8);
		}
		if(new!=NULL){
		   *(unsigned long *)new=(x)*(4096*1024)-(size+8);
		//    *(void **)(new+8)=NULL;
		//    temp=head;
		//    while(*(void **)(temp+8)!=NULL) {
		// 	   temp=*(void **)(temp+8);
		//    }
		//    *(void **)(temp+8)=new;
		//    *(void **)(new+16)=temp;
		  // printf("new: %p\n", new);
		   *(void **)(head+16)=new;
		   *(void **)(new+8)=head;
		   *(void **)(new+16)=NULL;
		    head=new;
		}
		return (void *)(ret+8);
	}
	else{
		void* prev=*(void **)(temp+16);
		void* next=*(void **)(temp+8);
		if(*(unsigned long *)temp>=size+32){
			void* new=(void *)(temp+size+8);
			*(unsigned long *)new=*(int *)temp-(size+8);
			*(unsigned long *)temp=size+8;
			if(prev!=NULL){
				*(void **)(prev+8)=next;
				*(void **)(next+16)=prev;
				*(void **)(new+8)=head;
				*(void **)(new+16)=NULL;
				head=new;
			}
			else{
				head=new;
				*(void **)(new+16)=NULL;
				*(void **)(new+8)=next;
			}
			//printf("Called1:%p\n", temp);
			return (void *)(temp+8);
		}
		else{
			if(prev!=NULL){
				*(void **)(prev+8)=next;
				if(next!=NULL)
				*(void **)(next+16)=prev;
			}
			else{
				head=next;
				*(void **)(next+16)=NULL;
			}
			//printf("Called2:%p\n", temp);
			return (void *)(temp+8);
		}
	}
	return NULL;
}

int memfree(void *ptr)
{
	if(ptr==NULL) return -1;
	ptr=ptr-8;
	//*(int *)ptr+=8;
	//printf("ptr= %p\n", ptr);
	//printf("memfree() called\n");
	void* temp_left=head;
	while(temp_left!=NULL && (temp_left+*(unsigned long *)(temp_left))!=ptr){
		//printf("-------------------\n");
		//printf("temp_left= %p\n", temp_left);
		//printf("size of temp_left= %d\n", *(unsigned long *)temp_left);
		temp_left=*(void **)(temp_left+8);
		// printf("Hello\n");
	}
	//printf("temp_left= %p\n", temp_left);
	void* temp_right=head;
	while(temp_right!=NULL && (ptr+*(unsigned long *)(ptr))!=temp_right){
		temp_right=*(void **)(temp_right+8);
	}
	//printf("temp_right= %p\n", temp_right);
	if(temp_left!=NULL){
		void* left=*(void **)(temp_left+16);
		//printf("left:%p\n", left);
		void* right=*(void **)(temp_left+8);
		//printf("right:%p\n", right);
		*(unsigned long *)(temp_left)+=(*(unsigned long *)(ptr));
		ptr=temp_left;
		if(left!=NULL){
			*(void **)(left+8)=right;
		}
		else if(right!=NULL) head=right;
		if(right!=NULL){
			*(void **)(right+16)=left;
		}
	}
	if(temp_right!=NULL){
		void* left=*(void **)(temp_right+16);
		//printf("left:%p\n", left);
		void* right=*(void **)(temp_right+8);
		//printf("right:%p\n", right);
		*(unsigned long *)(ptr)+=(*(unsigned long *)(temp_right));
		if(left!=NULL){
			*(void **)(left+8)=right;
		}
		else if(right!=NULL) head=right;
		if(right!=NULL){
			*(void **)(right+16)=left;
		}
	}
	if(ptr!=head){
		//printf("head: %p\n", head);
		*(void **)(head+16)=ptr;
	*(void **)(ptr+8)=head;
	*(void **)(ptr+16)=NULL;
	}
	head=ptr;
	//printf("head: %p\n", head);printf("size: %lu\n", *(unsigned long *)head);
	//void* t=head;
	// printf("%p\n", (void *)(ptr));
	// printf("%p\n", *(void **)(ptr+8));
	// if(t!=NULL)
	// //printf("%p\n", *(void **)(t+8));
	// while(t!=NULL){
	// 	printf("%p\n", t);
	// 	printf("size: %lu\n", *(unsigned long *)t);
    //     t=*(void **)(t+8);
	// }
	return 0;
}	
