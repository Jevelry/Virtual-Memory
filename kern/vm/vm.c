#include <types.h>
#include <proc.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <thread.h>
#include <addrspace.h>
#include <vm.h>
#include <machine/tlb.h>

/* Place your page table functions here */

paddr_t lookup_frame(vaddr_t page_num, struct addrspace *as);
int lookup_region(vaddr_t page_num, struct addrspace *as);
int insert_entry(vaddr_t page_num, paddr_t frame_num, struct addrspace *as);

paddr_t lookup_frame(vaddr_t page_num, struct addrspace *as){
    vaddr_t first_table_num = (page_num >> 21);
    if (as->page_table[first_table_num] == NULL){ 
        return 0; 
    }
    vaddr_t second_table_num = (page_num << 11) >> 23; //mask for 9 bits
    if (as->page_table[first_table_num][second_table_num] == 0) {
        return 0;
    }
    return as->page_table[first_table_num][second_table_num];
       
    
}
int lookup_region(vaddr_t page_num, struct addrspace *as) {
    struct region *curr_region = as->region_head;
    while (curr_region != NULL) {
        if (page_num >= curr_region->base && page_num < curr_region->base + (curr_region->size * PAGE_SIZE)) {
            return 1;
        }
        curr_region = curr_region->next; 
    }
    return -1;
}

int insert_entry(vaddr_t page_num, paddr_t frame_num, struct addrspace *as) {
    vaddr_t first_table_num = (page_num >> 21);
    vaddr_t second_table_num = (page_num << 11) >> 23; //mask for 9 bits
    if (as->page_table[first_table_num] == NULL){
        as->page_table[first_table_num] = kmalloc(PT_SL_SIZE * sizeof(paddr_t));
        int i = 0;
        while (i < PT_SL_SIZE) {
            as->page_table[first_table_num][i] = 0;
            i++;
        }     
    }
    as->page_table[first_table_num][second_table_num] = frame_num;
    return 0;

}

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


    // For Null
    if (faultaddress == 0x0){
        return EFAULT;
    }
    // VM_FAULT -> READONLY
    if (faulttype == VM_FAULT_READONLY){ 
        return EFAULT;
    }

    // Mask and remove offset portion of virtual address

    vaddr_t page_num = faultaddress & PAGE_FRAME;


    //Get processor's address space
    struct addrspace *address_space = proc_getas();
    if (address_space == NULL) {
		return EFAULT;
	}
    // look up frame in page table
    paddr_t frame_address = lookup_frame(page_num, address_space);

    // If frame is not found
    if (frame_address == 0) {
        int res = lookup_region(page_num, address_space);
        if (res == -1) {
            return EFAULT;
        }
        //Allocate Frame, Zero-fill
        vaddr_t kernal_address = alloc_kpages(1);
        if (kernal_address == 0) { 
            return ENOMEM;
        }
        frame_address = KVADDR_TO_PADDR(kernal_address); 
        bzero((void *)kernal_address, PAGE_SIZE);
        res = insert_entry(page_num, frame_address, address_space);
        if (res){
            return EFAULT;
        }
    }

    
    uint32_t entry_hi;
    uint32_t entry_lo;
    // LOAD into TLB
    int spl = splhigh();
    
    // check region write permission
    // if (curr_region->flags & FLAG_W) {
    //     entry_lo |= TLBLO_DIRTY;
    // }
    entry_hi = page_num;
    entry_lo = frame_address | TLBLO_VALID | TLBLO_DIRTY;
    //ignore global bit and not caechable bit
    tlb_random(entry_hi, entry_lo);
    splx(spl);

    return 0;
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

