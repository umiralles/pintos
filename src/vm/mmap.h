#ifndef VM_MMAP
#define VM_MMAP

#include <hash.h>
#include "filesys/file.h"

#define ERROR_CODE (-1)

/* Type used by identifiers in memory mapped files */
typedef uint32_t mapid_t;

/* File mapping struct */
struct mmap_entry {
  mapid_t map_id;          /* The identifier of the map element */
  struct file *file;       /* Pointer to the file being mapped */
  struct hash_elem elem;   /* A hash element used to track the mmap_elem */
};

/* Initialise mmap_table and map_table_lock (stored in mmap.c) */
void mmap_init(struct hash *);

/* Manipulation of mmap_table */
mapid_t mmap_create_entry(struct file *);
struct mmap_entry *mmap_find_entry(mapid_t);
void mmap_remove_entry(mapid_t);

#endif
