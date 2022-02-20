#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include "threads/thread.h"

void syscall_init (void);
bool check_ptr_invalidity(struct thread* t, void* ptr);
void exit_unexpectedly(struct thread* t);
void exit_expectedly(struct thread* t);

#endif /* userprog/syscall.h */
