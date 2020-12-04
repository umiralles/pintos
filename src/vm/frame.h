#ifndef VM_FRAME
#define VM_FRAME

#include <hash.h>
#include "threads/synch.h"

/* TODO Note: The frame_table is now static, I hope this is alright. */
/* Frame table */
//extern struct hash frame_table;

/* Single row of the frame table */
struct frame_table_entry {
  void *uaddr;           /* user virtual address frame represents */
  void *frame;           /* frame of memory that the data corresponds to */
  struct thread *owner;  /* thread which the page belongs to */
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
struct frame_table_entry *ft_find_entry(void *uaddr);
void ft_remove_entry(void *uaddr);

/* Access functions for frame_table_lock */
void ft_lock_acquire(void);
void ft_lock_release(void);

#endif
