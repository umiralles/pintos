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
