#ifndef VM_PAGE
#define VM_PAGE

#include <hash.h>
#include "filesys/file.h"
#include "threads/thread.h"

enum sup_entry_type {
  ZERO_PAGE,               /* Data cannot be expected to be found there */
  FILE_PAGE,               /* Data is in filesys */
  IN_SWAP,                 /* Data is in swap space */
  STACK_PAGE,		    /* Data is in stack */
  MMAPPED_PAGE		    /* Data is in memory map */
};

struct sup_table_entry {
  size_t block_number; 	    /* Where page data can be found after an eviction */
  struct file *file;        /* File pointer */
  off_t offset;	            /* Offset of page data in file */
  size_t read_bytes;        /* Number of bytes to be read from file */
  void *upage;              /* User page the entry represents */
  bool writable;            /* Whether data is writable, 
			       also used to determine ????? 
			       WHAT?!?! the suspense is kiling me*/
  struct hash_elem elem;    /* Used to store in the supplemental page table */
  enum sup_entry_type type; /* Type of entry (see enum above) */
};

/* Initialise sup_table */
void spt_init(struct hash *sup_table);

/* Manipulation of sup_table */
bool create_file_page(void *, struct file *, off_t, bool, size_t,
		      enum sup_entry_type);	      
void create_stack_page(void *);
struct sup_table_entry *spt_find_entry(struct thread *, void *);
void spt_remove_entry(void *);

/* Hash functions */
hash_action_func spt_destroy_entry;

#endif
