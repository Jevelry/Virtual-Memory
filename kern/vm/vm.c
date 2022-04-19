#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <addrspace.h>
#include <vm.h>
#include <machine/tlb.h>

/* Place your page table functions here */



void vm_bootstrap(void)
{
    /* Initialise any global components of your VM sub-system here.  
     *  
     * You may or may not need to add anything here depending what's
     * provided or required by the assignment spec.
     */
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
    (void) faulttype;
    (void) faultaddress;

    panic("vm_fault hasn't been written yet\n");

    // For Null
    if (faultaddress == 0){
        return EFAULT;
    }
    // VM_FAULT -> READONLY
    if (faulttype == VM_FAULT_READONLY){ 
        return EFAULT; 
    }

    // Mask and remove offset portion of virtual address

    // vaddr_t page_num = faultaddress && PAGE_FRAME;

    //Get processor's address space
    struct addrspace *address_space = proc_getas();
    if (adress_space == NULL) {
		return EFAULT;
	}
    // look up frame in page table
    paddr_t frame_addr = 0;
    vaddr_t first_table_num = faultaddress >> 21;
    vaddr_t second_table_num = (faultaddress >> 12) && 0x9;

    if (address_space -> page_table[first_table_num]) {
        if (address_space -> page_table[first_table_num][second_table_num]) {
            frame_addr = page_table[first_table_num];
        }
    }
    // If frame is not found
    else{
        
    }

    //Check if vaddress is in region


    // Create new frame and insert into page table



    // LOAD TLB
    return EFAULT;
}

/*
 * SMP-specific functions.  Unused in our UNSW configuration.
 */

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void)ts;
	panic("vm tried to do tlb shootdown?!\n");
}

