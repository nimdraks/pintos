#ifndef SWAP_H
#define SWAP_H

#include "devices/block.h"

struct swap_block{
	block_sector_t sector;
	size_t sector_size;
};

void swap_init(void);
void swap_free_map_init(void);
struct swap_block* swap_write_page(void* pages, size_t page_number);
bool swap_read_block(block_sector_t sector, size_t cnt, void* pages);
void swap_remove_block(block_sector_t sector, size_t cnt);

#endif
