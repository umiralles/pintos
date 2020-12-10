#ifndef VM_FRAME
#define VM_FRAME

#include <hash.h>
#include "threads/synch.h"
#include "filesys/file.h"
#include "vm/page.h"

struct owners_list_elem {
  struct sup_table_entry *owner; /* sup_table_entry which owns this frame */
  struct list_elem elem;         /* Used to insert into frame's owners list */
};

struct shared_table_entry {
  struct frame_table_entry *ft;
  const struct file *file;
  off_t offset;
  struct hash_elem elem;
};

/* Single row of the frame table */
struct frame_table_entry {
  void *frame;             /* Frame of memory that the data corresponds to */
  struct list owners;      /* The sup table entries which the page belongs to */
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
void ft_insert_entry(struct hash_elem *elem);
struct frame_table_entry *ft_find_entry(const void *frame);
void ft_remove_entry(void *frame);

struct frame_table_entry *ft_get_first(void);

/* Initialise shared_table */
void st_init(void);

/* Manipulation of shared table */
void st_insert_entry(struct hash_elem *elem);
struct shared_table_entry *st_find_entry(const struct file *file, off_t offset);
void st_remove_entry(struct file *file, off_t offset);

/* Access functions for locks */
void ft_lock_acquire(void);
void ft_lock_release(void);
bool ft_lock_held_by_current_thread(void);
void st_lock_acquire(void);
void st_lock_release(void);
bool st_lock_held_by_current_thread(void);

void ft_pin(void *uaddr, unsigned size);
void ft_unpin(void *uaddr, unsigned size);
#endif
