#include <round.h>
#include <debug.h>
#include <bitmap.h>
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "threads/pte.h"
#include "filesys/inode.h"
#include "vm/swap.h"


struct lock swap_lock;
struct block* swap_device;
struct bitmap* swap_free_map;


void
swap_init (void){
	swap_device = block_get_role(BLOCK_SWAP);
	if (swap_device == NULL)
		PANIC ("No swap system device found.");
	swap_free_map_init();
	lock_init(&swap_lock);
	printf("Swap device loaded\n");
}


void
swap_free_map_init(void){
	swap_free_map = bitmap_create(block_size(swap_device));
	if (swap_free_map == NULL)
		PANIC ("bitmap creation failed--swap system device is too large");
}


struct swap_block*
swap_write_page(void* pages, size_t page_number){
	printf("try to get swap lock %d\n", thread_tid());
	lock_acquire(&swap_lock);
	printf("get swap lock %d\n", thread_tid());
	pages = (uint8_t*)((uintptr_t) pages & PTE_ADDR);

	struct swap_block* sb = NULL;
	size_t cnt = DIV_ROUND_UP(page_number * PGSIZE, BLOCK_SECTOR_SIZE); 
	block_sector_t sector = bitmap_scan_and_flip(swap_free_map, 0, cnt, false);
	if (sector == BITMAP_ERROR)
		return NULL;

	size_t i=0;
	void* addr = pages;
	for (i=0; i<cnt; i++){
		addr = pages + (BLOCK_SECTOR_SIZE * i);
		block_write(swap_device, sector+i, addr);
	}

	printf("release swap lock %d\n", thread_tid());
	lock_release(&swap_lock);
	
	sb = malloc(sizeof(struct swap_block));
	sb->sector = sector;
	sb->sector_size = cnt;

	return sb;
}


bool
swap_read_block(block_sector_t sector, size_t cnt, void* pages){
	printf("try to get swap lock %d\n", thread_tid());
	lock_acquire(&swap_lock);
	printf("get swap lock %d\n", thread_tid());
	size_t i=0;
	void* addr = pages;

	for (i=0; i<cnt; i++){
		addr = pages + (BLOCK_SECTOR_SIZE * i);
		block_read(swap_device, sector+i, addr);
	}

	printf("release swap lock %d\n", thread_tid());
	lock_release(&swap_lock);

	return true;
}


void
swap_remove_block(block_sector_t sector, size_t cnt){
 	printf("try to get swap lock %d\n", thread_tid());
	lock_acquire(&swap_lock);
	printf("get swap lock %d\n", thread_tid()); 
	bitmap_set_multiple (swap_free_map, sector, cnt, false);

	printf("release swap lock %d\n", thread_tid());
	lock_release(&swap_lock);
}


