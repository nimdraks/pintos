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
#include "threads/palloc.h"
#include "threads/synch.h"
#include "lib/user/syscall.h"
#include "lib/string.h"
#include "devices/input.h"
#include "vm/page.h"

static void syscall_handler (struct intr_frame *);
static struct semaphore sysSema;

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
	sema_init(&sysSema, 1);
}


bool
check_esp_invalidity(struct thread* t, void* esp){
	if (!is_user_vaddr(esp) || pagedir_get_page(t->pagedir, esp) == NULL ){
			return true;
	}
	return false;
}

bool
check_ptr_invalidity(struct thread* t, void* ptr, void* esp){
	if (!is_user_vaddr(ptr)){
		printf("invalid 1\n");
		return true;
	}
/*
 	if (pagedir_get_page(t->pagedir, ptr) == NULL) {
		if ( is_grown_stack_kernel(esp, ptr)){
			return false;
		} else
			printf("invalid 2\n");
			return true;
	}
*/
	return false;
}

bool
check_mmap_argument_invalidty(int fd, void* addr){
	if(fd==0 || fd==1){
		return true;
	}

	if(thread_open_fd(fd)==NULL){
		return true;
	}

	if (is_kernel_vaddr(addr) || addr == NULL){
		return true;
	}

	if (pg_ofs(addr) != 0x0){
		return true;
	}
	if (pagedir_get_page(thread_current()->pagedir, addr) != NULL ){
		return true;
	}
	return false;
}


void
exit_unexpectedly(struct thread* t){
			sema_down(&sysSema);
			printf("%s: exit(%d)\n",t->name, -1);
			struct thread* p_t = tid_thread(t->p_tid);
			struct childSema* childSema=thread_get_childSema(p_t, t->tid);
			childSema->ret=-1;
			sema_up(&sysSema);
			sema_up(&childSema->sema);	
			thread_exit();
}

void
exit_expectedly(struct thread* t, int cRet){
			sema_down(&sysSema);
			struct thread* p_t = tid_thread(t->p_tid);
			struct childSema* childSema=thread_get_childSema(p_t, t->tid);
			childSema->ret=cRet;
			sema_up(&sysSema);
			sema_up(&childSema->sema);

			thread_exit();
}



static void
syscall_handler (struct intr_frame *f) 
{
	int* espP=f->esp;
	struct thread* t = thread_current();
	if (check_esp_invalidity(t, f->esp)){
		exit_unexpectedly(t);
		return;
	}
  int syscallNum = *espP;

	char* fileName=NULL;
	int fileInitSize=0;
	int fileSize=0;
	int fd=0;
	void* addr=0;
	struct file* file=NULL;	
	char* fileBuffer=NULL;
	char* execFile=NULL;
	char* copyExecFile=NULL;
	int execPid=0;
	int waitPid=0;
	int i=0;
	char a=0;
	
	struct mmapDesc* mmap_desc=NULL;
	int mmid=-1;

	int k=0;

	switch(syscallNum){
		case SYS_HALT:
			exit_expectedly(t, 0);
			break;

		case SYS_EXIT:
      if (!is_user_vaddr(espP+1))
				printf("%s: exit(%d)\n",thread_current()->name,-1);
			else
				printf("%s: exit(%d)\n",thread_current()->name, *(espP+1));
			printf("byebye %d with return %d\n", thread_tid(), *(espP+1));
			exit_expectedly(t, *(espP+1));
			break;

		case SYS_CREATE:
			fileName = (char*)*(espP+1);
			fileInitSize = *(espP+2);
			printf("sys_create %s\n", fileName);

			if(check_ptr_invalidity(t, (void*)fileName, espP) || fileName==NULL ){
				exit_unexpectedly(t);
				return;
			}

			if (strlen(fileName) > 500){
				f->eax=0;
			}
			else
				f->eax=filesys_create(fileName, fileInitSize);

			printf("sys_create_success %d at file name %s at pid %d\n", f->eax, fileName, thread_tid());
			if (f->eax == 0){
				return;
			}
			break;

		case SYS_OPEN:
			fileName = (char*)*(espP+1);
			printf("sys_open name %s\n", fileName);
			if(check_ptr_invalidity(t, (void*)fileName, espP) || fileName==NULL ){
				exit_unexpectedly(t);
				return;
			}
			if(strcmp(fileName, "")==0){
				f->eax=-1;
				break;
			}
			file = filesys_open(fileName);
			if (file == NULL){
				printf("failed to open at tid %d for file %s\n", thread_tid(), fileName);
				f->eax=-1;
				break;
			}
			fd = thread_make_fd(file);
			printf("fd check %d at tid %d for file %s\n", fd, thread_tid(), fileName);
			f->eax=fd;
			break; 

		case SYS_CLOSE:
			fd = *(espP+1);
			printf("sys_close fd %d at pid %d \n", fd, thread_tid());
			thread_close_fd(fd);
			break;

		case SYS_READ:
			fd = *(espP+1);
			fileBuffer = (char*)*(espP+2);
			fileSize = *(espP+3);
			printf("%d: sysread, fd %d, fileBuffer %x, fileSize %d\n", thread_tid(), fd, fileBuffer, fileSize);
			if(check_ptr_invalidity(t,fileBuffer, espP)){
				printf("failed to sys read 1, tid: %d, fileBuffer: %x, espP %x\n", thread_tid(), fileBuffer, espP);
				exit_unexpectedly(t);
				return;
			}

			if (fd == 1){
				printf("failed to sys read 2\n");
				break;
			}

			file = thread_open_fd(fd);
			if (file==NULL){
				printf("failed to sys read 3, %x, %d\n", espP, *espP);
				exit_unexpectedly(t);
				return;
			}
/*
			for(i=0; i<=fileSize; i++) {
				a=*((char*)fileBuffer + i);
				a++;
			}

			for (k=0; k<fileSize; k++){
				fileBuffer[k]=0;
			}
	*/
			memset(fileBuffer, 0, fileSize);
//			printf("start to sys read\n");
			f->eax = file_read(file, (void*)fileBuffer, fileSize);
			printf("%d: finish to sysread\n", thread_tid());
			sup_page_table_pin_zero(t->s_pagedir);
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
			printf("%d: try to syswrite\n", thread_tid());
			fd = *(espP+1);
			if (fd == 1){
				printf("%s", (char*)*(espP+2));
				break;
			}else if (fd == 0){
				break;
			}

			fileBuffer = (char*)*(espP+2);
			fileSize = *(espP+3);

			if(check_ptr_invalidity(t,fileBuffer, espP)){
				exit_unexpectedly(t);
				return;
			}

			file = thread_open_fd(fd);
			if (file==NULL){
				exit_unexpectedly(t);
				return;
			}


			for (k=0; k<fileSize; k++){
				 a += fileBuffer[k];
			}

			if(a == 0)
				printf("shit  \n");	

			printf("%d: start to syswirte\n", thread_tid());
			f->eax = file_write(file, fileBuffer, fileSize);
			printf("%d: finish to syswirte\n", thread_tid());

			sup_page_table_pin_zero(t->s_pagedir);
			break;

		case SYS_EXEC:
			execFile = *(char**)(espP+1);
			if(check_ptr_invalidity(t,execFile, espP)){
				exit_unexpectedly(t);
				return;
			}
			printf("sys exec: %s\n", execFile);
			copyExecFile = palloc_get_page(PAL_ASSERT|PAL_ZERO);
			strlcpy(copyExecFile, execFile, strlen(execFile)+1);
			execPid=process_execute(copyExecFile);
			palloc_free_page (copyExecFile);
			f->eax=execPid;
			break;

		case SYS_WAIT:
			waitPid = *(espP+1);
			f->eax=process_wait(waitPid);
			
			break;

		case SYS_MMAP:
			fd = *(espP+1);
			addr = (void*)*(espP+2);
			if (check_mmap_argument_invalidty(fd, addr)){
				f->eax=-1;
				break;
			}

			file = thread_open_fd(fd);
			fileSize = file_length(file);	
			if (fileSize == 0){
				f->eax=-1;
				break;
			}

			mmap_desc = thread_make_mmid(fd);
			if (mmap_desc == NULL){
				f->eax=-1;
				break;
			}

			file_read(file, addr, fileSize);
			file_seek(file, 0);

			mmap_desc->file = file;
			mmap_desc->addr = addr;
			mmap_desc->offset = fileSize;
			thread_mmapDesc_page_dirty_init(mmap_desc->mmid);

			f->eax = mmap_desc->mmid;


			break;

		case SYS_MUNMAP:
			mmid = *(espP+1);
			thread_close_mmapDesc(mmid);
			break;

		case SYS_REMOVE:
			fileName = (char*)*(espP+1);
			if(check_ptr_invalidity(t, (void*)fileName, espP) || fileName==NULL ){
				exit_unexpectedly(t);
				return;
			}
			if(strcmp(fileName, "")==0){
				f->eax=-1;
				break;
			}
			filesys_remove(fileName);


			break;

		default:
			break;
	}
}




