#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "lib/user/syscall.h"
#include "lib/string.h"

static void syscall_handler (struct intr_frame *);


void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}


bool
check_ptr_invalidity(struct thread* t, void* ptr){
	if (!is_user_vaddr(ptr) || pagedir_get_page(t->pagedir, ptr) == NULL ){
			return true;
	}
	return false;
}

void
exit_unexpectedly(struct thread* t){
			printf("%s: exit(%d)\n",t->name, -1);
			thread_unblock(tid_thread(t->p_tid));
			thread_exit();
}

void
exit_expectedly(struct thread* t){
			thread_unblock(tid_thread(t->p_tid));
			thread_exit();
}



static void
syscall_handler (struct intr_frame *f) 
{
	int* espP=f->esp;
	char* fileName=NULL;
	int fileInitSize=0;
	struct thread* t = thread_current();
	if (check_ptr_invalidity(t, f->esp)){
		exit_unexpectedly(t);
		return;
	}
  int syscallNum = *espP;
	struct file* file=NULL;	
	int fd=0;

	switch(syscallNum){
		case SYS_WRITE:
			printf("%s", (char*)*(espP+2));
			break;

		case SYS_EXIT:
      if (!is_user_vaddr(espP+1))
				printf("%s: exit(%d)\n",thread_current()->name,-1);
			else
				printf("%s: exit(%d)\n",thread_current()->name, *(espP+1));
			exit_expectedly(t);
			break;

		case SYS_CREATE:
			fileName = (char*)*(espP+1);
			fileInitSize = *(espP+2);
			if(check_ptr_invalidity(t, (void*)fileName) || fileName==NULL ){
				exit_unexpectedly(t);
				return;
			}

			if (strlen(fileName) > 500){
				f->eax=0;
			}
			else
				f->eax=filesys_create(fileName, fileInitSize);
			break;

		case SYS_OPEN:
			fileName = (char*)*(espP+1);
			if(check_ptr_invalidity(t, (void*)fileName) || fileName==NULL ){
				exit_unexpectedly(t);
				return;
			}
			if(strcmp(fileName, "")==0){
				f->eax=-1;
				break;
			}
			file = filesys_open(fileName);
			if (file == NULL){
				f->eax=-1;
				break;
			}	
			fd = thread_make_fd(file);
			f->eax=fd;
			break; 

		case SYS_CLOSE:
			fd = *(int*)*(espP+1);


		default:
			break;
	}
}




