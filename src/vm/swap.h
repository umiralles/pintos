#ifndef VM_SWAP
#define VM_SWAP
#include <bitmap.h>
#include <stddef.h>

extern struct bitmap *swap_table;

size_t find_swap_space(size_t cnt);
void remove_swap_space(size_t start, size_t cnt);

#endif
