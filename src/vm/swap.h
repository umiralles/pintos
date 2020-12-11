#ifndef VM_SWAP
#define VM_SWAP
#include <bitmap.h>
#include <stddef.h>

void swap_init(void);
size_t find_swap_space(size_t);
void remove_swap_space(size_t, size_t);
void swap_write_frame(void *, size_t);
void swap_read_frame(void *, size_t);
void swap_read_file(struct file *, size_t, size_t);
void swap_lock_acquire(void);
void swap_lock_release(void);
bool swap_lock_held_by_current_thread(void);

#endif
