#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/interrupt.h"

/* the number of implemented and working system calls in the syscall table */
#define MAX_SYSCALLS (13)

/* function template for a syscall operation 
   arguments and return location found in f
*/
typedef void (*syscall_func)(struct intr_frame *f);
void syscall_init (void);

#endif /* userprog/syscall.h */
