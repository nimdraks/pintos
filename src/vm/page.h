#ifndef PAGE_H
#define PAGE_H

#include <stdbool.h>
#include <stdint.h>
#include <devices/block.h>


struct frame_sup_page_table_entry{
	bool in_memory;
	block_sector_t sector;
	int pin;
	size_t cnt;
};


uint32_t* sup_pagedir_create(void);
struct frame_sup_page_table_entry* lookup_sup_page_table_entry (uint32_t *spd, const void* addr);
struct frame_sup_page_table_entry* just_lookup_sup_page_table_entry (uint32_t *spd, const void* addr);
uint32_t* set_sup_page_table(void);
bool set_sup_page_table_entry(uint32_t* spd, const void* uaddr, bool is_kernel);
bool set_sup_page_table_entry_only_pin(uint32_t* spd, const void* uaddr);
void sup_page_table_pin_zero (uint32_t* spd);
void sup_pagedir_destroy(uint32_t *spd);
struct frame_sup_page_table_entry* at_swap(void* fault_addr);
bool read_from_swap(void* fault_addr);

#endif
