#ifndef VM_PAGE
#define VM_PAGE

#include <hash.h>
#include "filesys/file.h"

extern struct hash sup_table;

union location {
  size_t block_number; /* Index of block in swap space */
  struct file_page {   /* Data about read only file */
    struct file *file;
    off_t offset;
  } file;
};

struct sup_table_entry {
  union location location; /* Where page data can be found after an eviction */
  void *upage;             /* User page the entry represents */
  struct hash_elem elem;   /* Used to store in the supplemental page table */ 
  bool empty;              /* Whether data can be expected to be found there */
  bool writable;           /* Whether data is writable, also used to determine
			      whether data in swap space or file system */
};

/* Hash functions */
hash_hash_func sup_table_hash_uaddr;
hash_less_func sup_table_cmp_uaddr;
hash_action_func destroy_spt_entry;


struct sup_table_entry *find_spt_entry(void *uaddr);
void remove_spt_entry(void *uaddr);

#endif
