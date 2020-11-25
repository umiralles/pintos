#ifndef VM_FRAME
#define VM_FRAME

#include <hash.h>

/* frame table */
extern struct hash frame_table;

/* single row of the frame table */
struct frame_table_elem {
  void *uaddr;           /* user virtual address frame represents */
  void *frame;           /* frame of memory that the data corresponds to */
  int64_t timestamp;     /* time the frame was allocated in ticks */
  struct hash_elem elem; /* used to insert into the table */
  struct list_elem delete_elem; /* TODO: find a better way of freeing these */
  bool reference_bit;    /* used for second chance algorithm calculations */
  bool modified;         /* states whether the frame has been modified */
};

hash_hash_func hash_user_address;
hash_less_func cmp_timestamp;

#endif
