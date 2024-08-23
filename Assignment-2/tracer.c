#include<context.h>
#include<memory.h>
#include<lib.h>
#include<entry.h>
#include<file.h>
#include<tracer.h>


/*
Points to be noted:-
Check everywhere where you have freed memory. os_alloc and os_page_alloc works differently then malloc
*/

///////////////////////////////////////////////////////////////////////////
//// 		Start of Trace buffer functionality 		      /////
///////////////////////////////////////////////////////////////////////////

int is_valid_mem_range(unsigned long buff, u32 count, int access_bit) 
{
	//printk("Checking valid memrange\n");
	struct exec_context *current = get_current_ctx();
	for(int i=0;i<MAX_MM_SEGS;i++){
		u64 start = current->mms[i].start;
		u64 end;
		if(i!=MM_SEG_STACK) end = current->mms[i].next_free;
		else end = current->mms[i].end;
		if(start <= buff && end >= buff+count){
			if(access_bit==0){
				if(current->mms[i].access_flags & 1){
					return 1;
				}
				else{
					return 0;
				}
			}
			else if(access_bit==1){
				if(current->mms[i].access_flags & 2){
					return 1;
				}
				else{
					return 0;
				}
			}
			else{
				if(current->mms[i].access_flags & 4){
					return 1;
				}
				else{
					return 0;
				}
			}
		}
	}
	//write some code just in case it is ciruclar
	struct vm_area* start = current->vm_area;
	struct vm_area *temp = current->vm_area;
	while(temp!=NULL){
		if(temp->vm_start <= buff && temp->vm_end >= buff+count){
			if(access_bit==0){
				if(temp->access_flags & 1){
					return 1;
				}
				else{
					return 0;
				}
			}
			else if(access_bit==1){
				if(temp->access_flags & 2){
					return 1;
				}
				else{
					return 0;
				}
			}
			else{
				if(temp->access_flags & 4){
					return 1;
				}
				else{
					return 0;
				}
			}
		}
		temp = temp->vm_next;
		if(temp == start) break;
	}
	return 0;
}



long trace_buffer_close(struct file *filep)
{
	//free the buffer inside the trace buffer
	if(filep->ref_count > 1){ filep->ref_count -= 1; return 0;}
	if(filep == NULL) return -EINVAL;
	if(filep->fops == NULL) return -EINVAL;
	//free the trace_buffer_info structure
	if(filep->trace_buffer == NULL) return -EINVAL;
	if(filep->trace_buffer->buff == NULL) return -EINVAL;
	os_free(filep->fops, sizeof(struct fileops));
	os_page_free(2, filep->trace_buffer->buff);
	os_free(filep->trace_buffer, sizeof(struct trace_buffer_info));
	//free the files structure
	os_free(filep, sizeof(struct file));
	//make the tb_fd as NULL
	filep=NULL;
	return 0;	
}



int trace_buffer_read(struct file *filep, char *buff, u32 count)
{
	//printk("In read\n");
	//error codes in read
	//Return -EINVAL if trace buffer does not have read access
	//check validity of buff and trace buffer
	if(is_valid_mem_range((unsigned long)buff, count, 1)==0) return -EBADMEM;
	
	if(filep == NULL) return -EINVAL;

	if(filep->type!=TRACE_BUFFER) return -EINVAL;

	if(filep->trace_buffer->space == 4096){
	return 0;
	}
	else{
	    s32 i=0;
		//printk("count : %d\n", count);
	    while(i<count){
	    	if(filep->trace_buffer->read_offset == filep->trace_buffer->write_offset && i!=0){
				break;
			}
			buff[i] = *(char *)(filep->trace_buffer->buff+filep->trace_buffer->read_offset);
			filep->trace_buffer->read_offset = (filep->trace_buffer->read_offset + 1) % TRACE_BUFFER_MAX_SIZE;
			i++;
	    }
		//printk("%d", i);
		filep->trace_buffer->space = filep->trace_buffer->space + i;
		return i;
	}
	return 0;
}

int trace_buffer_write(struct file *filep, char *buff, u32 count)
{
	//error codes in write
	//Return -EINVAL if trace buffer does not have write access
	//Also check validity of buff and trace buffer
	if(is_valid_mem_range((unsigned long)buff, count, 0)==0) return -EBADMEM;

	if(filep == NULL) return -EINVAL;

	if(filep->type!=TRACE_BUFFER) return -EINVAL;

	if(filep->trace_buffer->space == 0) return 0;
	else{
		s32 i = 0;
		//printk("count : %d\n", count);
		while(i < count){
			if(filep->trace_buffer->write_offset == filep->trace_buffer->read_offset && i!=0){
				break;
			}
			*(char *)(filep->trace_buffer->buff+filep->trace_buffer->write_offset) = buff[i];
			filep->trace_buffer->write_offset = (filep->trace_buffer->write_offset + 1) % TRACE_BUFFER_MAX_SIZE;
			i++;
		}
		//printk("Write offset:%d\n", filep->trace_buffer->write_offset);
		//printk("%d\n", i);

		// if(filep->trace_buffer->write_offset == filep->trace_buffer->read_offset) 
		//     filep->trace_buffer->full = 1;
		filep->trace_buffer->space = filep->trace_buffer->space - i;
		return i;
	}
    return 0;
}

int sys_create_trace_buffer(struct exec_context *current, int mode)
{
	//To Do:- Error codes for Memory allocation

	//Error in case if mode is wrong
	if(mode != O_READ && mode != O_WRITE && mode!= O_RDWR) return -EINVAL;
	//find the next free file descriptor using the file array in current
	u32 tb_fd = 0;
	while(current->files[tb_fd] != NULL){
		tb_fd++;
	}
	if(tb_fd >= MAX_OPEN_FILES){
		return -EINVAL;
	}

	//alocate a new struct file
	current->files[tb_fd] = os_alloc(sizeof(struct file));
	if(current->files[tb_fd] == 0) return -ENOMEM;

	//assign mode
	current->files[tb_fd]->mode = mode;
	current->files[tb_fd]->ref_count = 1;

	//allocate memory using os_page_alloc and initialize its members
	current->files[tb_fd]->trace_buffer = os_alloc(sizeof(struct trace_buffer_info));
	if(current->files[tb_fd]->trace_buffer == 0) return -ENOMEM;

	current->files[tb_fd]->type = TRACE_BUFFER;
	current->files[tb_fd]->inode = NULL;

	current->files[tb_fd]->trace_buffer->buff = (char *)os_page_alloc(2);
	if(current->files[tb_fd]->trace_buffer->buff == 0) return -ENOMEM;


	//assign the new functions to the fileops structure
	current->files[tb_fd]->fops = os_alloc(sizeof(struct fileops));
	if(current->files[tb_fd]->fops == 0) return -ENOMEM;

	current->files[tb_fd]->fops->read = trace_buffer_read;
	current->files[tb_fd]->fops->write = trace_buffer_write;
	current->files[tb_fd]->fops->close = trace_buffer_close;


	//initialize the trace_buffer_info structure
	current->files[tb_fd]->trace_buffer->read_offset=0;
	current->files[tb_fd]->trace_buffer->write_offset=0;
	current->files[tb_fd]->trace_buffer->space=4096;

	//printk("Created Trace buffer\n");
	//return proper code
	return tb_fd;
}

///////////////////////////////////////////////////////////////////////////
//// 		Start of strace functionality 		      	      /////
///////////////////////////////////////////////////////////////////////////


int perform_tracing(u64 syscall_num, u64 param1, u64 param2, u64 param3, u64 param4)
{
	struct exec_context *current = get_current_ctx();
    

	if(current->st_md_base == NULL) return 0;

	if(current->st_md_base->is_traced==0){
		//printk("Not being traced yet\n");
		return 0;
	} //Should this return an error?
    
	if(syscall_num == SYSCALL_EXIT )return 0;
	//HAndling case of FILTER TRACING : Syscall should belong to the list of syscalls to be traced
	if(current->st_md_base->tracing_mode==FILTERED_TRACING){
		s32 found=0;
		struct strace_info *temp = current->st_md_base->next;
		while(temp!=NULL){
			if(temp->syscall_num == syscall_num){
				found=1;
				break;
			}
			temp=temp->next;
		}
		if(found == 0) return 0;
	}
	//printk("Syscall:%d\n", syscall_num);
	//  printk("Param 1:%d\n", param1);
	//         printk("Param 2:%d\n", param2);
	//         printk("Param 3:%d\n", param3);
	//         printk("Param 4:%d\n", param4);
	//Address of trace buffer
	struct file *trace_buff = current->files[current->st_md_base->strace_fd];
	// if(syscall_num == SYSCALL_END_STRACE || syscall_num == SYSCALL_START_STRACE) return 0;
	// //check if trace buffer is full?
	// *(u64 *)(trace_buff->trace_buffer->buff+trace_buff->trace_buffer->write_offset) = syscall_num;
	// *(u64 *)(trace_buff->trace_buffer->buff+trace_buff->trace_buffer->write_offset+sizeof(u64)) = param1;
	// *(u64 *)(trace_buff->trace_buffer->buff+trace_buff->trace_buffer->write_offset+2*sizeof(u64)) = param2;
	// *(u64 *)(trace_buff->trace_buffer->buff+trace_buff->trace_buffer->write_offset+3*sizeof(u64)) = param3;
	// *(u64 *)(trace_buff->trace_buffer->buff+trace_buff->trace_buffer->write_offset+4*sizeof(u64)) = param4;
	// trace_buff->trace_buffer->write_offset = (trace_buff->trace_buffer->write_offset + 5*sizeof(u64)) % TRACE_BUFFER_MAX_SIZE;	
	// trace_buff->trace_buffer->space = trace_buff->trace_buffer->space - 5*sizeof(u64);
    // return 0;
	if(syscall_num == SYSCALL_READ || syscall_num == SYSCALL_MPROTECT ||
		   syscall_num == SYSCALL_WRITE || 
		   syscall_num == SYSCALL_LSEEK || syscall_num == SYSCALL_READ_STRACE ||
		   syscall_num == SYSCALL_READ_FTRACE){ //no. of parameters = 3
			*(u64 *)(trace_buff->trace_buffer->buff+trace_buff->trace_buffer->write_offset) = syscall_num;
	        *(u64 *)(trace_buff->trace_buffer->buff+trace_buff->trace_buffer->write_offset+sizeof(u64)) = param1;
	        *(u64 *)(trace_buff->trace_buffer->buff+trace_buff->trace_buffer->write_offset+2*sizeof(u64)) = param2;
	        *(u64 *)(trace_buff->trace_buffer->buff+trace_buff->trace_buffer->write_offset+3*sizeof(u64)) = param3;
			//printk("Syscall Value stored:%d %d\n", syscall_num, *(u64 *)(trace_buff->trace_buffer->buff+trace_buff->trace_buffer->write_offset));
			trace_buff->trace_buffer->write_offset = (trace_buff->trace_buffer->write_offset + 4*sizeof(u64)) % TRACE_BUFFER_MAX_SIZE;	
			trace_buff->trace_buffer->space = trace_buff->trace_buffer->space - 4*sizeof(u64);
		}
		else if(syscall_num == SYSCALL_FORK || syscall_num == SYSCALL_GETPID ||
		        syscall_num == SYSCALL_STATS || syscall_num == SYSCALL_CFORK ||
				syscall_num == SYSCALL_PHYS_INFO || syscall_num == SYSCALL_VFORK ||
				syscall_num == SYSCALL_GET_USER_P || syscall_num == SYSCALL_GET_COW_F ||
				syscall_num ==  SYSCALL_GETPPID){ //no. of parameters = 0 && SYSCALL_STRACE_END
		    *(u64 *)(trace_buff->trace_buffer->buff+trace_buff->trace_buffer->write_offset) = syscall_num;
			//printk("Syscall Value stored:%d %d\n", syscall_num, *(u64 *)(trace_buff->trace_buffer->buff+trace_buff->trace_buffer->write_offset));
			trace_buff->trace_buffer->write_offset = (trace_buff->trace_buffer->write_offset + sizeof(u64)) % TRACE_BUFFER_MAX_SIZE;	
			trace_buff->trace_buffer->space = trace_buff->trace_buffer->space - sizeof(u64);
		}
		else if(syscall_num == SYSCALL_CLOSE || syscall_num == SYSCALL_EXIT 
		    ||  syscall_num == SYSCALL_SLEEP || syscall_num == SYSCALL_CONFIGURE ||
			    syscall_num == SYSCALL_DUMP_PTT || syscall_num == SYSCALL_PMAP ||
				syscall_num == SYSCALL_DUP || syscall_num == SYSCALL_TRACE_BUFFER){ //No. of parameters = 1
		    *(u64 *)(trace_buff->trace_buffer->buff+trace_buff->trace_buffer->write_offset) = syscall_num;
			*(u64 *)(trace_buff->trace_buffer->buff+trace_buff->trace_buffer->write_offset+sizeof(u64)) = param1;
			//printk("Syscall Value stored:%d %d\n", syscall_num, *(u64 *)(trace_buff->trace_buffer->buff+trace_buff->trace_buffer->write_offset));
			trace_buff->trace_buffer->write_offset = (trace_buff->trace_buffer->write_offset + 2*sizeof(u64)) % TRACE_BUFFER_MAX_SIZE;	
			trace_buff->trace_buffer->space = trace_buff->trace_buffer->space - 2*sizeof(u64);
		} 
		else if(syscall_num ==  SYSCALL_EXPAND || syscall_num == SYSCALL_SHRINK ||
		        syscall_num == SYSCALL_SIGNAL || syscall_num == SYSCALL_CLONE ||
				syscall_num == SYSCALL_MUNMAP || syscall_num == SYSCALL_DUP2 ||
				syscall_num == SYSCALL_STRACE ||
				syscall_num == SYSCALL_OPEN ){ //No. of parameters = 2 : Also start_strace add karu kya?
			*(u64 *)(trace_buff->trace_buffer->buff+trace_buff->trace_buffer->write_offset) = syscall_num;
	        *(u64 *)(trace_buff->trace_buffer->buff+trace_buff->trace_buffer->write_offset+sizeof(u64)) = param1;
	        *(u64 *)(trace_buff->trace_buffer->buff+trace_buff->trace_buffer->write_offset+2*sizeof(u64)) = param2;
			//printk("Syscall Value stored:%d %d\n", syscall_num, *(u64 *)(trace_buff->trace_buffer->buff+trace_buff->trace_buffer->write_offset));
			trace_buff->trace_buffer->write_offset = (trace_buff->trace_buffer->write_offset + 3*sizeof(u64)) % TRACE_BUFFER_MAX_SIZE;	
			trace_buff->trace_buffer->space = trace_buff->trace_buffer->space - 3*sizeof(u64);
		}
		else if(syscall_num ==  SYSCALL_MMAP || syscall_num == SYSCALL_FTRACE){ //No. of parameters = 4
		    // printk("System call inside:%d\n", syscall_number);
			*(u64 *)(trace_buff->trace_buffer->buff+trace_buff->trace_buffer->write_offset) = syscall_num;
	     	*(u64 *)(trace_buff->trace_buffer->buff+trace_buff->trace_buffer->write_offset+sizeof(u64)) = param1;
	     	*(u64 *)(trace_buff->trace_buffer->buff+trace_buff->trace_buffer->write_offset+2*sizeof(u64)) = param2;
	     	*(u64 *)(trace_buff->trace_buffer->buff+trace_buff->trace_buffer->write_offset+3*sizeof(u64)) = param3;
	     	*(u64 *)(trace_buff->trace_buffer->buff+trace_buff->trace_buffer->write_offset+4*sizeof(u64)) = param4;
			//printk("Syscall Value stored:%d %d\n", syscall_num, *(u64 *)(trace_buff->trace_buffer->buff+trace_buff->trace_buffer->write_offset));
			trace_buff->trace_buffer->write_offset = (trace_buff->trace_buffer->write_offset + 5*sizeof(u64)) % TRACE_BUFFER_MAX_SIZE;	
			trace_buff->trace_buffer->space = trace_buff->trace_buffer->space - 5*sizeof(u64);
		}
		// else{
		// 	printk("Invalid syscall number\n");
		// }
		return 0;
}


int sys_strace(struct exec_context *current, int syscall_num, int action)
{
	//printk("Entered sys_strace\n");
	//Check validity of current, syscall_num, action?
	if(current->st_md_base == NULL){
		//printk("If st_md_base is NULL\n");
		 current->st_md_base = os_alloc(sizeof(struct strace_head));
		 if(current->st_md_base == 0) return -ENOMEM;
		 current->st_md_base->is_traced=0;
		 current->st_md_base->count = 0;
		 current->st_md_base->next = NULL;
		 current->st_md_base->last = NULL;
	}
	// if(current->st_md_base->is_traced==0){
	// 	return -EINVAL;
	// } //Should this return an error?

	// if(current->st_md_base->tracing_mode!=FILTERED_TRACING){
	// 	return -EINVAL;
	// }//Should this also return an error?

	//Error if syscall number doesnt exist
	//Error if already stored syscalls greater than MAX_STRACE

	//If action is ADD_STRACE
	//printk("Before if else in sys_strace\n");
	if(action == ADD_STRACE){
		//printk("Add Strace\n");
		if(current->st_md_base->count>=STRACE_MAX) return -EINVAL;
		struct strace_info *temp = current->st_md_base->next;
		while(temp!=NULL){ // if syscall already exists
			if(temp->syscall_num == syscall_num) return -EINVAL;
			temp = temp->next;
		}
		//allocating area for new node
		//printk("checked if syscall already exists\n");
		struct strace_info *new = os_alloc(sizeof(struct strace_info));
		if(new == 0) return -EINVAL;
		new->syscall_num = syscall_num;
		new->next = NULL;

		if(current->st_md_base->last!=NULL) current->st_md_base->last->next = new;
		current->st_md_base->last=new; // Add new node at end of list and update the list

		if(current->st_md_base->next==NULL) current->st_md_base->next=new;

		current->st_md_base->count++;
		//printk("Added successfully\n");
		return 0;
	}
	else if(action == REMOVE_STRACE){
		struct strace_info* temp = current->st_md_base->next;
		while(temp!=NULL){
			if(temp->syscall_num == syscall_num){
				struct strace_info *temp1 = current->st_md_base->next;
				while(temp1!=NULL){
					if(temp1->next == temp) break;
					temp1 = temp1->next;
				}
				struct strace_info *temp2 = temp->next;

				if(temp1==NULL) current->st_md_base->next = temp2;
				else temp1->next = temp2;

				os_free(temp, sizeof(struct strace_info));
				current->st_md_base->count--;
				return 0;
			}
			temp=temp->next;
		}
		return -EINVAL;
	}
    //printk("Shouldnt be here\n");
	return -EINVAL;
}

int sys_read_strace(struct file *filep, char *buff, u64 count)
{
	s32 i=0;
	s32 size=0;
	if(filep == NULL) return -EINVAL;
	if(filep->type!=TRACE_BUFFER) return -EINVAL;
	if(buff == NULL) return -EINVAL;

	while(i<count && filep->trace_buffer->space!=4096){
		// printk("Value of i:%d\n", i);
		u64 syscall_number=*(u64 *)(filep->trace_buffer->buff+filep->trace_buffer->read_offset);
		// u64 param1=*(u64 *)(filep->trace_buffer->buff+filep->trace_buffer->read_offset+sizeof(u64));
		// u64 param2=*(u64 *)(filep->trace_buffer->buff+filep->trace_buffer->read_offset+2*sizeof(u64));
		// u64 param3=*(u64 *)(filep->trace_buffer->buff+filep->trace_buffer->read_offset+3*sizeof(u64));
		// u64 param4=*(u64 *)(filep->trace_buffer->buff+filep->trace_buffer->read_offset+4*sizeof(u64));
		// filep->trace_buffer->read_offset = (filep->trace_buffer->read_offset + 5*sizeof(u64)) % TRACE_BUFFER_MAX_SIZE;
		// filep->trace_buffer->space = filep->trace_buffer->space + 5*sizeof(u64);
		//printk("System call outside:%d\n", syscall_number);
		//printk("%d\n", size);
		if(syscall_number == SYSCALL_READ || syscall_number == SYSCALL_MPROTECT ||
		   syscall_number == SYSCALL_WRITE || 
		   syscall_number == SYSCALL_LSEEK || syscall_number == SYSCALL_READ_STRACE ||
		   syscall_number == SYSCALL_READ_FTRACE){ //no. of parameters = 3
			*(u64 *)buff = syscall_number;
			*(u64 *)(buff + 1*sizeof(u64)) = *(u64 *)(filep->trace_buffer->buff+filep->trace_buffer->read_offset+sizeof(u64));
			*(u64 *)(buff + 2*sizeof(u64)) = *(u64 *)(filep->trace_buffer->buff+filep->trace_buffer->read_offset+2*sizeof(u64));
			*(u64 *)(buff + 3*sizeof(u64)) = *(u64 *)(filep->trace_buffer->buff+filep->trace_buffer->read_offset+3*sizeof(u64));
			buff = buff + 4*sizeof(u64);
			size+=4*sizeof(u64);
			filep->trace_buffer->read_offset = (filep->trace_buffer->read_offset + 4*sizeof(u64)) % TRACE_BUFFER_MAX_SIZE;
		filep->trace_buffer->space = filep->trace_buffer->space + 4*sizeof(u64);
		}
		else if(syscall_number == SYSCALL_FORK || syscall_number == SYSCALL_GETPID ||
		        syscall_number == SYSCALL_STATS || syscall_number == SYSCALL_CFORK ||
				syscall_number == SYSCALL_PHYS_INFO || syscall_number == SYSCALL_VFORK ||
				syscall_number == SYSCALL_GET_USER_P || syscall_number == SYSCALL_GET_COW_F ||
				syscall_number ==  SYSCALL_GETPPID){ //no. of parameters = 0 && SYSCALL_STRACE_END
		    *(u64 *)buff = syscall_number;
			buff = buff + sizeof(u64);
			size+=sizeof(u64);
			filep->trace_buffer->read_offset = (filep->trace_buffer->read_offset + 1*sizeof(u64)) % TRACE_BUFFER_MAX_SIZE;
		filep->trace_buffer->space = filep->trace_buffer->space + 1*sizeof(u64);
		}
		else if(syscall_number == SYSCALL_CLOSE || syscall_number == SYSCALL_EXIT 
		    ||  syscall_number == SYSCALL_SLEEP || syscall_number == SYSCALL_CONFIGURE ||
			    syscall_number == SYSCALL_DUMP_PTT || syscall_number == SYSCALL_PMAP ||
				syscall_number == SYSCALL_DUP || syscall_number == SYSCALL_TRACE_BUFFER){ //No. of parameters = 1
		    *(u64 *)buff = syscall_number;
			*(u64 *)(buff + 1*sizeof(u64)) = *(u64 *)(filep->trace_buffer->buff+filep->trace_buffer->read_offset+sizeof(u64));
			buff = buff + 2*sizeof(u64);
			size+=2*(sizeof(u64));
			filep->trace_buffer->read_offset = (filep->trace_buffer->read_offset + 2*sizeof(u64)) % TRACE_BUFFER_MAX_SIZE;
			filep->trace_buffer->space = filep->trace_buffer->space + 2*sizeof(u64);
		} 
		else if(syscall_number ==  SYSCALL_EXPAND || syscall_number == SYSCALL_SHRINK ||
		        syscall_number == SYSCALL_SIGNAL || syscall_number == SYSCALL_CLONE ||
				syscall_number == SYSCALL_MUNMAP || syscall_number == SYSCALL_DUP2 ||
				syscall_number == SYSCALL_STRACE ||
				syscall_number == SYSCALL_OPEN ){ //No. of parameters = 2 : Also start_strace add karu kya?
			*(u64 *)buff = syscall_number;
			*(u64 *)(buff + 1*sizeof(u64)) = *(u64 *)(filep->trace_buffer->buff+filep->trace_buffer->read_offset+sizeof(u64));
			*(u64 *)(buff + 2*sizeof(u64)) = *(u64 *)(filep->trace_buffer->buff+filep->trace_buffer->read_offset+2*sizeof(u64));
			buff = buff + 3*sizeof(u64);
			size+=3*(sizeof(u64));
			filep->trace_buffer->read_offset = (filep->trace_buffer->read_offset + 3*sizeof(u64)) % TRACE_BUFFER_MAX_SIZE;
			filep->trace_buffer->space = filep->trace_buffer->space + 3*sizeof(u64);
		}
		else if(syscall_number ==  SYSCALL_MMAP || syscall_number == SYSCALL_FTRACE){ //No. of parameters = 4
		    // printk("System call inside:%d\n", syscall_number);
			*(u64 *)buff = syscall_number;
			*(u64 *)(buff + 1*sizeof(u64)) = *(u64 *)(filep->trace_buffer->buff+filep->trace_buffer->read_offset+sizeof(u64));
			*(u64 *)(buff + 2*sizeof(u64)) = *(u64 *)(filep->trace_buffer->buff+filep->trace_buffer->read_offset+2*sizeof(u64));
			*(u64 *)(buff + 3*sizeof(u64)) = *(u64 *)(filep->trace_buffer->buff+filep->trace_buffer->read_offset+3*sizeof(u64));
			*(u64 *)(buff + 4*sizeof(u64)) = *(u64 *)(filep->trace_buffer->buff+filep->trace_buffer->read_offset+4*sizeof(u64));
			buff = buff + 5*sizeof(u64);
			size+=5*sizeof(u64);
			filep->trace_buffer->read_offset = (filep->trace_buffer->read_offset + 5*sizeof(u64)) % TRACE_BUFFER_MAX_SIZE;
			filep->trace_buffer->space = filep->trace_buffer->space + 5*sizeof(u64);
		}
		else{
			printk("Invalid syscall number\n");
		}
		i++;
	}
	return size;
}

int sys_start_strace(struct exec_context *current, int fd, int tracing_mode)
{
	//To do : Error Handling
    if(tracing_mode != FULL_TRACING && tracing_mode != FILTERED_TRACING) return -EINVAL;
	if(current->files[fd] == NULL) return -EINVAL;
	if(current->files[fd]->type!=TRACE_BUFFER) return -EINVAL;
	//Allocate memory to the st_md_base : Do I have to allot memory or is already alloted??
	if(current->st_md_base == NULL){
	    current->st_md_base = os_alloc(sizeof(struct strace_head));
		if(current->st_md_base == 0) return -EINVAL;
        //Initialize the pointers to NULL
	    current->st_md_base->next=NULL;
	    current->st_md_base->last=NULL;
	}

	//Initializing count to 0
	current->st_md_base->count=0;

	//Enable bit to show that we have started tracing
	current->st_md_base->is_traced=1;

	//Initializing the trace buffer
	current->st_md_base->strace_fd=fd;

	//Trace mode
	current->st_md_base->tracing_mode=tracing_mode;


	return 0;
}

int sys_end_strace(struct exec_context *current)
{
	//To do : Error Handling
	//Free all the pointers in strace_info
	struct strace_info *temp = current->st_md_base->next;

	while(temp!=NULL){
		struct strace_info *temp1 = temp;
		temp = temp->next;
		os_free(temp1, sizeof(struct strace_info));
		temp1=NULL;
	}

	current->st_md_base->is_traced=0;
	//Finally free the strace_head
	os_free(current->st_md_base, sizeof(struct strace_head));
	
	//printk("st_md_base value:%d\n", (u64)current->st_md_base);
	//printk("st_md_base->istraced value:%d\n", current->st_md_base->count);
	current->st_md_base=NULL;
	return 0;
}



///////////////////////////////////////////////////////////////////////////
//// 		Start of ftrace functionality 		      	      /////
///////////////////////////////////////////////////////////////////////////

//Function prologue: pushes rbp of current function to stack and then make the current rsp as the new rbp
long do_ftrace(struct exec_context *ctx, unsigned long faddr, long action, long nargs, int fd_trace_buffer)
{
	//printk("Entered do_ftrace\n");
	// void* ptr=(void *)faddr;
	//printk("Address sent 1:%x\n", faddr);
	// *(u8 *)(ptr)=INV_OPCODE;
	// *(u8 *)(ptr+1)=INV_OPCODE;
	// *(u8 *)(ptr+2)=INV_OPCODE;
	// *(u8 *)(ptr+3)=INV_OPCODE;
	// printk("Address sent 2:%x\n", *(u32 *)ptr);

    if(ctx->ft_md_base == NULL){
		ctx->ft_md_base = os_alloc(sizeof(struct ftrace_head));
		if(ctx->ft_md_base == 0) return -EINVAL; 
		ctx->ft_md_base->count=0;
		ctx->ft_md_base->next=NULL;
		ctx->ft_md_base->last=NULL;
	}

	//Check if faddr is valid??
	if(action == ADD_FTRACE){
		//check count
		if(ctx->files[fd_trace_buffer] == NULL) return -EINVAL;
	    if(ctx->files[fd_trace_buffer]->type!=TRACE_BUFFER) return -EINVAL;
		if(nargs>MAX_ARGS) return -EINVAL;
		if(ctx->ft_md_base->count>=FTRACE_MAX) return -EINVAL;
		//search if already exists, if so return error
		struct ftrace_info *search = ctx->ft_md_base->next;
		while(search!=NULL){
			if(search->faddr == faddr){
				//printk("Already in list\n");
				return -EINVAL;
			}
			search=search->next;
		}

		struct ftrace_info *temp = os_alloc(sizeof(struct ftrace_info));
		if(temp == 0) return -EINVAL;
		temp->faddr = faddr;
		temp->num_args = nargs;
		temp->fd = fd_trace_buffer;
		temp->capture_backtrace = 0;
		temp->next = NULL;

		//Saving code in code_backup
		void* ptr=(void *)faddr;
		for(s32 i=0;i<4;i++){
			temp->code_backup[i] = *(u8 *)(ptr+i);
		}

		if(ctx->ft_md_base->next == NULL){
			ctx->ft_md_base->next = temp;
			ctx->ft_md_base->last = temp;
		}
		else{
			ctx->ft_md_base->last->next = temp;
			ctx->ft_md_base->last = temp;
		}
		ctx->ft_md_base->count++;
	}
	else if(action == REMOVE_FTRACE){
		//Find the node to be removed
		struct ftrace_info *temp = ctx->ft_md_base->next;
		s32 found = 0;
		while(temp!=NULL){
			if(temp->faddr == faddr){
				found = 1;
				break;
			}
			temp=temp->next;
		}
		if(found == 0) return -EINVAL;
		struct ftrace_info* to_be_deleted=temp;

		//Find previous node
		temp = ctx->ft_md_base->next;
		while(temp!=NULL){
			if(temp->next == to_be_deleted) break;
			temp=temp->next;
		}

	    //Update the list
		if(temp!=NULL){
			temp->next = to_be_deleted->next;
		}
		else{
			ctx->ft_md_base->next = to_be_deleted->next;
		}

		//Restore code backup just in case it is enabled
		void* ptr = (void *)faddr;
		for(s32 i=0;i<4;i++){
			*(u8 *)(ptr+i) = to_be_deleted->code_backup[i];
		}
		
		//free memory
		os_free(to_be_deleted, sizeof(struct ftrace_info));

		//reduce the count
		ctx->ft_md_base->count--;
	}
	else if(action == ENABLE_FTRACE){
		struct ftrace_info *temp = ctx->ft_md_base->next;
		s32 found = 0;
		while(temp!=NULL){
			if(temp->faddr == faddr){
				found = 1;
				break;
			}
			temp=temp->next;
		}
		if(found == 0) return -EINVAL;
		void* ptr = (void *)faddr;
		for(s32 i=0;i<4;i++){
			*(u8 *)(ptr+i) = INV_OPCODE;
		}
	}
	else if(action == DISABLE_FTRACE){
		//Search if function already exists
		struct ftrace_info *temp = ctx->ft_md_base->next;
		s32 found = 0;
		while(temp!=NULL){
			if(temp->faddr == faddr){
				found = 1;
				break;
			}
			temp=temp->next;
		}
		if(found == 0) return -EINVAL;
		temp->capture_backtrace = 0;
		//Restore code backup just in case it is enabled
		void* ptr = (void *)faddr;
		for(s32 i=0;i<4;i++){
			*(u8 *)(ptr+i) = temp->code_backup[i];
		}
	}
	else if(action == ENABLE_BACKTRACE){
		//Search if function already exists
		struct ftrace_info *temp = ctx->ft_md_base->next;
		s32 found = 0;
		while(temp!=NULL){
			if(temp->faddr == faddr){
				found = 1;
				break;
			}
			temp=temp->next;
		}
		if(found == 0) return -EINVAL;
		void* ptr = (void *)faddr;
		for(s32 i=0;i<4;i++){
			*(u8 *)(ptr+i) = INV_OPCODE;
		}
		temp->capture_backtrace = 1;	
	}
	else if(action == DISABLE_BACKTRACE){
		//Search if function already exists
		struct ftrace_info *temp = ctx->ft_md_base->next;
		s32 found = 0;
		while(temp!=NULL){
			if(temp->faddr == faddr){
				found = 1;
				break;
			}
			temp=temp->next;
		}
		if(found == 0) return -EINVAL;

		temp->capture_backtrace = 0;

		void* ptr = (void *)faddr;
		for(s32 i=0;i<4;i++){
			*(u8 *)(ptr+i) = temp->code_backup[i];
		}
	}
	else return -EINVAL;
    return 0;
}

//Fault handler
long handle_ftrace_fault(struct user_regs *regs)
{
	//printk("Inside Handling Ftrace fault");
	//printk("Value of RIP:-%x\n", regs->entry_rip);
    

    struct exec_context *current = get_current_ctx();

	//Search for entry of function in list
	struct ftrace_info *func = current->ft_md_base->next;
	while(1){
		if(func->faddr == regs->entry_rip) break;
		func=func->next;
	}

	//Important registers
	//rsp -->stack pointer
	//rbp -->base pointer
	//Instructions we need to emulate in the fault handler
	//push   %rbp
	//mov    %rsp,%rbp
	//Increment %rip properly by 4 bytes
	regs->entry_rip+=4;
	regs->entry_rsp-=8;
	*(u64 *)(regs->entry_rsp) = regs->rbp;
	regs->rbp = regs->entry_rsp;

	//Finding trace buffer for function ftrace
	struct file *trace_buff = current->files[func->fd];

	//Entering value of normal ftrace
	*(u64 *)(trace_buff->trace_buffer->buff + trace_buff->trace_buffer->write_offset+sizeof(u64)) = func->faddr;
	if(func->num_args>=1) *(u64 *)(trace_buff->trace_buffer->buff + trace_buff->trace_buffer->write_offset+2*sizeof(u64)) = regs->rdi;
	if(func->num_args>=2) *(u64 *)(trace_buff->trace_buffer->buff +trace_buff->trace_buffer->write_offset+3*sizeof(u64)) = regs->rsi;
	if(func->num_args>=3) *(u64 *)(trace_buff->trace_buffer->buff +trace_buff->trace_buffer->write_offset+4*sizeof(u64)) = regs->rdx;
	if(func->num_args>=4) *(u64 *)(trace_buff->trace_buffer->buff +trace_buff->trace_buffer->write_offset+5*sizeof(u64)) = regs->rcx;
	if(func->num_args>=5) *(u64 *)(trace_buff->trace_buffer->buff +trace_buff->trace_buffer->write_offset+6*sizeof(u64)) = regs->r8;
    
	

	
    //Capture information if back_trace is enabled
	u64 backtrace_size=0;
	if(func->capture_backtrace == 1){
		//address of first instruction always the address of function?
		*(u64 *)(trace_buff->trace_buffer->buff + trace_buff->trace_buffer->write_offset + (2+func->num_args)*sizeof(u64) + backtrace_size)
		   = func->faddr;
		backtrace_size+=sizeof(u64);
		u64 curr_rbp = regs->rbp;
		u64 return_add = *(u64 *)(curr_rbp + sizeof(u64));
		while(return_add != END_ADDR){
			*(u64 *)(trace_buff->trace_buffer->buff + trace_buff->trace_buffer->write_offset + (2+func->num_args)*sizeof(u64) + backtrace_size)
		   = return_add;
		   backtrace_size+=sizeof(u64);
		   curr_rbp = *(u64 *)(curr_rbp);
		   return_add = *(u64 *)(curr_rbp + sizeof(u64));
		}
	}

	*(u64 *)(trace_buff->trace_buffer->buff +trace_buff->trace_buffer->write_offset) = (1+func->num_args+backtrace_size/sizeof(u64));
    
	//To add term of backtrace size
	trace_buff->trace_buffer->write_offset = (trace_buff->trace_buffer->write_offset + (2+ func->num_args)*sizeof(u64) + backtrace_size)%TRACE_BUFFER_MAX_SIZE;
	trace_buff->trace_buffer->space = trace_buff->trace_buffer->space - (2+ func->num_args)*sizeof(u64) - backtrace_size;


	//Arg1: rdi
	//Arg2: rsi
	//Arg3: rdx
	//Arg4: rcx
	//Arg5: r8

	//printk("Executed fault handler!\n");
    return 0;
}


int sys_read_ftrace(struct file *filep, char *buff, u64 count)
{
	if(filep == NULL) return -EINVAL;
	if(filep->type!=TRACE_BUFFER) return -EINVAL;
	if(buff == NULL) return -EINVAL;
	s32 size_read = 0;
	for(u64 i=0;i<count;i++){
		if(filep->trace_buffer->space == 4096 ) break;
		u64 j = *(u64 *)(filep->trace_buffer->buff + filep->trace_buffer->read_offset);
		for(int k=0;k<j;k++){
			*(u64 *)(buff+size_read) = *(u64 *)(filep->trace_buffer->buff+filep->trace_buffer->read_offset+sizeof(u64)*(k+1));
			size_read+=sizeof(u64);
		}
		filep->trace_buffer->read_offset = (filep->trace_buffer->read_offset + (j+1)*sizeof(u64)) % TRACE_BUFFER_MAX_SIZE;
		filep->trace_buffer->space = filep->trace_buffer->space + (j+1)*sizeof(u64);
	}
    return size_read;
}


