#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "lib/user/syscall.h"
#include "lib/string.h"
#include "devices/input.h"

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
			struct thread* pThread = tid_thread(t->p_tid);
			pThread->c_ret=-1;
			thread_unblock(tid_thread(t->p_tid));
			thread_exit();
}

void
exit_expectedly(struct thread* t, int cRet){
			struct thread* pThread = tid_thread(t->p_tid);
			pThread->c_ret=cRet;
			thread_unblock(pThread);
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
	char* fileBuffer=NULL;
	int fileSize=0;

	char* execFile=NULL;
	char* copyExecFile=NULL;
	int execPid=0;

	int waitPid=0;

	switch(syscallNum){
		case SYS_HALT:
			exit_expectedly(t, 0);
			break;

		case SYS_EXIT:
      if (!is_user_vaddr(espP+1))
				printf("%s: exit(%d)\n",thread_current()->name,-1);
			else
				printf("%s: exit(%d)\n",thread_current()->name, *(espP+1));
			
			exit_expectedly(t, *(espP+1));
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
		//	if(strcmp(fileName, thread_current()->name)==0){
	//			file_deny_write(file);
//			}
	
			fd = thread_make_fd(file);
			f->eax=fd;
			break; 

		case SYS_CLOSE:
			fd = *(espP+1);
			thread_close_fd(fd);
			break;

		case SYS_READ:
			fd = *(espP+1);
			fileBuffer = (char*)*(espP+2);
			fileSize = *(espP+3);

			if(check_ptr_invalidity(t,fileBuffer)){
				exit_unexpectedly(t);
				return;
			}

			if (fd == 1){
				break;
			}

			file = thread_open_fd(fd);
			if (file==NULL){
				exit_unexpectedly(t);
				return;
			}

			f->eax = file_read(file, (void*)fileBuffer, fileSize);	

		  break;
		case SYS_SEEK:
			fd = *(espP+1);		
			file = thread_open_fd(fd);
			file_seek(file, *(espP+2));	

		case SYS_FILESIZE:
			fd = *(espP+1);
			file = thread_open_fd(fd);
			f->eax = file_length(file);	
			break;

		case SYS_WRITE:
			fd = *(espP+1);
			if (fd == 1){
				printf("%s", (char*)*(espP+2));
				break;
			}

			fileBuffer = (char*)*(espP+2);
			fileSize = *(espP+3);

			if(check_ptr_invalidity(t,fileBuffer)){
				exit_unexpectedly(t);
				return;
			}

			if (fd == 0){
				break;
			}

			file = thread_open_fd(fd);
			if (file==NULL){
				exit_unexpectedly(t);
				return;
			}

			f->eax = file_write(file, fileBuffer, fileSize);
			break;


		case SYS_EXEC:
			execFile = *(char**)(espP+1);
			if(check_ptr_invalidity(t,execFile)){
				exit_unexpectedly(t);
				return;
			}
			copyExecFile = (char*)malloc(strlen(execFile)+1);
			strlcpy(copyExecFile, execFile, strlen(execFile)+1);
			execPid=process_execute(copyExecFile);
			free(copyExecFile);
			f->eax=execPid;
			break;

		case SYS_WAIT:
			waitPid = *(espP+1);
			f->eax=process_wait(waitPid);
			break;

		default:
			waitPid = *(espP+1);
			f->eax=process_wait(waitPid);

			break;
	}
}




