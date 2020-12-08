#include "vm/swap.h"
#include <bitmap.h>
#include <stddef.h>
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "devices/block.h"
#include "filesys/file.h"

/* Swap table */
struct bitmap *swap_table;
static struct lock swap_table_lock;

void swap_init() {
  swap_table = bitmap_create(block_size(
                          block_get_role(BLOCK_SWAP)) / BLOCK_SECTOR_SIZE);
  lock_init(&swap_table_lock);
}

/* Finds space in swap table for cnt adjascent pages 
   Returns the index of the first sector of the allocated swap space
   MUST ACQUIRE THE SWAP TABLE LOCK BEFORE CALLING */
size_t find_swap_space(size_t cnt) {
  return bitmap_scan_and_flip(swap_table, 0,
		  cnt * PGSIZE / BLOCK_SECTOR_SIZE, true);
}

/* Frees space for cnt pages starting from sector index start
   Takes a starting sector index and a number of pages to remove 
   MUST ACQUIRE THE SWAP TABLE LOCK BEFORE CALLING */
void remove_swap_space(size_t start, size_t cnt) {
  bitmap_set_multiple(swap_table, start,
		  cnt * PGSIZE / BLOCK_SECTOR_SIZE, false);
}

/* Writes a frame of data into the swap space 
   Takes the frame to write and the start sector to write to
   MUST ACQUIRE THE SWAP TABLE LOCK BEFORE CALLING */
void swap_write_frame(void *frame, size_t start) {
  struct block *b = block_get_role(BLOCK_SWAP); 
  for (block_sector_t i = start; i < start + PGSIZE / BLOCK_SECTOR_SIZE; i++) {
    block_write(b, i, (frame + i * BLOCK_SECTOR_SIZE));
  }
}

/* Reads a page of data into a frame from the swap space 
   Takes the frame to write to and the start sector to read from
   MUST ACQUIRE THE SWAP TABLE LOCK BEFORE CALLING */
void swap_read_frame(void *frame, size_t start) {
  struct block *b = block_get_role(BLOCK_SWAP); 
  for (block_sector_t i = start; i < start + PGSIZE / BLOCK_SECTOR_SIZE; i++) {
    block_read(b, i, (frame + i * BLOCK_SECTOR_SIZE));
  }
}

/* Reads a page of data from swap space into a given file 
   Takes a file to write into and a sector index to read from
   MUST ACQUIRE THE FILESYS AND SWAP TABLE LOCKS BEFORE CALLING */
void swap_read_file(struct file *file, size_t start){
  void *buffer = malloc(BLOCK_SECTOR_SIZE);
  struct block *b = block_get_role(BLOCK_SWAP);
  for (block_sector_t i = start; i < start + PGSIZE / BLOCK_SECTOR_SIZE; i++) {
    block_read(b, i, (buffer + i * BLOCK_SECTOR_SIZE));
    file_write(file, buffer, BLOCK_SECTOR_SIZE);
  }
  free(buffer);
}

/* Acquire the swap table lock */
void swap_lock_acquire(void) {
  lock_acquire(&swap_table_lock);
}

/* Release the swap table lock */
void swap_lock_release(void) {
  lock_release(&swap_table_lock);
}
