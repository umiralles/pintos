#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/interrupt.h"
#include "filesys/file.h"

/* The number of implemented and working system calls in the syscall table */
#define MAX_SYSCALLS (15)
#define ERROR_CODE (-1)

/* Takes the value of the argument pointer provided by get_argument */
#define GET_ARGUMENT_VALUE(frame, type, no)	\
  *((type *) get_argument(frame->esp, no))

/* Function template for a syscall operation 
   arguments and return location found in f */
typedef void (*syscall_func)(struct intr_frame *f);

/* File element struct */
struct file_elem {
  int fd;                  /* File descriptor for the file */
  struct file *file;       /* Pointer to the open file */
  struct list_elem elem;   /* An element to be inserted into the
			      list of files */
};

void syscall_init (void);

/* Used to access the files lock outside of syscall */
void filesys_lock_acquire(void);
void filesys_lock_release(void);

#endif /* userprog/syscall.h */
