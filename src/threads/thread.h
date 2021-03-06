#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include <hash.h>
#include "synch.h"
#include "threads/fixed-point.h"

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

#ifdef USERPROG
/* Element for use in a list of child processes of a parent thread.
   Used in the thread struct and userprog/process.c */
struct tid_elem {
  struct list_elem elem;              /* Element to store in a list */
  struct semaphore child_semaphore;   /* Semaphore used in process_wait to
					 halt the parent thread and to wait
					 for child to load executable */
  struct lock tid_elem_lock;          /* Lock shared between parent and
					 child */
  tid_t tid;                          /* tid of the child process */
  int exit_status;                    /* Exit status of child thread */
  bool process_dead;		      /* True if one of the processes
					 terminated */
  bool has_faulted;		      /* True when process exits erroneously
					 duing startup */
};
#endif

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
    char name[16];                      /* Name (for debugging
					   purposes). */
    uint8_t *stack;                     /* Saved stack pointer. */
    int priority;                       /* Priority. */
    int effective_priority;             /* Base priority prior to donations */
    int nice;                           /* Niceness */
    fixed_point_number recent_cpu;      /* Recent CPU usage */
    struct list_elem allelem;           /* List element for all threads
					   list. */
    struct list donating_threads;	/* List of threads that donated to
					   this thread */
    struct list_elem donations_elem;	/* List elem for list of donations */
    struct lock *waiting_lock;		/* Lock on which thread is blocked */
    struct semaphore donations_sema;    /* Controls access to 
					   donating_threads */

    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */

#ifdef USERPROG
    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                  /* Page directory. */
    struct file *executable;            /* Open executable file */
    struct list files;			/* List of file descriptors */
    int next_available_fd;              /* Next available file descriptor */
    struct list child_tid_list;         /* List of tid_elem structs
					   corresponding to children */
    struct tid_elem *tid_elem;          /* Pointer to this thread's
					   tid_elem in its parent's
					   child_tid_list */
    struct list allocated_pointers;     /* List of pointers to be freed on
					   exit */
    void *curr_esp;                     /* Current esp of last user thread */
    struct hash sup_table;              /* Supplemental page table */
    struct hash mmap_table;             /* Memory mapped files table */
    int next_map_id;                    /* Next available memory map id */
    int stack_page_cnt;                 /* Number of stack pages added. */
#endif

    /* Owned by thread.c. */
    unsigned magic;                     /* Detects stack overflow. */
  };

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "mlfqs". */
extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);
size_t threads_ready(void);

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

/* Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func (struct thread *t, void *aux);
void thread_foreach (thread_action_func *, void *);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);
list_less_func cmp_priority;

/* Donations function. */
void donation_grant(struct lock *lock, int priority);
void donation_revoke(struct lock *lock);

#endif /* threads/thread.h */
