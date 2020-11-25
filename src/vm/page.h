#ifndef VM_PAGE
#define VM_PAGE

#include <hash.h>

extern struct hash sup_table;

struct sup_table_entry {
  void *location;
  void *upage;
  struct hash_elem elem;
  bool empty;
  bool read_only;
};

hash_hash_func sup_table_hash_uaddr;
hash_less_func sup_table_cmp_uaddr;

#endif
