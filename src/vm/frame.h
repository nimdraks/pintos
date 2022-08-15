#include "threads/thread.h"
#include "stdint.h"



extern struct frame_entry* frame_table;

struct frame_entry{
	bool used;
	tid_t tid;
	uint8_t* vaddr;
	int age;
};


void frame_table_init(void);


int get_frame_table_size(void);
void set_frame_table_entry_with_idx_cnt(size_t page_idx, size_t page_cnt, struct thread *t);
void set_frame_table_entry_with_va(void* uva, void* kva);
void unset_frame_table_entry_with_idx_cnt(size_t page_idx, size_t page_cnt);
void unset_frame_table_entries_of_thread(struct thread* t);
