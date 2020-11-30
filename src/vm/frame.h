#ifndef VM_FRAME
#define VM_FRAME

#include <hash.h>
#include "threads/synch.h"

/* Frame table */
extern struct hash frame_table;

/* Lock for frame table */
extern struct lock frame_table_lock;

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

struct frame_table_entry *find_ft_entry(void *uaddr);
void remove_ft_entry(void *uaddr);

/* Hash functions */
hash_hash_func hash_user_address;
hash_less_func cmp_timestamp;

#endif
