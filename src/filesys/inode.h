#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include "devices/block.h"

struct bitmap;

struct inode_disk_first* new_inode_disk_first(off_t);
struct inode_disk_second* new_inode_disk_second(void);
char* new_zeros_sector(void);
void test_zero_sector_size(void);
int offset_to_sector_with_expand(block_sector_t, off_t);
int offset_to_sector_with_expand_second(block_sector_t, off_t*);
int offset_to_sector(struct inode_disk_first*, off_t);


void inode_init (void);
bool inode_create (block_sector_t, off_t);
struct inode *inode_open (block_sector_t);
struct inode *inode_reopen (struct inode *);
block_sector_t inode_get_inumber (const struct inode *);
void inode_close (struct inode *);
void inode_remove (struct inode *);
off_t inode_read_at (struct inode *, void *, off_t size, off_t offset);
off_t inode_write_at (struct inode *, const void *, off_t size, off_t offset);
void inode_deny_write (struct inode *);
void inode_allow_write (struct inode *);
off_t inode_length (const struct inode *);
void inode_set_byte_length_2(const struct inode *, off_t length);
off_t inode_sector_length(const struct inode *);
block_sector_t inode_to_sector(struct inode*);
#endif /* filesys/inode.h */
