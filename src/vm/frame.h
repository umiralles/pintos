#ifndef VM_FRAME
#define VM_FRAME

#include <hash.h>
#include "threads/synch.h"
#include "filesys/file.h"

/* TODO Note: The frame_table is now static, I hope this is alright. */
/* Frame table */
//extern struct hash frame_table;

struct owners_list_elem {
  struct thread *owner;         /* thread which owns this frame */
  struct list_elem elem;  /* Used to insert into frame's owners list */
};

struct shared_table_entry {
  struct frame_table_entry *ft;
  struct file *file;
  struct hash_elem elem;
};

/* Single row of the frame table */
struct frame_table_entry {
  void *uaddr;           /* user virtual address frame represents */
  void *frame;           /* frame of memory that the data corresponds to */
  struct list owners;   /* threads which the page belongs to */
  int64_t timestamp;     /* time the frame was allocated in ticks */
  struct hash_elem elem; /* used to insert into the table */
  bool reference_bit;    /* used for second chance algorithm calculations */
  bool modified;         /* states whether the frame has been modified */
  bool writable;         /* whether the thread can be written to or not */
};

/* Initialise frame_table */
void ft_init(void);

/* Manipulation of frame_table */
void ft_insert_entry(struct hash_elem *elem);
struct frame_table_entry *ft_find_entry(const void *uaddr);
void ft_remove_entry(void *uaddr);


/* Initialise shared_table */
void st_init(void);

/* Manipulation of shared table */
void st_insert_entry(struct hash_elem *elem);
struct shared_table_entry *st_find_entry(const struct file *file);
void st_remove_entry(struct file *file);

/* Access functions for locks */
void ft_lock_acquire(void);
void ft_lock_release(void);
void st_lock_acquire(void);
void st_lock_release(void);

#endif
