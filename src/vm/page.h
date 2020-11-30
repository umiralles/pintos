#ifndef VM_PAGE
#define VM_PAGE

#include <hash.h>
#include "filesys/file.h"

extern struct hash sup_table;

union location {
  size_t block_number;
  struct file_page {
    struct file *file;
    off_t offset;
  } file;
};

struct sup_table_entry {
  union location location;
  void *upage;
  struct hash_elem elem;
  bool empty;
  bool writable;
};

hash_hash_func sup_table_hash_uaddr;
hash_less_func sup_table_cmp_uaddr;


struct sup_table_entry *find_spt_entry(void *uaddr);
void remove_spt_entry(void *uaddr);

#endif
