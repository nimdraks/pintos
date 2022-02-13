#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "lib/user/syscall.h"



static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
	int* espP=f->esp;
	if (espP > 0xc0000000){
			printf("%s: exit(%d)\n",thread_current()->name, -1);
			thread_unblock(tid_thread(thread_current()->p_tid));
			thread_exit();
			return;
	}


  int syscallNum = *espP;
	switch(syscallNum){
		case SYS_WRITE:
			printf("%s", (char*)*(espP+2));
			break;
		case SYS_EXIT:
			printf("%s: exit(%d)\n",thread_current()->name, *(espP+1));
			thread_unblock(tid_thread(thread_current()->p_tid));
			thread_exit();
			break; 
		default:
			break;
	}




//  halt();
//  syscall1(2, "args-none");

//	printf("%c", esp++);
//  printf ("system call!\n");
//  thread_exit ();
	return;
}




