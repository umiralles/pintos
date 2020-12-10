#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "threads/palloc.h"
#include "filesys/file.h"
#include "vm/frame.h"
#include "vm/page.h"

#define PALLOC_PTR (true)
#define MALLOC_PTR (false)

/* Element to store a pointer (used for allocated_pointers list) */
struct pointer_elem {
  void *pointer;                      /* Pointer to be freed on exit */
  bool palloc;                        /* True: palloc, False: malloc */
  struct list_elem elem;              /* Element to store in a list */
};

tid_t process_execute (const char *);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

/* */
void install_shared_page(struct shared_table_entry *, struct sup_table_entry *);
void *allocate_user_page (void *, enum palloc_flags, bool);

/* Manipulate the thread's allocated_pointers list */
void create_alloc_elem(void *, bool);
void remove_alloc_elem(void *);


#endif /* userprog/process.h */
