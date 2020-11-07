#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/interrupt.h"

/* the number of implemented and working system calls in the syscall table */
#define MAX_SYSCALLS (13)

/* Takes the value of the argument pointer provided by get_argument */
#define GET_ARGUMENT_VALUE(frame, type, no)	\
  *((type *) get_argument(frame->esp, no))

/* function template for a syscall operation 
   arguments and return location found in f
*/
typedef void (*syscall_func)(struct intr_frame *f);
void syscall_init (void);

#endif /* userprog/syscall.h */
