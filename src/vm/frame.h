#ifndef FRAME_H
#define FRAME_H

#include "threads/thread.h"
#include "threads/pte.h"
#include "userprog/pagedir.h"
#include "vm/swap.h"
#include "vm/page.h"
#include "stdint.h"



extern struct frame_entry* frame_table;
extern struct lock global_frame_table_lock;

struct frame_entry{
	bool used;
	int tid;
	uint8_t* vaddr;
};


void frame_table_init(void);


int get_frame_table_size(void);
void set_frame_table_entry_with_idx_cnt(size_t page_idx, size_t page_cnt, struct thread *t);
void set_frame_table_entry_with_va(void* uva, void* kva);
void unset_frame_table_entry_with_idx_cnt(size_t page_idx, size_t page_cnt);
void unset_frame_table_entries_of_thread(struct thread* t);
void second_chance_entry(int clock);
size_t choose_evicted_entry(void);
bool replace_frame_entry(void* fault_addr, size_t i);
bool is_full_frame_table(void);

#endif
