#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/synch.h"
#include "filesys/file.h"

/* States in a thread's life cycle. */
enum thread_status
  {
    THREAD_RUNNING,     /* Running thread. */
    THREAD_READY,       /* Not running but ready to run. */
    THREAD_BLOCKED,     /* Waiting for an event to trigger. */
    THREAD_DYING        /* About to be destroyed. */
  };

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

#define BEFORE_DECIMAL 17
#define AFTER_DECIMAL 14

/* A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list. */
struct thread
  {
    /* Owned by thread.c. */
    tid_t tid;                          /* Thread identifier. */
    enum thread_status status;          /* Thread state. */
    char name[16];                      /* Name (for debugging purposes). */
    uint8_t *stack;                     /* Saved stack pointer. */
    int priority;                       /* Priority. */
		int64_t sleepTime;
    struct list_elem allelem;           /* List element for all threads list. */

	
    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */

		/* blockSlist */
		struct list_elem elemS;

		/* for priority donation */
		struct list lock_own_list;
		struct lock* wait_lock;
		int original_priority;

		/* for file descriptor */
		struct list fdList;
		struct list childList;

		struct semaphore execSema;
		bool success;

		/* mlfqs */
		int mlfqs_priority;
		int nice;
		int64_t recent_cpu;

		tid_t p_tid;


#ifdef USERPROG
    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                  /* Page directory. */
		struct file* tFile;

		uint32_t *s_pagedir;
#endif

		struct list mmid_list;

    /* Owned by thread.c. */
    unsigned magic;                     /* Detects stack overflow. */
  };



struct childSema{
		int tid;
		int ret;
		struct semaphore sema;	
		struct list_elem elem;	
	};



/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);
bool thread_current_high(void);

/* Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func (struct thread *t, void *aux);
void thread_foreach (thread_action_func *, void *);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

void thread_go_to_sleep(struct thread*);
void thread_check_awake(int64_t tick);
bool thread_highest_priority_into_front(struct thread* cur);

void thread_update_priority_from_lock_list(struct thread* t);

void thread_update_recent_cpu(struct thread* t);
void thread_update_priority(struct thread* t);
void thread_current_update_recent_cpu(void);
void update_all_thread_priority(void);
void update_all_thread_recent_cpu(void);
void update_ready_thread(void);
void update_load_avg(void);

int fraction_into(int num);
int fraction_out(int num);
int fraction_mul(int num1, int num2);
int fraction_div(int num, int denom);

bool check_mlfqs_list_empty(void);
struct thread* return_high_priority_mlfqs_list_entry(void);

struct thread* tid_thread(tid_t tid);

int thread_make_fd(struct file* file);
struct file* thread_open_fd (int fd);
bool thread_close_fd (int fd);
void thread_close_all_fd (void);

void thread_make_childSema (int);
struct childSema* thread_get_childSema (struct thread*, int);
bool thread_remove_childSema (struct thread* t, int childtid);
void thread_remove_all_childSema (void);


struct mmapDesc* thread_make_mmid(int fd);
struct mmapDesc* thread_get_mmapDesc (int mmid);
bool thread_close_mmapDesc(int mmid);
void thread_close_all_mmapDesc (void);
void thread_mmapDesc_page_dirty_init(int mmid);



#endif /* threada/thread.h */
