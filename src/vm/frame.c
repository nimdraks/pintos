#include <stdio.h>
#include <round.h>
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "vm/frame.h"


static uint8_t* frame_base_vaddr;
static size_t frame_number;
struct frame_entry* frame_table;
struct lock frame_table_lock;

void
frame_table_init (){
	frame_number = get_pool_size(false);
	frame_base_vaddr = get_pool_base(false);	
	int frame_pages = DIV_ROUND_UP(get_frame_table_size(), PGSIZE);
	frame_table = (struct frame_entry*)palloc_get_multiple (PAL_ASSERT|PAL_ZERO, frame_pages);

	lock_init(&frame_table_lock);	

	size_t i =0;
	for (i = 0; i < frame_number; i++){
		frame_table[i].used=false;
		frame_table[i].tid=-1;
		frame_table[i].vaddr=0;
	}

	printf("User frame number %d in user pool\n", frame_number);
	printf("User frame vaddr(kernel vaddr) starts at %p\n", frame_base_vaddr);
	printf("Frame_entry size %d\n", sizeof(struct frame_entry));
	printf("The number of pages for the frame table, %d\n", frame_pages);

}

int get_frame_table_size(void){
	return sizeof(struct frame_entry) * frame_number;
}

void set_frame_table_entry_with_idx_cnt(size_t page_idx, size_t page_cnt, struct thread* t){
	lock_acquire (&frame_table_lock);
	size_t offset=0;	

	for (offset=0; offset < page_cnt; offset++){
		size_t page_no = page_idx + offset;
		if (frame_table[page_no].used == true){
			printf("tid:%d, vaddr:%p\n", frame_table[page_no].tid, frame_table[page_no].vaddr);
			PANIC ("it is already allocated");
		}
		frame_table[page_no].used=true;
		frame_table[page_no].tid=t->tid;
	}

	lock_release(&frame_table_lock);
}


void set_frame_table_entry_with_va(void* uva, void* kva){
	int page_idx = pg_no (kva) - pg_no(frame_base_vaddr);

	lock_acquire (&frame_table_lock);

	if (frame_table[page_idx].used == false){
		printf("tid:%d, vaddr:%p\n", frame_table[page_idx].tid, frame_table[page_idx].vaddr);
		PANIC ("it should be allocated");
	}

	frame_table[page_idx].vaddr=uva;

	lock_release(&frame_table_lock);
}

void unset_frame_table_entry_with_idx_cnt(size_t page_idx, size_t page_cnt){
	lock_acquire (&frame_table_lock);
	size_t offset=0;

	for (offset=0; offset < page_cnt; offset++){
		size_t page_no = page_idx + offset;
		frame_table[page_no].used=false;
		frame_table[page_no].tid=-1;
		frame_table[page_no].vaddr=0;
	}

	lock_release(&frame_table_lock);
}


void unset_frame_table_entries_of_thread(struct thread* t){
	size_t i = 0;
	lock_acquire(&frame_table_lock);
	for (i = 0; i < frame_number; i++){
		if (frame_table[i].tid == t->tid){
			frame_table[i].used=false;
			frame_table[i].tid=-1;
			frame_table[i].vaddr=0;
		}
	}
	lock_release(&frame_table_lock);
}
