#ifndef VM_FRAME
#define VM_FRAME

#include <hash.h>
#include "filesys/file.h"

#define ERROR_CODE (-1)

/* Type used by identifiers in memory mapped files */
typedef uint32_t mapid_t;

/* File mapping struct */
struct mmap_entry {
  mapid_t map_id;          /* The identifier of the map element */
  void *addr;              /* The address the file is mapped from */
  struct file *file;       /* Pointer to the file being mapped */
  off_t length;            /* Length of the file */
  struct hash_elem elem;   /* A hash element used to track the mmap_elem */
};

/* Initialise mmap_table and map_table_lock (stored in mmap.c) */
void mmap_init(void);

/* Manipulation of mmap_table */
mapid_t mmap_create_entry(void *addr, struct file *file, off_t length);
struct mmap_entry *mmap_find_entry(mapid_t map_id);
void mmap_remove_entry(mapid_t map_id);

/* Access functions for mmap_table_lock */
void mmap_lock_acquire(void);
void mmap_lock_release(void);

#endif
