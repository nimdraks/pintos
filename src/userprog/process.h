#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include <stdbool.h>

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
bool install_page (void *upage, void *kpage, bool writable, bool is_kernel);
bool add_new_page (void* fault_addr);
bool add_new_page_with_kpage (void* fault_addr, void* kpage, bool is_kernel);

#endif /* userprog/process.h */
