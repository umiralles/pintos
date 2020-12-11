#ifndef USERPROG_EXCEPTION_H
#define USERPROG_EXCEPTION_H

#include <stdbool.h>

/* Page fault error code bits that describe the cause of the exception.  */
#define PF_P 0x1    /* 0: not-present page. 1: access rights violation. */
#define PF_W 0x2    /* 0: read, 1: write. */
#define PF_U 0x4    /* 0: kernel, 1: user process. */

/* Definitions for ease of use in calls to load_frame */
#define WRITE_ACCESS (true)
#define READ_ACCESS (false)
#define USER_ACCESS (true)
#define KERNEL_ACCESS (false)
#define FAULT_ACCESS (true)
#define LOAD_ACCESS (false)

#define run_if_false(function, run_bool) \
  do { if(!run_bool) function; } while (0)

void exception_init (void);
void exception_print_stats (void);

bool load_frame(void *, void *, bool, bool, bool);
struct sup_table_entry *grow_stack(void *, struct sup_table_entry *);

#endif /* userprog/exception.h */
