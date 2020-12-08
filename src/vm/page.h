#ifndef VM_PAGE
#define VM_PAGE

#include <hash.h>
#include "filesys/file.h"
#include "threads/thread.h"

enum sup_entry_type {
  ZERO_PAGE,               /* Data cannot be expected to be found there */
  FILE_PAGE,               /* Data is in filesys */
  IN_SWAP,                 /* Data is in swap space */
  STACK_PAGE,		   /* Data is in stack */
  NEW_STACK_PAGE          /* Data will be in stack, but currently empty */
};

struct sup_table_entry {
  size_t block_number;	        /* Block number of swap space data if present */
  struct file *file;            /* File pointer */
  off_t offset;	                /* Offset of page data in file */
  size_t read_bytes;            /* Number of bytes to be read from file */
  void *upage;                  /* User page the entry represents */
  struct frame_table_entry *ft; /* Frame where page is loaded, 
				   NULL if not loaded */
  struct list_elem frame_elem;  /* Used to insert into frame's owners list */
  bool writable;                /* Whether data is writable */
  struct hash_elem elem;        /* Used to store in supplemental page table */
  enum sup_entry_type type;     /* Type of entry (see enum above) */
};

void spt_init(struct hash *sup_table);
void spt_destroy(struct hash *sup_table);
void create_file_page(void *upage, struct file *file, off_t offset,
		      bool writable, size_t read_bytes,
		      enum sup_entry_type type);
void overwrite_file_page(struct sup_table_entry *spt, void *upage, 
			  struct file *file, off_t offset, bool writable, 
			  size_t read_bytes, enum sup_entry_type type);		      
void create_stack_page(void *upage);

struct sup_table_entry *spt_find_entry(struct thread *t, void *uaddr);

#endif
