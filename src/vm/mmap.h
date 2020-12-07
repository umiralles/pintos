#ifndef VM_FRAME
#define VM_FRAME

#include <hash.h>

/* Type used by identifiers in memory mapped files */
typedef uint32_t mapid_t;

/* File mapping struct */
struct mmap_entry {
  mapid_t map_id;          /* The identifier of the map element */
  struct file *file;       /* Pointer to the file being mapped */
  void *addr;              /* The address the file is mapped from*/
  struct hash_elem elem;   /* A hash element used to track the mmap_elem */
};

void mmap_init(void);

struct mmap_entry *mmap_find_entry(mapid_t map_id);

#endif
