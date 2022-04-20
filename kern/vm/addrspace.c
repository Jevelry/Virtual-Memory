/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <spinlock.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>
#include <proc.h>

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 *
 * UNSW: If you use ASST3 config as required, then this file forms
 * part of the VM subsystem.
 *
 */

struct addrspace *
as_create(void)
{
	struct addrspace *as;

	as = kmalloc(sizeof(struct addrspace));
	if (as == NULL) {
		return NULL;
	}

	/*
	 * Initialize as needed.
	 */
	as->stack = USERSTACK;

	as->region_head = NULL;

	as->page_table = kmalloc(sizeof(paddr_t *) * PT_FL_SIZE);

	if (as->page_table == NULL) {
		panic("page table not initialised");
	}

	for (int counter = 0; counter < PT_FL_SIZE; counter++) {
		as->page_table[counter] = NULL;
	}

	return as;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *newas;

	newas = as_create();
	if (newas==NULL) {
		return ENOMEM;
	}

	struct region *newas_region = newas->region_head;
	struct region *oldas_region = old->region_head;

	//copy region linked list
	while (oldas_region != NULL) {
		struct region *new = kmalloc(sizeof(struct region));
		if (new == NULL) {
			return ENOMEM;
		}

		new->base = oldas_region->base;
		new->size = oldas_region->size;
		new->flags = oldas_region->flags;
		new->next = NULL;

		if (newas_region == NULL) {
            newas->region_head = new;
			newas_region = new;
			oldas_region = oldas_region->next;
        } else {
            newas_region->next = new;
            newas_region = newas_region->next;
            oldas_region = oldas_region->next;
        }
	}
	

	//copy page table
	paddr_t **new_pt = newas->page_table;
	paddr_t **old_pt = old->page_table;

	if (old_pt == NULL || new_pt == NULL) {
		return EFAULT;
	}

	//traverse first level
	for (int i = 0; i < PT_FL_SIZE; i++) {
		if (old_pt[i] == NULL) {
			new_pt[i] = NULL;
		} else {
			new_pt[i] = kmalloc(sizeof(paddr_t) * PT_SL_SIZE);
			if (new_pt[i] == NULL) {
				return ENOMEM;
			}
			
			//traverse second level
			for (int j = 0; j < PT_SL_SIZE; j++) {
				if (old_pt[i][j] == 0) {
					new_pt[i][j] = 0;
				} else {
					vaddr_t k_addr = alloc_kpages(1);
					if (k_addr == 0) {
						return ENOMEM;
					}

					paddr_t f_addr = KVADDR_TO_PADDR(k_addr);
					new_pt[i][j] = f_addr;

					memcpy((void *) k_addr, (void*) PADDR_TO_KVADDR(old_pt[i][j]), PAGE_SIZE);
				}
			}
		}
	}

	newas->page_table = new_pt;

	*ret = newas;
	return 0;
}

void
as_destroy(struct addrspace *as)
{
	/*
	 * Clean up as needed.
	 */

	if (as == NULL) {
		panic("bad memory reference");
	}

	if (as->region_head != NULL) {
		struct region *cur = as->region_head;
		struct region *next;
		while (cur != NULL) {
			next = cur->next;
			kfree(cur);
			cur = next;
		}
	}

	if (as->page_table != NULL) {
		for (int i = 0; i < PT_FL_SIZE; i++) {
			if (as->page_table[i] != NULL) {
				for (int j = 0; j < PT_SL_SIZE; j++) {
					if (as->page_table[i][j] != 0) {
						free_kpages(PADDR_TO_KVADDR(as->page_table[i][j]));
					}
				}
			}
			kfree(as->page_table[i]);
		}
		kfree(as->page_table);
	}

	kfree(as);
}

void
as_activate(void)
{
	struct addrspace *as;

	as = proc_getas();
	if (as == NULL) {
		/*
		 * Kernel thread without an address space; leave the
		 * prior address space in place.
		 */
		return;
	}

	int spl = splhigh();

	for (int counter = 0; counter < NUM_TLB; counter++) {
		tlb_write(TLBHI_INVALID(counter), TLBLO_INVALID(), counter);
	}

	splx(spl);

}

void
as_deactivate(void)
{
	/*
	 * Write this. For many designs it won't need to actually do
	 * anything. See proc.c for an explanation of why it (might)
	 * be needed.
	 */

	int spl = splhigh();

	for (int counter = 0; counter < NUM_TLB; counter++) {
		tlb_write(TLBHI_INVALID(counter), TLBLO_INVALID(), counter);
	}

	splx(spl);
}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */
int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t memsize,
		 int readable, int writeable, int executable)
{
	if (as == NULL) {
		return EFAULT;
	}

	struct region *region = kmalloc(sizeof(struct region));
	if (region == NULL) {
		return ENOMEM;
	}

	region->base = vaddr;
	region->size = memsize;

	int flag = 0;
	if (readable) flag = flag | FLAG_R;
	if (writeable) flag = flag | FLAG_W;
	if (executable) flag = flag | FLAG_X;

	region->flags = flag;
	region->next = as->region_head;
	as->region_head = region;

	return 0;
}

int
as_prepare_load(struct addrspace *as)
{
	if (as == NULL) {
		return EFAULT;
	}

	struct region *cur = as->region_head;
	while (cur != NULL) {
		if (cur->flags == FLAG_R) {
			cur->flags = cur->flags | FLAG_L;
			cur->flags = cur->flags | FLAG_W;
		}
		cur = cur->next;
	}

	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	if (as == NULL) {
		return EFAULT;
	}

	struct region *cur = as->region_head;
	while (cur != NULL) {
		if (cur->flags & FLAG_L) {
			cur->flags = FLAG_R;
		}
		cur = cur->next;
	}

	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{

	int size = FIXED_STACK_SIZE * PAGE_SIZE;
	vaddr_t stack = USERSTACK - size;

	int errno = as_define_region(as, stack, size, 1, 1, 0);
	if (errno) {
		return errno;
	}
	
	/* Initial user-level stack pointer */
	*stackptr = USERSTACK;

	return 0;
}

