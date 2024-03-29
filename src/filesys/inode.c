#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/cache.h"
#include "threads/malloc.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
#define ID_FIRST_SIZE 126
#define ID_SECOND_SIZE 128

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk_first
  {
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    block_sector_t id_second_table[ID_FIRST_SIZE];      /* Not used. */
  };

struct inode_disk_second
  {
    block_sector_t data_table[ID_SECOND_SIZE];      /* First data sector. */
  };

struct inode_disk_first*
new_inode_disk_first(off_t length) {
	struct inode_disk_first* id_first=calloc(1, sizeof(struct inode_disk_first));

	id_first->length = length;
	id_first->magic = INODE_MAGIC;

	memset(id_first->id_second_table, 0, ID_FIRST_SIZE * sizeof(block_sector_t) );

	return id_first;
}

struct inode_disk_second*
new_inode_disk_second(void) {
	struct inode_disk_second* id_second=calloc(1, sizeof(struct inode_disk_second));

	memset(id_second->data_table, 0, ID_SECOND_SIZE * sizeof(block_sector_t) );

	return id_second;
}

char*
new_zeros_sector(void){
	char* zeros = malloc(BLOCK_SECTOR_SIZE);
	memset(zeros, 0, ID_SECOND_SIZE * sizeof(block_sector_t) );

	return zeros;
}

void
test_zero_sector_size(void){
#ifdef INFO5
	struct buffer_cache* bc2 = get_buffer_cache_value_from_sector(0);
	struct inode_disk_first* id_first=(struct inode_disk_first*)(bc2->data);
	ASSERT(id_first->magic == INODE_MAGIC);
	printf("sector 0 test with length %d\n", id_first->length);
	free(bc2);
#endif
}


int
offset_to_sector_with_expand(block_sector_t id_first_sector, off_t offset){
	int i=0, ret=0;

	struct buffer_cache* bc = get_buffer_cache_value_from_sector(id_first_sector);
	struct inode_disk_first* id_first=(struct inode_disk_first*)(bc->data);
	struct inode_disk_second* zeros=(struct inode_disk_second*)new_zeros_sector();

	ASSERT(id_first->magic == INODE_MAGIC);

#ifdef INFO5
	printf("inode sector at expand_first: %d with length %d\n", id_first_sector, id_first->length );
#endif

	for(i=0; i<ID_FIRST_SIZE; i++){
		if (id_first->id_second_table[i] == 0){
			block_sector_t id_second_sector;
			if(!free_map_allocate (1, &id_second_sector)){
				free(bc);
				free(zeros);
				return -1;
			}
			id_first->id_second_table[i] = id_second_sector;
			write_src_to_buffer_cache_from_sector(id_first_sector, 0, id_first, BLOCK_SECTOR_SIZE);
			write_src_to_buffer_cache_from_sector(id_first->id_second_table[i], 0, zeros, BLOCK_SECTOR_SIZE);
		}

#ifdef INFO5
		printf("id_first idx at expand_first: %d, and its id_second_sector: %d\n", i, id_first->id_second_table[i]);
#endif
		ret = offset_to_sector_with_expand_second(id_first->id_second_table[i], &offset);
		if (ret==-1){
			free(bc);
			free(zeros);
			return -1;
		}
		else if (ret!=0){
			free(bc);
			free(zeros);
			return ret;
		}
	}		

	free(bc);
	free(zeros);
	PANIC("failed to find block_sector_t at offset_to_sector_with_expand");

	return -1;
}

int
offset_to_sector_with_expand_second(block_sector_t id_second_sector, off_t* offset){
	int i=0, ret=0;
	char *zeros = new_zeros_sector();
 	struct buffer_cache* bc = get_buffer_cache_value_from_sector(id_second_sector);
	struct inode_disk_second* id_second=(struct inode_disk_second*)(bc->data);

	for(i=0; i<ID_SECOND_SIZE; i++){
		if (id_second->data_table[i]==0){
			block_sector_t data_sector;
			if(!free_map_allocate (1, &data_sector)){
				free(bc);
				free(zeros);
				return -1;
			}
			id_second->data_table[i] = data_sector;
			write_src_to_buffer_cache_from_sector(id_second_sector, 0, id_second, BLOCK_SECTOR_SIZE);
			write_src_to_buffer_cache_from_sector(id_second->data_table[i], 0, zeros, BLOCK_SECTOR_SIZE);
		}

#ifdef INFO5
		printf("id_second idx : %d -> data_sector %d\n", i, id_second->data_table[i]);
#endif

		if((*offset)==0){
#ifdef INFO5
		printf("id_second idx : %d -> data_sector %d when return\n", i, id_second->data_table[i]);
#endif
			ret = id_second->data_table[i];
			free(bc);
			free(zeros);
			return ret;
		}
		(*offset)--;
	}

	free(bc);
	free(zeros);
	return ret;
}

int
offset_to_sector(struct inode_disk_first* id_first, off_t offset)
{
#ifdef INFO4
	printf("offset_to_sector with offset %d\n", offset);
#endif

	ASSERT(id_first->magic==INODE_MAGIC);

	int i=0, j=0, ret=-1;
	off_t remain_offset=offset;
 	struct buffer_cache* bc=NULL;
	struct inode_disk_second* id_second;
 
	for(i=0; i<ID_FIRST_SIZE; i++){
#ifdef INFO14
		printf("offset_to_sector: %d\n", id_first->id_second_table[i]);
#endif
		if (id_first->id_second_table[i] == 0){
			free(bc);
			break;
		}

		block_sector_t id_second_sector = id_first->id_second_table[i];
		bc = get_buffer_cache_value_from_sector(id_second_sector);	
		id_second = (struct inode_disk_second*)(bc->data);

		for(j=0; j<ID_SECOND_SIZE; j++){
			if(id_second->data_table[j]!=0 && remain_offset==0){
				ret=id_second->data_table[j];
				free(bc);
#ifdef INFO3
	printf("offset_to_sector i: %d, j: %d, ret: %d\n", i, j, ret);
#endif
				return ret;
			}
			if (id_second->data_table[j]!=0){
				remain_offset--;
			}
		}
		free(bc);
	}

//		PANIC("offset is over the length of inode: remain_offset %d\n", remain_offset);

	return -1;
}



/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
  };

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length){
  ASSERT (length >= 0);
  ASSERT (sizeof(struct inode_disk_first) == BLOCK_SECTOR_SIZE)

	struct inode_disk_first* id_first = new_inode_disk_first(length);
	write_src_to_buffer_cache_from_sector(sector, 0, id_first, BLOCK_SECTOR_SIZE);
	ASSERT(id_first->magic==INODE_MAGIC);
	free(id_first);

	if(length == 0){
		return true;
	}

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */

  size_t sectors = bytes_to_sectors (length);
	off_t offset_sectors = sectors-1;
	int data_sector = offset_to_sector_with_expand(sector, offset_sectors);
	if (data_sector==-1){
		return false;
	}
#ifdef INFO5
	printf("inode %d create with sectors %d, length %d\n", sector, sectors, length);
#endif
	ASSERT (data_sector != 0);

	struct inode inode;
	inode.sector = sector;

#ifdef INFO2
	printf("inode length sectors from parameter: %d\n", sectors);
	printf("inode_sector_length: %d\n", inode_sector_length(&inode));
#endif

	ASSERT(sectors == (size_t)inode_sector_length(&inode));

  return true;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

#ifdef INFO12
	printf("inode_open newly: sector %d\n", sector);
#endif

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL){
#ifdef INFO12
	printf("inode_open newly: failed to malloc\n");
#endif
    return NULL;
	}
  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;


	/* Load to Buffer Cache */
	struct buffer_cache* bc=get_buffer_cache_value_from_sector(sector);	
	free(bc);

  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode* inode){
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
					struct buffer_cache* bc = get_buffer_cache_value_from_sector(inode->sector);
			  	struct inode_disk_first* id_first = (struct inode_disk_first*)(bc->data);

					ASSERT(id_first->magic==INODE_MAGIC);

					int i = 0, length = inode_sector_length(inode);
					for (i = 0; i < length; i++) {
						block_sector_t sector = offset_to_sector(id_first, i);
						free_map_release(sector, 1);
					}

					block_sector_t second_t;
					for (i = 0; i < ID_FIRST_SIZE; i++) {
						second_t=id_first->id_second_table[i];
						if(second_t!=0)
          		free_map_release (second_t, 1);
					}

          free_map_release (inode->sector, 1);
					free(bc);
        }
#ifdef INFO16
			printf("inode_close: sector %d\n", inode->sector);
#endif
      free (inode); 
    }

}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) {
	uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
	off_t offset_sector = 0;
	struct buffer_cache* bc;
 	struct inode_disk_first* id_first;

#ifdef INFO3
	printf("call inode_read_at_2 at sector %d\n", inode->sector);
#endif
  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
#ifdef INFO
			printf("%p, offset: %d, size: %d, bytes_read: %d at read\n", inode,  offset, size, bytes_read );
#endif
			bc = get_buffer_cache_value_from_sector(inode->sector);
			id_first = (struct inode_disk_first*)(bc->data);
			ASSERT(id_first->magic==INODE_MAGIC);

			offset_sector = offset / BLOCK_SECTOR_SIZE;
      int sector_idx = offset_to_sector (id_first, offset_sector);
			free(bc);
			if (sector_idx < 0)
				break;

      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
#ifdef INFO
			printf("chunk_size %d, sector_idx %d, inode_length %d\n", chunk_size, sector_idx, inode_length(inode));
#endif
      if (chunk_size <= 0)
        break;

			bc = get_buffer_cache_value_from_sector(sector_idx);
#ifdef INFO
			printf("bc selected when read inode at sector_idx %d\n", sector_idx);
#endif

     if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
					memcpy(buffer + bytes_read, bc->data, BLOCK_SECTOR_SIZE);
        }
      else 
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          memcpy (buffer + bytes_read, bc->data + sector_ofs, chunk_size);
        }
     	free(bc); 
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }

#ifdef INFO
	printf("inode_read at is finished\n");
#endif

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) {
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
	off_t offset_sector = 0;
	off_t init_offset = offset;

  if (inode->deny_write_cnt)
    return 0;

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
			offset_sector = offset / BLOCK_SECTOR_SIZE;
      int sector_idx = offset_to_sector_with_expand (inode->sector, offset_sector);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

			if (sector_idx==-1)
				break;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
  //    off_t inode_left = inode_length (inode) - offset;
//      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
//      int min_left = inode_left < sector_left ? inode_left : sector_left;
			int min_left = BLOCK_SECTOR_SIZE - sector_ofs;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;

#ifdef INFO6
//			printf("inode_write_at_2: inode %x, buffer %x, size %d, offset %d, offset_sector %d, sector_idx %d, sector_ofs %d, inode_left %d, sector_left %d, min_left %d, chunk_size %d\n", inode, buffer, size, offset, offset_sector, sector_idx, sector_ofs, inode_left, sector_left, min_left, chunk_size);
			printf("inode_write_at_2: inode %x, buffer %x, size %d, offset %d, offset_sector %d, sector_idx %d, sector_ofs %d, min_left %d, chunk_size %d\n", inode, buffer, size, offset, offset_sector, sector_idx, sector_ofs, min_left, chunk_size);
#endif

      if (chunk_size <= 0)
        break;

#ifdef INFO5
			printf("bc selected when write inode at sector_idx %d\n", sector_idx);
#endif

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
					write_src_to_buffer_cache_from_sector(sector_idx, 0, buffer+bytes_written, BLOCK_SECTOR_SIZE);
       }
      else 
        {
					write_src_to_buffer_cache_from_sector(sector_idx, sector_ofs, buffer+bytes_written, chunk_size);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }


	if (inode_length(inode) < init_offset + bytes_written)
		inode_set_byte_length_2(inode, init_offset+bytes_written);


  return bytes_written;
}


/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode){
	struct buffer_cache* bc = get_buffer_cache_value_from_sector(inode->sector);	
	struct inode_disk_first* id = (struct inode_disk_first*)(bc->data);
	ASSERT(id->magic==INODE_MAGIC);
	off_t ret = id->length;

	free(bc);

	return ret;
}

void
inode_set_byte_length_2(const struct inode *inode, off_t length)
{
	struct buffer_cache* bc = get_buffer_cache_value_from_sector(inode->sector);	
	struct inode_disk_first* id = (struct inode_disk_first*)(bc->data);
	ASSERT(id->magic==INODE_MAGIC);
	id->length = length;

	write_src_to_buffer_cache_from_sector(inode->sector, 0, id, BLOCK_SECTOR_SIZE);

	free(bc);
}

off_t
inode_sector_length(const struct inode* inode){
	int i=0, j=0;
	off_t length=0;
 	struct buffer_cache* bc;
	struct inode_disk_first* id_first;
	struct inode_disk_second* id_second;

#ifdef INFO2
	printf("inode sector at inode_length: %d\n", inode->sector);
#endif

	bc = get_buffer_cache_value_from_sector(inode->sector);	
	id_first = (struct inode_disk_first*)(bc->data);

	ASSERT(id_first->magic==INODE_MAGIC);
 
	for(i=0; i<ID_FIRST_SIZE; i++){
		if (id_first->id_second_table[i] == 0){
			free(bc);
			return length;
		}

		block_sector_t id_second_sector = id_first->id_second_table[i];
		struct buffer_cache* bc2 = get_buffer_cache_value_from_sector(id_second_sector);	
		id_second = (struct inode_disk_second*)(bc2->data);

		for(j=0; j<ID_SECOND_SIZE; j++){
#ifdef INFO2
		printf("id_first idx at inode_length: %d, its second sector: %d\n", i, id_first->id_second_table[i]);
		printf("id_second idx at inode_length: %d, its data sector: %d\n", j, id_second->data_table[j]);
#endif
			if (id_second->data_table[j]!=0)
				length++;
			else{
				free(bc);
				free(bc2);
				return length;
			}
		}

		free(bc2);		
	}

	// max block size 504 * 512
	ASSERT (length == 258048);
	free(bc); 
	return length;
}


block_sector_t inode_to_sector(struct inode* inode) {
	return inode->sector;
}
