#include "limits.h"
#include "round.h"
#include "string.h"
#include "devices/timer.h"
#include "filesys/cache.h"
#include "filesys/filesys.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/synch.h"

static struct lock buffer_cache_lock;
static struct buffer_cache* buffer_cache_arr;

void
buffer_cache_init(void){
	int bc_arr_size_bytes = sizeof (struct buffer_cache) * BUFFER_CACHE_ARR_SIZE;
	int bc_arr_size_pages = DIV_ROUND_UP (bc_arr_size_bytes, PGSIZE);

	buffer_cache_arr = palloc_get_multiple(PAL_ZERO, bc_arr_size_pages);

	printf("buffer cache is initialized\n");
	printf("bc size: %d, bc page size: %d, bc_arr addr: %p\n", bc_arr_size_bytes, bc_arr_size_pages, buffer_cache_arr);

	lock_init(&buffer_cache_lock);
}


struct buffer_cache*
get_buffer_cache_value_from_sector(block_sector_t sector_idx){
	struct buffer_cache* ret=malloc(sizeof(struct buffer_cache));

	lock_acquire(&buffer_cache_lock);

  struct buffer_cache* bc = get_buffer_cache_from_sector(sector_idx);
#ifdef INFO11
	printf("get_buffer_cache_value sector_idx:%d, bc: %p, ret:%p\n", sector_idx, bc, ret);
#endif
	memcpy(ret, bc, sizeof(struct buffer_cache));

	lock_release(&buffer_cache_lock);

	return ret;
}

void
write_src_to_buffer_cache_from_sector(block_sector_t sector_idx, int sector_ofs, const void* src, int size){
	if (size-sector_ofs > BLOCK_SECTOR_SIZE)
		PANIC("size-sector_ofs should be smaller than BLOCK_SECTOR_SIZE\n");		

	lock_acquire(&buffer_cache_lock);

  struct buffer_cache* bc = get_buffer_cache_from_sector(sector_idx);
	memcpy(bc->data + sector_ofs ,src ,size);
	bc->is_dirty=true;

#ifdef INFO7
	printf("write_src_buffer_cache_from sector_idx:%d, bc:%p\n", sector_idx, bc);
#endif

	lock_release(&buffer_cache_lock);
}

struct buffer_cache*
get_buffer_cache_from_sector(block_sector_t sector_idx){
  struct buffer_cache* bc = is_in_buffer_cache_arr(sector_idx);

  if (bc == NULL){
    bc = write_in_buffer_cache_arr(sector_idx);
  }

	return bc;
}

struct buffer_cache*
is_in_buffer_cache_arr(block_sector_t sector_idx){
	int i=0;
	struct buffer_cache* bc=NULL, *bc_return=NULL;

	for (i = 0; i < BUFFER_CACHE_ARR_SIZE; i++) {
		bc = buffer_cache_arr + i;
		if (bc->sector_idx == sector_idx && bc->is_used==true) {
			bc->is_accessed=true;
			bc->access_cnt++;
#ifdef INFO
			printf("found sector_idx %d at buffer cache i %d\n", sector_idx, i);
#endif
			bc_return=bc;
			break;
		}
	}

#ifdef INFO5
	for (i = 0; i < BUFFER_CACHE_ARR_SIZE; i++) {
		bc = buffer_cache_arr + i;
		if (bc->sector_idx ==0 && bc->is_used==true){
			if (((int*)bc->data)[0] != 512 ){
				printf("sector 0 length %d\n", ((int*)bc->data)[0]);
				PANIC("fuck");
			}
		}
	}
#endif

	return bc_return;
}

struct buffer_cache*
write_in_buffer_cache_arr(block_sector_t sector_idx) {
	int idx = find_empty_in_buffer_cache_arr();
	struct buffer_cache* bc = buffer_cache_arr + idx;

#ifdef INFO5
	printf("sector idx %d will be written to buffer cache to idx %d, bc:%p\n", sector_idx, idx, bc);
#endif

	bc->sector_idx = sector_idx;
	bc->is_used = true;
	bc->is_accessed = true;
	bc->is_dirty = false;
	bc->access_cnt = 1;

#ifdef INFO5
	int i=0;
	if (sector_idx==0){
		for (i=0; i<128; i++){
			printf("%d ", ((int*)(bc->data))[i]);
		}
		printf("\nis before\n");
	}
#endif

	block_read(fs_device, sector_idx, bc->data);

#ifdef INFO5
	printf("sector idx %d is written to buffer cache to idx %d, bc:%p\n", sector_idx, idx, bc);
	if (sector_idx==0){
		for (i=0; i<128; i++){
			printf("%d ", ((int*)(bc->data))[i]);
		}
		printf("\nis after\n");
		struct buffer_cache test;
		block_read(fs_device, sector_idx, &test);
		printf("test\n");
	}


#endif

	return bc;
}


int
find_empty_in_buffer_cache_arr() {
	int i=0, empty_idx=-1;
	struct buffer_cache* bc;

	for (i = 0; i < BUFFER_CACHE_ARR_SIZE; i++) {
		bc = buffer_cache_arr + i;
		if (bc->is_used==false) {
			empty_idx = i;
			return empty_idx;
		}
	}

	empty_idx = choose_victim_in_buffer_cache_arr();
	if(empty_idx == -1)
		PANIC("failed to find empty in buffer_cache_arr\n");
	
	return empty_idx;
}


int
choose_victim_in_buffer_cache_arr() {
	int i=0, victim_idx=-1;
	int min_access_cnt=INT_MAX;
	struct buffer_cache* bc;

	for (i = 0; i < BUFFER_CACHE_ARR_SIZE; i++) {
		bc = buffer_cache_arr + i;
		if (bc->is_used==true && bc->is_accessed==false) {
			victim_idx = i;
			break;
		}

		if (min_access_cnt > bc->access_cnt){
			min_access_cnt = bc->access_cnt;
			victim_idx = i;
		}
	}	

	bc = buffer_cache_arr + victim_idx;

#ifdef INFO5
	printf("victim idx is %d and its sector %d\n", victim_idx, bc->sector_idx);
	if (bc->sector_idx==0){
		int i=0;
		for (i=0; i<128; i++){
			printf("%d ", ((int*)(bc->data))[i]);
		}
	}
	printf("\nwritten to addr %p if dirty %d\n", bc->data, bc->is_dirty);
#endif

	if(victim_idx == -1)
		PANIC("failed to find victim in buffer_cache_arr\n");

	if (bc->is_dirty==true)
		block_write(fs_device, bc->sector_idx, bc->data);

	return victim_idx;
}

void
clock_algorithm_for_buffer_cache_arr(int tick) {
	if (buffer_cache_arr == NULL)
		return;

	int idx = tick % BUFFER_CACHE_ARR_SIZE;
	struct buffer_cache* bc;
	bc = buffer_cache_arr+ idx;

	if (bc->is_used==true && bc->is_accessed==true) 
		bc->is_accessed = false;
}


void
run_dirty_buffer_cache_writer() {
	printf("run dirty_buffer cache writer\n");
	thread_create("dirty_buffer_cache_writer", PRI_DEFAULT, write_dirty_buffer_cache_to_sector_periodically, NULL);
}


void
write_dirty_buffer_cache_to_sector_periodically(void* aux UNUSED) {
	while(true) {
		write_dirty_buffer_cache_to_sector();
		timer_sleep(TIMER_FREQ);
	}

	return;
}


void
write_dirty_buffer_cache_to_sector(void) {
	if (buffer_cache_arr == NULL)
		return;

#ifdef INFO5
	printf("interrupt\n");
#endif

	int i=0;
	struct buffer_cache* bc=NULL;

	lock_acquire(&buffer_cache_lock);

	for (i = 0; i < BUFFER_CACHE_ARR_SIZE; i++) {
		bc = buffer_cache_arr + i;
		if (bc->is_used==true && bc->is_dirty==true) {
#ifdef INFO5
			if (bc->sector_idx==0)
				printf("sector 0 is written peridicallly\n");
#endif

			block_write(fs_device, bc->sector_idx, bc->data);
		}
	}

	lock_release(&buffer_cache_lock);

#ifdef INFO5
	printf("interrupt finished\n");
#endif

	return;
}



