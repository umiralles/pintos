#ifndef VM_PAGE
#define VM_PAGE

#include <hash.h>
#include "filesys/file.h"
#include "threads/thread.h"

extern struct hash sup_table;

struct sup_table_entry {
  size_t block_number; 	   /* Where page data can be found after an eviction */
  struct file *file;       /* File pointer */
  off_t offset;	    /* Offset of page data in file */
  void *upage;             /* User page the entry represents */
  struct hash_elem elem;   /* Used to store in the supplemental page table */ 
  bool empty;              /* Whether data can be expected to be found there */
  bool writable;           /* Whether data is writable, also used to determine */
  bool in_swap;	    /* Whether data is in swap space */			      
};

/* Hash functions */
hash_hash_func sup_table_hash_uaddr;
hash_less_func sup_table_cmp_uaddr;
hash_action_func destroy_spt_entry;


struct sup_table_entry *find_spt_entry(struct thread *t, void *uaddr);
void remove_spt_entry(void *uaddr);

#endif
