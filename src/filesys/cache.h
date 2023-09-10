#include <stdbool.h>
#include <stdio.h>
#include <devices/block.h>

#define BUFFER_CACHE_ARR_SIZE 64

struct buffer_cache {
  char data[BLOCK_SECTOR_SIZE];
  block_sector_t sector_idx;
  bool is_used;
  bool is_accessed;
  bool is_dirty;
	int access_cnt;
};

void buffer_cache_init(void);
struct buffer_cache* get_buffer_cache_value_from_sector(block_sector_t);
void write_src_to_buffer_cache_from_sector(block_sector_t, int, const void*, int);
struct buffer_cache* get_buffer_cache_from_sector(block_sector_t);
struct buffer_cache* is_in_buffer_cache_arr(block_sector_t);
struct buffer_cache* write_in_buffer_cache_arr(block_sector_t);
int find_empty_in_buffer_cache_arr(void);
int choose_victim_in_buffer_cache_arr(void);
void clock_algorithm_for_buffer_cache_arr(int);
void run_dirty_buffer_cache_writer(void);
void write_dirty_buffer_cache_to_sector_periodically(void*);
void write_dirty_buffer_cache_to_sector(void);
