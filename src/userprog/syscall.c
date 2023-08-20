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
#include "threads/synch.h"
#include "lib/user/syscall.h"
#include "lib/string.h"
#include "devices/input.h"

static void syscall_handler (struct intr_frame *);
static struct semaphore sysSema;

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
	sema_init(&sysSema, 1);
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
	if (check_ptr_invalidity(t, f->esp)){
		exit_unexpectedly(t);
		return;
	}
  int syscallNum = *espP;

	char* fileName=NULL;
	int fileInitSize=0;
	int fileSize=0;
	int fd=0;
	struct file* file=NULL;	
	char* fileBuffer=NULL;
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

			if (strlen(fileName) > 500)
				f->eax=0;
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
			break;

		case SYS_TELL:
			fd = *(espP+1);		
			file = thread_open_fd(fd);
			f->eax = file_tell(file);	
			break;

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
			}else if (fd == 0){
				break;
			}

			fileBuffer = (char*)*(espP+2);
			fileSize = *(espP+3);

#ifdef INFO6
			printf("SYS_WRITE: fd %d, fileBuffer %x, fileSize %d\n", fd, fileBuffer, fileSize);
#endif

			if(check_ptr_invalidity(t,fileBuffer)){
				exit_unexpectedly(t);
				return;
			}

			file = thread_open_fd(fd);
			if (file==NULL){
				exit_unexpectedly(t);
				return;
			}

			if (file_is_dir(file)){
				exit_unexpectedly(t);
				return;
			}

			f->eax = file_write(file, fileBuffer, fileSize);
#ifdef INFO6
			printf("SYS_WRITE: writtend byte %d\n", f->eax);
#endif
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

		case SYS_MKDIR:
			fileName = (char*)*(espP+1);
			f->eax=filesys_create_dir(fileName);
			break;

		case SYS_CHDIR:
			fileName = (char*)*(espP+1);
			f->eax=filesys_change_dir(fileName);
			break;

		case SYS_REMOVE:
			fileName = (char*)*(espP+1);
			f->eax=filesys_remove(fileName);
			break;

		case SYS_INUMBER:
			fd = (char*)*(espP+1);

			file = thread_open_fd(fd);
			if (file==NULL){
				exit_unexpectedly(t);
				return;
			}

			f->eax=file_sector_number(file);
			break;

		case SYS_ISDIR:
			fd = (char*)*(espP+1);

			file = thread_open_fd(fd);
			if (file==NULL){
				exit_unexpectedly(t);
				return;
			}

			f->eax=file_is_dir(file);
			break;

		default:
			break;
	}
}




