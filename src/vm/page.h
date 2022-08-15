#include <stdbool.h>
#include <stdint.h>


struct frame_sup_page_table_entry{
	bool in_memory;
	int slot_id;
	char* file_name;
};


uint32_t* sup_pagedir_create(void);
struct frame_sup_page_table_entry* lookup_sup_page_table_entry (uint32_t *spd, const void* addr);
uint32_t* set_sup_page_table(void);
bool set_sup_page_table_entry(uint32_t* spd, const void* uaddr);
void sup_pagedir_destroy(uint32_t *spd);
