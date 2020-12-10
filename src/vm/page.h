#ifndef VM_PAGE
#define VM_PAGE

#include <hash.h>
#include "filesys/file.h"
#include "threads/thread.h"

enum sup_entry_type {
	// empty stack page, empty file page, r/o file page, writable file page
	// stack page in swap, stack page in frame
  ZERO_PAGE,               /* Empty file page, either in frame or not stored */
  FILE_PAGE,               /* Data is a file in filesys or frame */
  IN_SWAP_FILE,            /* Data is a file in swap space */
  STACK_PAGE,		   /* Data is a stack page, in frame or in swap */
  MMAPPED_PAGE,		   /* Data is in memory map, in frame or in swap */
  NEW_STACK_PAGE           /* Empty stack page, not stored */
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
  struct thread *owner;		/* Pointer to thread which owns this sup table */
  bool writable;                /* Whether data is writable */
  bool modified;		/* Whether data was modified */
  struct hash_elem elem;        /* Used to store in supplemental page table */
  enum sup_entry_type type;     /* Type of entry (see enum above) */
};

/* Initialise sup_table */
void spt_init(struct hash *);

/* Manipulation of sup_table */
bool create_file_page(void *, struct file *, off_t, bool, size_t,
		      enum sup_entry_type);	      
void create_stack_page(void *);
struct sup_table_entry *spt_find_entry(struct thread *, const void *);
void spt_remove_entry(void *);
void spt_destroy(struct hash *);

#endif
