#include <round.h>
#include <debug.h>
#include <bitmap.h>
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
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
	lock_acquire(&swap_lock);

	struct swap_block* sb = NULL;
	int cnt = DIV_ROUND_UP(page_number * PGSIZE, BLOCK_SECTOR_SIZE); 
	block_sector_t sector = bitmap_scan_and_flip(swap_free_map, 0, cnt, false);
	if (sector == BITMAP_ERROR)
		return NULL;

	block_write(swap_device, sector, pages);
	
	sb = malloc(sizeof(struct swap_block));
	sb->sector = sector;
	sb->sector_size = cnt;

	lock_release(&swap_lock);
	return sb;
}


bool
swap_read_block(block_sector_t sector, size_t cnt, void* pages){
	lock_acquire(&swap_lock);
	size_t i=0;
	void* addr = pages;

	for (i=0; i<cnt; i++){
		addr = pages + (BLOCK_SECTOR_SIZE * cnt);
		block_read(swap_device, sector+cnt, addr);
	}

	lock_release(&swap_lock);

	return true;
}






