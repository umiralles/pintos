#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "threads/palloc.h"
#include "filesys/file.h"
#include "vm/frame.h"
#include "vm/page.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
void install_shared_page(struct shared_table_entry *st,
			 struct sup_table_entry *spt);
void *allocate_user_page (void *, enum palloc_flags, bool);


#endif /* userprog/process.h */
