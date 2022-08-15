#include "threads/palloc.h"
#include "threads/pte.h"
#include "vm/page.h"
#include <stddef.h>
#include <round.h>
#include <stdio.h>

size_t size_sup_page_table_entry;
size_t size_sup_page_table;
size_t needed_page_numbers;

uint32_t*
sup_pagedir_create(void) {
	uint32_t* upd = palloc_get_page(PAL_ZERO);
	return upd;
}


struct frame_sup_page_table_entry*
lookup_sup_page_table_entry (uint32_t *spd, const void* vaddr) {
	uint32_t *spde;
	struct frame_sup_page_table_entry *spt, *spte;

	spde = spd + pd_no(vaddr);
	if (*spde == 0) {
		spt = (struct frame_sup_page_table_entry*)set_sup_page_table ();
		if (spt == NULL)
			return NULL;
		*spde = spt;
	}else{
		spt = (struct frame_sup_page_table_entry*)(*spde);
	}

	spte = spt + pt_no(vaddr);

	return spte;
}

uint32_t*
set_sup_page_table () {
	size_sup_page_table_entry = sizeof(struct frame_sup_page_table_entry);
	size_sup_page_table = size_sup_page_table_entry * (1 << PTBITS);
	needed_page_numbers = DIV_ROUND_UP(size_sup_page_table, PGSIZE);

	uint32_t* spde = palloc_get_multiple(PAL_ASSERT|PAL_ZERO, needed_page_numbers);
	if (spde == NULL)
		return NULL;

	return spde;
}

bool
set_sup_page_table_entry(uint32_t *spd, const void* uaddr){
	struct frame_sup_page_table_entry* spte = lookup_sup_page_table_entry(spd, uaddr);
	if(spte == NULL){
		return false;
	}
	spte->in_memory = true;	
	return true;
}


void
sup_pagedir_destroy (uint32_t * spd){
	if (spd == NULL){
		return;
	}

	uint32_t *spde;
	for (spde = spd; spde < spd + pd_no(PHYS_BASE); spde++) {
		if (*spde != 0) {
			palloc_free_multiple((void*)*spde, needed_page_numbers);
		}
	}
	palloc_free_page(spd);
}

