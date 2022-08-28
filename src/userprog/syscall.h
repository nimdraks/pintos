#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include "threads/thread.h"

void syscall_init (void);
bool check_esp_invalidity(struct thread* t, void* esp);
bool check_ptr_invalidity(struct thread* t, void* ptr, void* esp);
bool check_mmap_argument_invalidty(int fd, void* addr);
void exit_unexpectedly(struct thread* t);
void exit_expectedly(struct thread* t, int);

#endif /* userprog/syscall.h */
