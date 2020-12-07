#ifndef VM_SWAP
#define VM_SWAP
#include <bitmap.h>
#include <stddef.h>

/* Swap table */
extern struct bitmap *swap_table;

size_t find_swap_space(size_t cnt);
void remove_swap_space(size_t start, size_t cnt);
void swap_write_frame(void *frame, size_t start);
void swap_read_frame(void *frame, size_t start);
void swap_read_file(struct file *file, size_t start);
void swap_lock_acquire(void);
void swap_lock_release(void);

#endif
