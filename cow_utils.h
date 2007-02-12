#ifndef COW_UTILS_H
#define COW_UTILS_H

int copy_special(struct cow *cow);
int copy_fifo(struct cow *cow);
int copy_link(struct cow *cow);
int copy_file(struct cow *cow);

#endif
