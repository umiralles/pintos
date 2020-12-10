#ifndef VM_FRAME
#define VM_FRAME

#include <hash.h>
#include "threads/synch.h"
#include "filesys/file.h"
#include "vm/page.h"

/* Single row of the shared table */
struct shared_table_entry {
  struct frame_table_entry *ft; /* Frame which the file is mapped to */
  const struct file *file;      /* Read only file that is shared */
  off_t offset;                 /* Offset of the file segment that is shared */
  struct hash_elem elem;        /* Used to insert into the table */
};

/* Single row of the frame table */
struct frame_table_entry {
  void *frame;             /* Frame of memory that the data corresponds to */
  struct list owners;      /* The sup table entries that the page belongs to */
  struct lock owners_lock; /* Restricts access to owners list */
  int64_t timestamp;       /* Time the frame was allocated in ticks */
  struct hash_elem elem;   /* Used to insert into the table */
  bool reference_bit;      /* Used for second chance algorithm calculations */
  bool modified;           /* States whether the frame has been modified */
  bool writable;           /* Whether the thread can be written to or not */
  bool pinned;		   /* True if frame is not able to be evicted */
};

/* Initialise frame_table */
void ft_init(void);

/* Manipulation of frame_table */
void ft_insert_entry(struct hash_elem *);
struct frame_table_entry *ft_find_entry(const void *);
void ft_remove_entry(void *);
void ft_pin(const void *, unsigned);
void ft_unpin(const void *, unsigned);
void ft_reset_reference_bits(void);

/* Page replacement algorithm */
struct frame_table_entry *ft_get_victim(void);

/* Initialise shared_table */
void st_init(void);

/* Manipulation of shared table */
void st_insert_entry(struct hash_elem *);
struct shared_table_entry *st_find_entry(const struct file *, off_t);
void st_remove_entry(struct file *, off_t);

/* Access functions for locks */
void ft_lock_acquire(void);
void ft_lock_release(void);
bool ft_lock_held_by_current_thread(void);
void st_lock_acquire(void);
void st_lock_release(void);
bool st_lock_held_by_current_thread(void);

#endif
