#include "round.h"
#include "filesys/cache.h"
#include "devices/block.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"


struct buffer_cache {
	char data[BLOCK_SECTOR_SIZE];
	block_sector_t sector;
	bool is_used;
	bool is_accessed;
	bool is_dirty;	
};

static struct buffer_cache* buffer_cache_arr;

void
buffer_cache_init(void){
	int bc_arr_size_bytes = sizeof (struct buffer_cache) * BUFFER_CACHE_ARR_SIZE;
	int bc_arr_size_pages = DIV_ROUND_UP (bc_arr_size_bytes, PGSIZE);

	buffer_cache_arr = palloc_get_multiple(PAL_ZERO, bc_arr_size_pages);

	printf("buffer cache is initialized\n");
	printf("bc size: %d, bc page size: %d, bc_arr addr: %p\n", bc_arr_size_bytes, bc_arr_size_pages, buffer_cache_arr);
}


