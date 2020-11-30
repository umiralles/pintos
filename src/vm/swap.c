#include "vm/swap.h"
#include <bitmap.h>
#include <stddef.h>
#include "threads/vaddr.h"
#include "devices/block.h"

struct bitmap *swap_table;

size_t find_swap_space(size_t cnt) {
  return bitmap_scan_and_flip(swap_table, 0,
		  cnt * PGSIZE / BLOCK_SECTOR_SIZE, true);
}

void remove_swap_space(size_t start, size_t cnt) {
  bitmap_set_multiple(swap_table, start,
		  cnt * PGSIZE / BLOCK_SECTOR_SIZE, false);
}

void swap_write_frame(void *frame, size_t start) {
  struct block *b = block_get_role(BLOCK_SWAP); 
  for (block_sector_t i = start; i < start + PGSIZE / BLOCK_SECTOR_SIZE; i++) {
    block_write(b, i, (frame + i * BLOCK_SECTOR_SIZE));
  }
}

void swap_read_frame(void *frame, size_t start) {
  struct block *b = block_get_role(BLOCK_SWAP); 
  for (block_sector_t i = start; i < start + PGSIZE / BLOCK_SECTOR_SIZE; i++) {
    block_read(b, i, (frame + i * BLOCK_SECTOR_SIZE));
  }
}
