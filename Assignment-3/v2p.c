#include <types.h>
#include <mmap.h>
#include <fork.h>
#include <v2p.h>
#include <page.h>

/* 
 * You may define macros and other helper functions here
 * You must not declare and use any static/global variables 
 * */

void free_pte(struct exec_context *current, u64 start_addr, u64 end_addr){
    //Loop for all 4 KB pages from start_addr to end_addr
    while(start_addr != end_addr){

        //Store value of pgd and check if the correct pud page exists
        u64* pgd = (u64 *)osmap(current->pgd);
        if((*(pgd + (start_addr>>39)) & 0x1) == 0) return;

        //Store value of pud and check if correct pmd page exists
        u64* pud = (u64 *)osmap((*(pgd + (start_addr>>39)) >> 12));
        if((*(pud + ((start_addr>>30) & 0x1FF)) & 0x1) == 0) return;

        //Store value of pmd and check if correct pte page exists
        u64* pmd = (u64 *)osmap((*(pud + ((start_addr>>30) & 0x1FF)) >> 12));
        if((*(pmd + ((start_addr>>21) & 0x1FF)) & 0x1 )== 0) return;

        //Store value of pte and finally check if the page is allocated
        u64* pte = (u64 *)osmap((*(pmd + ((start_addr>>21) & 0x1FF)) >> 12));
        if((*(pte + ((start_addr>>12) & 0x1FF)) & 0x1) == 0) return;

        //Free the PFN if it exists
        u32 pfn = *(pte + ((start_addr>>12) & 0x1FF))>>12;
        if(get_pfn_refcount(pfn) != 1){ //Dont free PFN if there are multiple references to it
            put_pfn(pfn);
            start_addr = start_addr + 4096;
            *(pte + ((start_addr>>12) & 0x1FF)) = 0;
            continue;
        }
        //printk("Value of pfn: %x\n", pfn);
        *(pte + ((start_addr>>12) & 0x1FF)) = 0;
        put_pfn(pfn);
        //printk("Refcount is: %d\n", get_pfn_refcount(pfn));
        os_pfn_free(USER_REG, pfn);
        //printk("Freed pfn: %d\n", pfn);
        //*(pte + ((start_addr>>12) & 0x1FF)) = 0;

        start_addr = start_addr + 4096;
    }
}

void change_pte(struct exec_context* current, u64 start_addr, u64 end_addr, u32 access_flags){
    //Loop for all 4 KB pages from start_addr to end_addr
    while(start_addr != end_addr){
        //check present bit for all :- Code to be written yet
        //printk("Called change_pte\n");

        //Store value of pgd and check if the correct pud page exists
        u64* pgd = (u64 *)osmap(current->pgd);
        if((*(pgd + (start_addr>>39)) & 0x1) == 0) return;

        //Store value of pud and check if correct pmd page exists
        u64* pud = (u64 *)osmap((*(pgd + (start_addr>>39)) >> 12));
        if((*(pud + ((start_addr>>30) & 0x1FF)) & 0x1) == 0) return;

        //Store value of pmd and check if correct pte page exists
        u64* pmd = (u64 *)osmap((*(pud + ((start_addr>>30) & 0x1FF)) >> 12));
        if((*(pmd + ((start_addr>>21) & 0x1FF)) & 0x1 )== 0) return;

        //Store value of pte and finally check if the page is allocated
        u64* pte = (u64 *)osmap((*(pmd + ((start_addr>>21) & 0x1FF)) >> 12));
        if((*(pte + ((start_addr>>12) & 0x1FF)) & 0x1) == 0) return;
        //u64* pte_end = (*(pmd + ((end_addr>>12) & 0x1FF)) >> 12)<<12;
        //return (pte+((start_addr>>12)&0x1FF));
        //printk("Value of pte1: %x\n", *(pte + ((start_addr>>12) & 0x1FF)));
        //*(pte + ((start_addr>>12) & 0x1FF)) = (*(pte + ((start_addr>>12) & 0x1FF)) >> 12 ) << 12;

        u32 pfn = *(pte + ((start_addr>>12) & 0x1FF))>>12;
        if(get_pfn_refcount(pfn) != 1){//If there are multiple references to the PFN, dont need to change the access flags
            start_addr = start_addr + 4096;
            continue;   
        }
        //Change access flags as requested
        if(access_flags == PROT_READ){
            *(pte + ((start_addr>>12) & 0x1FF)) = *(pte + ((start_addr>>12) & 0x1FF)) & 0xFFFFFFFFFFFFFFF7;
        }
        else if(access_flags == (PROT_READ | PROT_WRITE)){
            *(pte + ((start_addr>>12) & 0x1FF)) = *(pte + ((start_addr>>12) & 0x1FF)) | 0x8;
        }
        start_addr = start_addr + 4096;
    }
    return;
}
/**
 * mprotect System call Implementation.
 */
long vm_area_mprotect(struct exec_context *current, u64 addr, int length, int prot)
{
    //Page align the length
    if(length%4096 != 0)
        length = (length/4096 + 1)*4096;
    
    struct vm_area* prev = current->vm_area;
    struct vm_area* curr = current->vm_area->vm_next;

    //Finding the first vm_area which satisfies the condition
    while(curr!=NULL){
        if(curr->vm_start == addr){
            if(prev!=current->vm_area){
                if(prev->vm_end == curr->vm_start && prev->access_flags == prot){
                    if(addr+length >= curr->vm_end){
                        //printk("Here %x\n", curr->vm_start);
                        change_pte(current, curr->vm_start, curr->vm_end, prot);
                        prev->vm_end = curr->vm_end;
                        prev->vm_next = curr->vm_next;
                        os_free(curr, sizeof(struct vm_area));
                        stats->num_vm_area--; //The curr vm_area is now deleted
                        //curr = prev->vm_next;
                        curr = prev;
                    }
                    // else if(addr+length == curr->vm_end){
                    //     change_pte(current, curr->vm_start, curr->vm_end, prot);
                    //     prev->vm_end = curr->vm_end;
                    //     prev->vm_next = curr;
                    //     curr->vm_start = addr+length;
                    // }
                    else{
                        prev->vm_end = addr+length;
                        curr->vm_start = addr+length;
                        change_pte(current, addr, addr+length, prot);
                        return 1;
                    }
                }
            }
            break;
        }
        else if(curr->vm_start < addr && curr->vm_end > addr){
            if(curr->access_flags == prot){
                break;
            }
            else{
                struct vm_area* new = (struct vm_area*)os_alloc(sizeof(struct vm_area));
                new->vm_start = addr;
                new->vm_end = curr->vm_end;
                new->access_flags = curr->access_flags;
                new->vm_next = curr->vm_next;
                stats->num_vm_area++;

                curr->vm_next = new;
                curr->vm_end = addr;
                curr = new;
                break;
            }
        }
        else if(addr >= prev->vm_end && addr <= curr->vm_start){
           break;
        }
        else{
            prev = curr;
            curr = curr->vm_next;
        }
    }

    struct vm_area* prev_t=prev;
    //Changing prot values 
    while(curr!=NULL){
        //printk("Value of curr: %x\n",curr->vm_start);
        if(addr+length == curr->vm_start){
            //curr->access_flags = prot;
            //change_pte(current, curr->vm_start, curr->vm_end, prot);
            if(prev_t->vm_end == curr->vm_start && prev_t->access_flags == curr->access_flags && prev_t != prev){
                prev_t->vm_end = curr->vm_end;
                prev_t->vm_next = curr->vm_next;
                os_free(curr, sizeof(struct vm_area));
                stats->num_vm_area--;
            }
            break;
        }
        else if(addr+length > curr->vm_start && addr+length < curr->vm_end){
            if(curr->access_flags == prot) break;
            else{
                struct vm_area* new = (struct vm_area*)os_alloc(sizeof(struct vm_area));
                new->vm_start = addr+length;
                new->vm_end = curr->vm_end;
                new->access_flags = curr->access_flags;
                new->vm_next = curr->vm_next;
                stats->num_vm_area++;

                curr->access_flags = prot;
                curr->vm_end = addr+length;
                curr->vm_next = new;
                change_pte(current, curr->vm_start, curr->vm_end, prot);
                // u64* pgd = current->pgd << 12;
                // u64* pud = (*(pgd + (curr->vm_start>>39)) >> 12)<<12;
                // u64* pmd = (*(pud + ((curr->vm_start>>30) & 0x1FF)) >> 12)<<12;
                // u64* pte = (*(pmd + ((curr->vm_start>>21) & 0x1FF)) >> 12)<<12;
                //printk("Value of pte4: %x\n", *(pte + ((curr->vm_start>>12) & 0x1FF)));
                break;
            }
            //printk("Value of next->vm_start :%x\n",next->vm_start);
            break;
        }
        else if(addr+length == curr->vm_end){
            curr->access_flags = prot;
            change_pte(current, curr->vm_start, curr->vm_end, prot);
            if(curr->vm_next!=NULL){
                if(curr->vm_end == curr->vm_next->vm_start && curr->vm_next->access_flags == prot){
                    curr->vm_end = curr->vm_next->vm_end;
                    curr->vm_next = curr->vm_next->vm_next;
                    stats->num_vm_area--; //Joint
                }
            }
            if(prev_t!=prev && prev_t->vm_end == curr->vm_start && prev_t->access_flags == curr->access_flags){
                prev_t->vm_end = curr->vm_end;
                prev_t->vm_next = curr->vm_next;
                os_free(curr, sizeof(struct vm_area));
                stats->num_vm_area--;
            }
            break;
        }
        else{
            curr->access_flags = prot;
            change_pte(current, curr->vm_start, curr->vm_end, prot);

            if(prev_t!=prev && prev_t->vm_end == curr->vm_start && prev_t->access_flags == curr->access_flags){
                prev_t->vm_end = curr->vm_end;
                prev_t->vm_next = curr->vm_next;
                os_free(curr, sizeof(struct vm_area));
                stats->num_vm_area--;
            }
            
            curr = curr->vm_next;
            prev_t=prev_t->vm_next;
        }
    }
    struct vm_area* temp_curr=prev->vm_next;
    struct vm_area* temp_prev=prev;
    while(temp_curr!=curr->vm_next){
        if(temp_prev->vm_end == temp_curr->vm_start && temp_prev->access_flags == temp_curr->access_flags){
            struct vm_area* to_free = temp_curr;
            temp_prev->vm_end = temp_curr->vm_end;
            temp_prev->vm_next = temp_curr->vm_next;
            //temp_prev = temp_curr;
            temp_curr = temp_prev->vm_next;
            os_free(to_free, sizeof(struct vm_area));
            stats->num_vm_area--;
            //temp_curr = prev->vm_next;
        }
        else{
            temp_prev = temp_curr;
            temp_curr = temp_curr->vm_next;
        }
    }
    //Current implementation to flush the TLB for the current process
    //printk("Value of pgd:%x\n", current->pgd);
    asm volatile ("mov %cr3, %rax \n"
                  "mov %rax, %cr3");
    //asm volatile ("mov %rax, %cr3");
    //printk("Value of pgd:%x\n", current->pgd);
    return 0;
}

/**
 * mmap system call implementation.
 */
long vm_area_map(struct exec_context *current, u64 addr, int length, int prot, int flags)
{
    /*
    Vm area count not working - Update itself ki I have to write the code for it
    Keep track that vm_area does not exceed mmap_end_area?
    Error handling:-
    1. Check if entire address range lies between MMAP_AREA_START and MMAP_AREA_END
    */
    //Errors
    if(flags!=0 && flags!=MAP_FIXED) return -1;
    if(prot!=PROT_READ && prot != (PROT_READ | PROT_WRITE)) return -1;
    //Correct length to a multiple of 4KB just greater than the length needed
    if(length%4096 != 0)
    length = (length/4096 + 1)*4096;


    /*Cases needed to be evaluated :-
      1. If hint Addr is NULL
      2. If hint addr is NUll and flag == MAP_FIXED return error
      3. If hint addr is not NULL 
      4. If hint addr is not NULL and flag == MAP_FIXED

      prot cases needed to be evaluated in each accordingly
    */ 

    //Initialize the NULL vm_area with the dummy on first contact
    //    printk("Value :%x\n",current->vm_area->vm_end);
    if(current->vm_area == NULL){ //Memory allocation for vm_area using os_alloc / os_page_alloc
        struct vm_area *dummy = (struct vm_area *)os_alloc(sizeof(struct vm_area));
        dummy->vm_start = MMAP_AREA_START;
        dummy->vm_end = MMAP_AREA_START + 4*1024;
        dummy->access_flags = 0;
        dummy->vm_next = NULL;
        current->vm_area = dummy;
        stats->num_vm_area++;
    }

    //Hint addr is NULL
    if(addr == NULL){
        //s32 found = 0;
        //If flag == MAP_FIXED, return error
        if(flags == MAP_FIXED) return -1;

        struct vm_area *temp = current->vm_area;
        while(temp != NULL && temp->vm_next != NULL){
            if(temp->vm_next->vm_start - temp->vm_end >= length){
                if(temp->access_flags != prot){ //If the access flags are different
                    //Initialize new vm_area
                    struct vm_area *new = (struct vm_area *)os_alloc(sizeof(struct vm_area));
                    new->vm_start = temp->vm_end;
                    new->vm_end = temp->vm_end + length;
                    new->access_flags = prot;
                    new->vm_next = temp->vm_next;
                    temp->vm_next = new;
                    stats->num_vm_area++;

                    //merge with the temp->vm_next if protections match and vm_end == vm_next->vm_start
                    if(new->vm_end == new->vm_next->vm_start && new->access_flags == new->vm_next->access_flags){
                        struct vm_area *to_free = new->vm_next;
                        new->vm_end = new->vm_next->vm_end;
                        new->vm_next = new->vm_next->vm_next;
                        os_free(to_free, sizeof(struct vm_area));
                        to_free=NULL;
                        stats->num_vm_area--;
                    }

                    return new->vm_start;
                }
                else{ //If access flags are same, just extend the vm_area
                    unsigned long ret = temp->vm_end;
                    temp->vm_end = temp->vm_end + length;
                    if(temp->vm_end == temp->vm_next->vm_start && temp->access_flags == temp->vm_next->access_flags){
                        struct vm_area *to_free = temp->vm_next;
                        temp->vm_end = temp->vm_next->vm_end;
                        temp->vm_next = temp->vm_next->vm_next;
                        os_free(to_free, sizeof(struct vm_area));
                        to_free=NULL;
                        stats->num_vm_area--;
                    }
                    return ret;
                }
                // found = 1;
                //No need for break honestly lol
                break;
            }
            temp=temp->vm_next;
        }

        //No large enough free area available, so create a new vm_area at the end
        if(temp->access_flags ==  prot){
            //Just extend the vm_area if the access flags are same
            unsigned long ret = temp->vm_end;
            temp->vm_end = temp->vm_end + length;
            return ret;
        }
        else{
            //Create a new area otherwise
            struct vm_area *new = (struct vm_area *)os_alloc(sizeof(struct vm_area));
            new->vm_start = temp->vm_end;
            new->vm_end = temp->vm_end + length;
            new->access_flags = prot;
            new->vm_next = NULL;
            temp->vm_next = new;
            stats->num_vm_area++;
            return new->vm_start;
        }
   }
   else{//If hint addr is not NULL
        
        struct vm_area* temp = current->vm_area;
        while(temp!=NULL && temp->vm_next!=NULL){
            // s32 found = 0;
            if(addr>=temp->vm_end && addr+length<=temp->vm_next->vm_start){
               //found a place to create a new vm_area
               if(addr == temp->vm_end && prot == temp->access_flags){ //extend temp vm_area
                    temp->vm_end = temp->vm_end + length;

                    //Check if new temp_vm area now aligns with the next vm_area in line
                    if(temp->vm_end == temp->vm_next->vm_start && temp->access_flags == temp->vm_next->access_flags){
                        struct vm_area *to_free = temp->vm_next;
                        temp->vm_end = temp->vm_next->vm_end;
                        temp->vm_next = temp->vm_next->vm_next;
                        os_free(to_free, sizeof(struct vm_area));
                        to_free=NULL;
                        stats->num_vm_area--;
                    }

                    return (temp->vm_end - length);
                }
                else if(addr + length == temp->vm_next->vm_start && prot == temp->vm_next->access_flags){
                    //Extend the temp->vm_next vm_area
                    temp->vm_next->vm_start = addr;
                    return addr;
                }
                else{
                    //Lies completely disjoint, right in the middle
                    struct vm_area *new = (struct vm_area *)os_alloc(sizeof(struct vm_area));
                    new->vm_start = addr;
                    new->vm_end = addr + length;
                    new->access_flags = prot;
                    new->vm_next = temp->vm_next;
                    temp->vm_next = new;
                    stats->num_vm_area++;
                    return addr;
                }
            }
            temp=temp->vm_next;
        }


        //Else continue temp->vm_next
        //If no place found, check MAP_FIXED :- If found, return error, else create a new vm_area? where?, redo the while loop for addr = NULL
        
        //Checking beyond last vm_area
        if(addr == temp->vm_end){
            temp->vm_end = temp->vm_end + length;
            return temp->vm_end -length;
        }
        else if(addr > temp->vm_end){
            struct vm_area* new= (struct vm_area*)os_alloc(sizeof(struct vm_area));
            new->vm_start = addr;
            new->vm_end = addr+length;
            new->access_flags = prot;
            temp->vm_next = new;
            stats->num_vm_area++;
            return addr;
        }
        else if(flags == MAP_FIXED){ //If not found any place yet and MAP_FIXED is set
            return -EINVAL;
        }
        else{ //Just the same code as hint addr == NULL
            struct vm_area *temp = current->vm_area;
        while(temp != NULL && temp->vm_next != NULL){
            if(temp->vm_next->vm_start - temp->vm_end >= length){
                if(temp->access_flags != prot){ //If the access flags are different
                    struct vm_area *new = (struct vm_area *)os_alloc(sizeof(struct vm_area));
                    new->vm_start = temp->vm_end;
                    new->vm_end = temp->vm_end + length;
                    new->access_flags = prot;
                    new->vm_next = temp->vm_next;
                    temp->vm_next = new;
                    stats->num_vm_area++;
                    //merge with the temp->vm_next?
                    if(new->vm_end == new->vm_next->vm_start && new->access_flags == new->vm_next->access_flags){
                        struct vm_area *to_free = new->vm_next;
                        new->vm_end = new->vm_next->vm_end;
                        new->vm_next = new->vm_next->vm_next;
                        os_free(to_free, sizeof(struct vm_area));
                        to_free=NULL;
                        stats->num_vm_area--;
                    }
                    return new->vm_start;
                }
                else{ //If access flags are same, just extend the vm_area
                    temp->vm_end = temp->vm_end + length;
                    if(temp->vm_end == temp->vm_next->vm_start && temp->access_flags == temp->vm_next->access_flags){
                        struct vm_area *to_free = temp->vm_next;
                        temp->vm_end = temp->vm_next->vm_end;
                        temp->vm_next = temp->vm_next->vm_next;
                        os_free(to_free, sizeof(struct vm_area));
                        to_free=NULL;
                        stats->num_vm_area--;
                    }
                    return temp->vm_end - length;
                }
                // found = 1;
                break;
            }
            temp=temp->vm_next;
        }
        if(temp->access_flags ==  prot){
            temp->vm_end = temp->vm_end + length;
            return temp->vm_end - length;
        }
        else{
            struct vm_area *new = (struct vm_area *)os_alloc(sizeof(struct vm_area));
            new->vm_start = temp->vm_end;
            new->vm_end = temp->vm_end + length;
            new->access_flags = prot;
            new->vm_next = NULL;
            temp->vm_next = new;
            stats->num_vm_area++;
            return new->vm_start;
        }
        }
    }
    return -EINVAL;
}

/**
 * munmap system call implemenations
 */

long vm_area_unmap(struct exec_context *current, u64 addr, int length)
{
    /* To do:-
    1. Error handling
    2. For lazy allocation - do you free a higher level if all lower levels are free? -->No way to check though!
    */

    //Correcting length 
    if(length%4096 != 0)
    length = (length/4096 + 1)*4096;

    //The three pointers needed for the final linking of vm_area list
    struct vm_area *prev = current->vm_area;
    struct vm_area *curr = current->vm_area->vm_next;
    struct vm_area *next;

    //First while loop to find the prev vm_area to addr passed
    while(curr!=NULL){
        if(curr->vm_start == addr){
            break;
        }
        else if(curr->vm_start < addr && curr->vm_end > addr){
            prev = curr;
            // prev->vm_end = addr;
            struct vm_area *new = (struct vm_area *)os_alloc(sizeof(struct vm_area));
            new->vm_start = addr;
            new->vm_end = curr->vm_end;
            new->access_flags = curr->access_flags;
            new->vm_next = curr->vm_next;

            prev->vm_next = new;
            prev->vm_end = addr;

            //printk("Value of prev->vm_next:%x\n", prev->vm_next);
            curr = prev->vm_next;
            stats->num_vm_area++;
            //more code??
            break;
        }
        else if(addr >= prev->vm_end && addr <= curr->vm_start){
           break;
        }
        else{
            prev = curr;
            curr = curr->vm_next;
        }
    }
    // printk("Value of prev->vm_start :%x\n",prev->vm_start);
    // printk("Value of curr: %x\n",curr->vm_start);
    //Second while loop to find the next vm_area to addr passed
    while(curr!=NULL){
        //printk("Value of curr: %x\n",curr->vm_start);
        if(addr+length <= curr->vm_start){
            next = curr;
            break;
        }
        else if(addr+length > curr->vm_start && addr+length < curr->vm_end){
            free_pte(current, curr->vm_start, addr+length);
            next = curr;
            next->vm_start = addr+length;
            //printk("Value of next->vm_start :%x\n",next->vm_start);
            break;
        }
        else if(addr+length == curr->vm_end){
            next = curr->vm_next;
            //free physical pages in curr here
            free_pte(current, curr->vm_start, curr->vm_end);
            os_free(curr, sizeof(struct vm_area));
            stats->num_vm_area--;
            break;
        }
        else{
            if(curr->vm_next == NULL){
                next = NULL;
                free_pte(current, curr->vm_start, curr->vm_end);
                os_free(curr, sizeof(struct vm_area));
                stats->num_vm_area--;
                break;
                //No need to continue the loop further right?
            }
            else{
                if(curr->vm_next->vm_start>=addr+length){
                    next = curr->vm_next;
                    //free physical pages in curr here
                    free_pte(current, curr->vm_start, curr->vm_end);
                    os_free(curr, sizeof(struct vm_area));
                    stats->num_vm_area--;
                    break;
                }
                else{
                    struct vm_area* to_free = curr;
                    curr = curr->vm_next;
                    //free pages in to_free here
                    free_pte(current, to_free->vm_start, to_free->vm_end);
                    os_free(to_free, sizeof(struct vm_area));
                    stats->num_vm_area--;
                }
            }
        }
    }
    //printk("Value of next->vm_start :%x\n",next->vm_start);

    //Link prev and next vm_areas
    prev->vm_next = next;

    //Current implementation to flush the TLB -->Merge them into one asm volatile
    // asm volatile ("mov %cr3, %rax");
    // asm volatile ("mov %rax, %cr3");
    asm volatile ("mov %cr3, %rax \n"
                  "mov %rax, %cr3");
    
    return 0;
}



/**
 * Function will invoked whenever there is page fault for an address in the vm area region
 * created using mmap
 */

long vm_area_pagefault(struct exec_context *current, u64 addr, int error_code)
{
    /*
    Possible error codes:-
    1. 0x1
    2. 0x2
    3. 0x3
    4. 0x4
    5. 0x5
    6. 0x6
    7. 0x7
    */
    //printk("Error Code: %d\n", error_code);
    struct vm_area* head = current->vm_area->vm_next;

    while(head!=NULL){
        if(addr >= head->vm_start && addr < head->vm_end) break;
        head = head->vm_next;
    }

    //VMA doesnt exist, should return Invalid Access -1
    if(head == NULL) return -1;

    //Find PGD, PUD, PMD, and PTE in order
    //Finding the PGD
    //Check protection for PTE entries should be 0x9?
    u64* pgd = (u64 *)osmap(current->pgd) ;
    //printk("Value of pgd: %x\n", pgd);
    //printk("Value of entry stored at pgd: %x\n", *(pgd + (addr>>39)));
    if((*(pgd + (addr>>39)) & 0x1) == 0){ //PGD entry not alloted
        //u32 test = os_pfn_alloc(OS_PT_REG);
        //printk("Value of 32\n");
        *(pgd +(addr>>39)) = os_pfn_alloc(OS_PT_REG) << 12;
        *(pgd +(addr>>39)) = *(pgd +(addr>>39)) | 25;
    }
    //printk("Value of entry stored at pgd: %x\n", *(pgd + (addr>>39)));
    u64* pud = (u64* )osmap((*(pgd + (addr>>39)) >> 12));
    //printk("Value of pud: %x\n", pud);
    if((*(pud + ((addr>>30) & 0x1FF)) & 0x1) == 0){ //PUD entry not alloted
        *(pud +((addr>>30) & 0x1FF)) = os_pfn_alloc(OS_PT_REG)<<12;
        *(pud +((addr>>30) & 0x1FF)) = *(pud +((addr>>30) & 0x1FF)) | 25;
    }

    u64* pmd = (u64 *)osmap((*(pud + ((addr>>30) & 0x1FF)) >> 12));
    //printk("Value of pmd: %x\n", pmd);
    if((*(pmd + ((addr>>21) & 0x1FF)) & 0x1 )== 0){ //PMD entry not alloted
        u32 test = os_pfn_alloc(OS_PT_REG);
        // printk("Value of 32:%d\n", test);
        *(pmd + ((addr>>21) & 0x1FF)) = test<<12;
        *(pmd + ((addr>>21) & 0x1FF)) = *(pmd + ((addr>>21) & 0x1FF)) | 25;
    }
    u64* pte = (u64 *)osmap((*(pmd + ((addr>>21) & 0x1FF)) >> 12));
    //printk("Value of pte: %x\n", pte);
    // if(*(pte + ((addr>>12) & 0x1FF)) & 0x1 == 0){ //PTE entry not alloted
    //     *(pte + ((addr>>12) & 0x1FF)) = os_pfn_alloc(OS_PT_REG)<<12;
    //     *(pte + ((addr>>12) & 0x1FF)) = *(pte + ((addr>>12) & 0x1FF)) | 0x9;
    // }

    //Finally check if page is in physical memory and allot memory accordingly
    // printk("Error Code:- %d\n", error_code);
    if(error_code == 0x4){
        //valid access
        //Check if page is in physical memory
        *(pte +((addr>>12)&0x1FF)) = os_pfn_alloc(USER_REG)<<12;
        //get_pfn(*(pte +((addr>>12)&0x1FF))>>12);
        if(head->access_flags == PROT_READ)
        *(pte +((addr>>12)&0x1FF)) = *(pte +((addr>>12)&0x1FF)) | 17;
        else if(head->access_flags == (PROT_READ|PROT_WRITE))
        *(pte +((addr>>12)&0x1FF)) = *(pte +((addr>>12)&0x1FF)) | 25;
    }
    else if(error_code == 0x6){
        //printk("Head access flags :- %d\n", head->access_flags);
        if(head->access_flags == PROT_READ) return -1;
        else{
            //printk("In error_code  = 0x6\n");
            u32 test = os_pfn_alloc(USER_REG);
            //get_pfn(test);
            //printk("Value of test: %x\n", get_pfn_refcount(test));
            *(pte +((addr>>12)&0x1FF)) = test<<12;
            *(pte +((addr>>12)&0x1FF)) = *(pte +((addr>>12)&0x1FF)) | 25;
            //printk("Value of entry stored: %x\n", *(pte +((addr>>12)&0x1FF)));
            //printk("osmap: %x\n", );
        }
    }
    else if(error_code == 0x7){
        //printk("head address: %x\n", head->vm_start);
        //printk("head access: %d\n", head->access_flags);
        if(head->access_flags == PROT_READ) return -1;
        else //cow implementation
        {
            //printk("Error code = 0x7\n");
            handle_cow_fault(current, addr, head->access_flags);
        }
    }
    return 1;
}

/**
 * cfork system call implemenations
 * The parent returns the pid of child process. The return path of
 * the child process is handled separately through the calls at the 
 * end of this function (e.g., setup_child_context etc.)
 */

long do_cfork(){
    u32 pid;
    struct exec_context *new_ctx = get_new_ctx();
    struct exec_context *ctx = get_current_ctx();
     /* Do not modify above lines
     * 
     * */   
     /*--------------------- Your code [start]---------------*/
     //Duplicate all the exec content from parent to child
    //  printk("Value of ctx->pid: %d\n", new_ctx->pid);
    //  printk("Value of pid: %d\n", pid);
    //printk("RIP position: %x\n", ctx->regs.entry_rip);
     pid = new_ctx->pid;
     //new_ctx->ppid = ctx->ppid;
     //printk("New ctx type: %d\n", new_ctx->type);
     new_ctx->type = ctx->type;
     new_ctx->state = ctx->state;
     new_ctx->used_mem = ctx->used_mem;
     //new_ctx->pgd = ctx->pgd;
     new_ctx->os_rsp = ctx->os_rsp;
     new_ctx->os_stack_pfn = ctx->os_stack_pfn;
     for(u32 i=0;i<MAX_MM_SEGS;i++){ 
        new_ctx->mms[i] = ctx->mms[i]; //Can I do so easily?? They both now have the same pointers, should they though?
     }
     //new_ctx->vm_area = ctx->vm_area; --> Cant do it so easily, need to make a seperate copy for the child process
     if(ctx->vm_area != NULL){
        new_ctx->vm_area = (struct vm_area*)os_alloc(sizeof(struct vm_area));
        new_ctx->vm_area->vm_start = ctx->vm_area->vm_start;
        new_ctx->vm_area->vm_end = ctx->vm_area->vm_end;
        new_ctx->vm_area->access_flags = ctx->vm_area->access_flags;
        new_ctx->vm_area->vm_next = NULL;

        struct vm_area* prev = new_ctx->vm_area;

        struct vm_area* temp = ctx->vm_area->vm_next;
        while(temp!=NULL){
           struct vm_area* new = (struct vm_area*)os_alloc(sizeof(struct vm_area));
           new->vm_start = temp->vm_start;
           new->vm_end = temp->vm_end;
           new->access_flags = temp->access_flags;
           new->vm_next = NULL;

           prev->vm_next = new;
           prev = prev->vm_next;
           temp = temp->vm_next;
        }
     }
     else new_ctx->vm_area = NULL;
     // Basically dont copy the pointers directly.... most of the time
     for(u32 i=0;i<CNAME_MAX;i++) new_ctx->name[i] = ctx->name[i];
     new_ctx->regs = ctx->regs;
    //  printk("Value of rip here: %x\n", ctx->regs.rax);
    //  new_ctx->regs.entry_rip+=100;
    //  printk("Value of rip here: %x\n", new_ctx->regs.rax);
     new_ctx->pending_signal_bitmap = new_ctx->pending_signal_bitmap;
     
     for(u32 i=0;i<MAX_SIGNALS;i++) new_ctx->sighandlers[i] = ctx->sighandlers[i];
     new_ctx->ticks_to_sleep = ctx->ticks_to_sleep;
     new_ctx->alarm_config_time = ctx->alarm_config_time;
     new_ctx->ticks_to_alarm = ctx->ticks_to_alarm;

     //Parent and child point to the same inode of open files so not a problem
     for(u32 i=0;i<MAX_OPEN_FILES;i++) new_ctx->files[i] = ctx->files[i];  //Increase ref count of the file?? 

     //Should we copy parent threads to child threads??
     new_ctx->ctx_threads = ctx->ctx_threads;

     //Update the ppid of the child process
     new_ctx->ppid = ctx->pid;

     //Build a new page table for the child process
     new_ctx->pgd = os_pfn_alloc(OS_PT_REG);
     
     //MM segments keliye
     for(u32 i=0;i<MAX_MM_SEGS;i++){
        //printk("Copying mm_area :%d\n", i);
        unsigned long start_addr=new_ctx->mms[i].start;
        //printk("Value of mm_start: %x %x\n", new_ctx->mms[i].start, ctx->mms[i].start);
        unsigned long end_addr;
        if(i!=3) end_addr = new_ctx->mms[i].next_free;
        else end_addr = new_ctx->mms[i].end;
        while(start_addr<end_addr){
            //u64 pfn_alloc = page_alloced(ctx, start_addr);

            u64* pgd = (u64 *)osmap(ctx->pgd);
            if((*(pgd + (start_addr>>39)) & 0x1) == 0){
                start_addr+=4096;
                continue;
            }
            //Store value of pud and check if correct pmd page exists
            u64* pud = (u64 *)osmap((*(pgd + (start_addr>>39)) >> 12));
            if((*(pud + ((start_addr>>30) & 0x1FF)) & 0x1) == 0){
                start_addr+=4096;
                continue;
            }
            //Store value of pmd and check if correct pte page exists
            u64* pmd = (u64 *)osmap((*(pud + ((start_addr>>30) & 0x1FF)) >> 12));
            if((*(pmd + ((start_addr>>21) & 0x1FF)) & 0x1 )== 0){
                start_addr+=4096;
                continue;
            }
            //Store value of pte and finally check if the page is allocated
            u64* pte = (u64 *)osmap((*(pmd + ((start_addr>>21) & 0x1FF)) >> 12));
            if((*(pte + ((start_addr>>12) & 0x1FF)) & 0x1) == 0){
                start_addr+=4096;
                continue;
            }

            u32 pfn = *(pte + ((start_addr>>12) & 0x1FF)) >> 12;
            get_pfn(pfn); //Increase ref count of that pfn by one

            *(pte + ((start_addr>>12) & 0x1FF)) = *(pte + ((start_addr>>12) & 0x1FF)) & 0xFFFFFFFFFFFFFFF7;

            //if page is alloced in parent, create all levels and make sure that same pfn is assigned in the last level
            //Increase the reference count of the PFN
            u64* n_pgd = (u64 *)osmap(new_ctx->pgd);
            if((*(n_pgd + (start_addr>>39)) & 0x1) == 0){ //PGD entry not alloted
                *(n_pgd +(start_addr>>39)) = os_pfn_alloc(OS_PT_REG) << 12;
                *(n_pgd +(start_addr>>39)) = *(n_pgd +(start_addr>>39)) | 25;
            }
            u64* n_pud = (u64* )osmap((*(n_pgd + (start_addr>>39)) >> 12));
            //printk("Value of pud: %x\n", n_pud);
            if((*(n_pud + ((start_addr>>30) & 0x1FF)) & 0x1) == 0){ //PUD entry not alloted
                *(n_pud +((start_addr>>30) & 0x1FF)) = os_pfn_alloc(OS_PT_REG)<<12;
                *(n_pud +((start_addr>>30) & 0x1FF)) = *(n_pud +((start_addr>>30) & 0x1FF)) | 25;
            }
            u64* n_pmd = (u64 *)osmap((*(n_pud + ((start_addr>>30) & 0x1FF)) >> 12));
            //printk("Value of pmd: %x\n", n_pmd);
            if((*(n_pmd + ((start_addr>>21) & 0x1FF)) & 0x1 )== 0){ //PMD entry not alloted
                u32 test = os_pfn_alloc(OS_PT_REG);
                // printk("Value of 32:%d\n", test);
                *(n_pmd + ((start_addr>>21) & 0x1FF)) = test<<12;
                *(n_pmd + ((start_addr>>21) & 0x1FF)) = *(n_pmd + ((start_addr>>21) & 0x1FF)) | 25;
            }
            u64* n_pte = (u64 *)osmap((*(n_pmd + ((start_addr>>21) & 0x1FF)) >> 12));
            *(n_pte +((start_addr>>12)&0x1FF)) = *(pte + ((start_addr>>12) & 0x1FF));
            //printk("Comparison : %x %x\n", *(n_pte +((start_addr>>12)&0x1FF)), *(pte +((start_addr>>12)&0x1FF)));
            start_addr = start_addr + 4096;         
        }        
     }
     //printk("copied mm_areas\n");
     //VM Area to be copied
     struct vm_area* temp = new_ctx->vm_area;
     //printk("Value of temp: %x\n", temp);
     while(temp){
        //printk("Copying vm_area: %x\n", temp->vm_start);
        unsigned long start_addr=temp->vm_start;
        unsigned long end_addr=temp->vm_end;
        while(start_addr<end_addr){
            //u64 pfn_alloc = page_alloced(ctx, start_addr);

            u64* pgd = (u64 *)osmap(ctx->pgd);
            if((*(pgd + (start_addr>>39)) & 0x1) == 0){
                start_addr+=4096;
                continue;
            }
            //Store value of pud and check if correct pmd page exists
            u64* pud = (u64 *)osmap((*(pgd + (start_addr>>39)) >> 12));
            if((*(pud + ((start_addr>>30) & 0x1FF)) & 0x1) == 0){
                start_addr+=4096;
                continue;
            }
            //Store value of pmd and check if correct pte page exists
            u64* pmd = (u64 *)osmap((*(pud + ((start_addr>>30) & 0x1FF)) >> 12));
            if((*(pmd + ((start_addr>>21) & 0x1FF)) & 0x1 )== 0){
                start_addr+=4096;
                continue;
            }
            //Store value of pte and finally check if the page is allocated
            u64* pte = (u64 *)osmap((*(pmd + ((start_addr>>21) & 0x1FF)) >> 12));
            if((*(pte + ((start_addr>>12) & 0x1FF)) & 0x1) == 0){
                start_addr+=4096;
                continue;
            }

            u32 pfn = *(pte + ((start_addr>>12) & 0x1FF)) >> 12;
            get_pfn(pfn); //Increase ref count of that pfn by one

            *(pte + ((start_addr>>12) & 0x1FF)) = *(pte + ((start_addr>>12) & 0x1FF)) & 0xFFFFFFFFFFFFFFF7;

            //if page is alloced in parent, create all levels and make sure that same pfn is assigned in the last level
            //Increase the reference count of the PFN
            u64* n_pgd = (u64 *)osmap(new_ctx->pgd);
            if((*(n_pgd + (start_addr>>39)) & 0x1) == 0){ //PGD entry not alloted
                *(n_pgd +(start_addr>>39)) = os_pfn_alloc(OS_PT_REG) << 12;
                *(n_pgd +(start_addr>>39)) = *(n_pgd +(start_addr>>39)) | 25;
            }
            u64* n_pud = (u64* )osmap((*(n_pgd + (start_addr>>39)) >> 12));
            // printk("Value of pud: %x\n", pud);
            if((*(n_pud + ((start_addr>>30) & 0x1FF)) & 0x1) == 0){ //PUD entry not alloted
                *(n_pud +((start_addr>>30) & 0x1FF)) = os_pfn_alloc(OS_PT_REG)<<12;
                *(n_pud +((start_addr>>30) & 0x1FF)) = *(n_pud +((start_addr>>30) & 0x1FF)) | 25;
            }
            u64* n_pmd = (u64 *)osmap((*(n_pud + ((start_addr>>30) & 0x1FF)) >> 12));
            // printk("Value of pmd: %x\n", pmd);
            if((*(n_pmd + ((start_addr>>21) & 0x1FF)) & 0x1 )== 0){ //PMD entry not alloted
                u32 test = os_pfn_alloc(OS_PT_REG);
                // printk("Value of 32:%d\n", test);
                *(n_pmd + ((start_addr>>21) & 0x1FF)) = test<<12;
                *(n_pmd + ((start_addr>>21) & 0x1FF)) = *(n_pmd + ((start_addr>>21) & 0x1FF)) | 25;
            }
            u64* n_pte = (u64 *)osmap((*(n_pmd + ((start_addr>>21) & 0x1FF)) >> 12));
            *(n_pte +((start_addr>>12)&0x1FF)) = *(pte + ((start_addr>>12) & 0x1FF));
            start_addr = start_addr + 4096;         
        }   
        temp=temp->vm_next;     
    }
    //printk("Finished copying VM_area\n");

     /*--------------------- Your code [end] ----------------*/
    
     /*
     * The remaining part must not be changed
     */
    copy_os_pts(ctx->pgd, new_ctx->pgd);
    do_file_fork(new_ctx);
    setup_child_context(new_ctx);
    return pid;
}



/* Cow fault handling, for the entire user address space
 * For address belonging to memory segments (i.e., stack, data) 
 * it is called when there is a CoW violation in these areas. 
 *
 * For vm areas, your fault handler 'vm_area_pagefault'
 * should invoke this function
 * */

long handle_cow_fault(struct exec_context *current, u64 vaddr, int access_flags)
{
    // printk("Inside handle cow fault %d\n", current->pid);
    // printk("Current instruction pointer: %x\n", current->regs.entry_rip);
    // printk("vaddr accessed: %x\n", vaddr);
    //Find PTE of accessed vaddr in current process
    u64* pgd = (u64 *)osmap(current->pgd);
    if((*(pgd + (vaddr>>39)) & 0x1) == 0) return -1;
    //Store value of pud and check if correct pmd page exists
    u64* pud = (u64 *)osmap((*(pgd + (vaddr>>39)) >> 12));
    if((*(pud + ((vaddr>>30) & 0x1FF)) & 0x1) == 0) return -1;
    //Store value of pmd and check if correct pte page exists
    u64* pmd = (u64 *)osmap((*(pud + ((vaddr>>30) & 0x1FF)) >> 12));
    if((*(pmd + ((vaddr>>21) & 0x1FF)) & 0x1 )== 0) return -1;
    //Store value of pte and finally check if the page is allocated
    u64* pte = (u64 *)osmap((*(pmd + ((vaddr>>21) & 0x1FF)) >> 12));
    if((*(pte + ((vaddr>>12) & 0x1FF)) & 0x1) == 0) return -1;

    //If the page is read only, allocate a new PFN and copy the shit
    u32 old_pfn = *(pte + ((vaddr>>12) & 0x1FF)) >> 12;
    //If reference count of pfn is already one -> No need to copy just change permissions according to access_flags
    if(get_pfn_refcount(old_pfn) == 1){
        //printk("Inside get_refocunt ==1 \n");
        if(access_flags == PROT_READ) return -1;
        else if(access_flags == (PROT_READ | PROT_WRITE)){
            *(pte + ((vaddr>>12) & 0x1FF)) = *(pte + ((vaddr>>12) & 0x1FF)) | 0x8;
        }
        return 1;
    }
    u32 new_pfn = os_pfn_alloc(USER_REG);
    //get_pfn(new_pfn);
    put_pfn(old_pfn);
    memcpy(osmap(new_pfn), osmap(old_pfn), 4096);
    //Copied into new pfn
    //Assign new pfn to the pte table entry
    u64 conditions = ( *(pte + ((vaddr>>12) & 0x1FF)) & 0xFFF ) | 0x8;
    *(pte + ((vaddr>>12) & 0x1FF)) = (new_pfn << 12) + conditions;
    //Decrease reference count of old_pfn by 1
    return 1;
}
