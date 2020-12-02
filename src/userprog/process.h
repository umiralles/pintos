#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "threads/palloc.h"
#include "filesys/file.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
void *allocate_user_page (void* uaddr, enum palloc_flags flags,
		bool writable);
void create_file_page(void *upage, struct file *file, off_t offset,
		bool writable);
void create_stack_page(void *upage);


#endif /* userprog/process.h */
