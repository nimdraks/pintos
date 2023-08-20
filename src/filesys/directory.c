#include "filesys/directory.h"
#include <stdio.h>
#include <string.h>
#include <list.h>
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/malloc.h"
#include "threads/thread.h"

char DIR_DELIMIT = '/';
char* DIR_DELIMIT_STR = "/";


/* A directory. */
struct dir 
  {
    struct inode *inode;                /* Backing store. */
    off_t pos;                          /* Current position. */
  };

/* A single directory entry. */
struct dir_entry 
  {
    block_sector_t inode_sector;        /* Sector number of header. */
    char name[NAME_MAX + 1];            /* Null terminated file name. */
    bool in_use;                        /* In use or free? */
		bool is_dir;
  };

/* Creates a directory with space for ENTRY_CNT entries in the
   given SECTOR.  Returns true if successful, false on failure. */
bool
dir_create (block_sector_t sector, size_t entry_cnt, block_sector_t parent)
{
	bool success = inode_create (sector, entry_cnt * sizeof (struct dir_entry));
	if (!success)
		return false;

	struct dir* dir = dir_open(inode_open(sector));
	if (dir == NULL){
		PANIC("failed to inode_open at dir_create()");
	}

	success = dir_add (dir, "..", parent, true);
	if (success==false){
		PANIC("failed to .. at dir_add");
	}

	success = dir_add (dir, ".", sector, true);
	if (success==false){
		PANIC("failed to . at dir_add");
	}

	return true;
}

bool
dir_create_root (block_sector_t sector, size_t entry_cnt)
{
	return dir_create(sector, entry_cnt, thread_current()->cwd_sector);
}

/* Opens and returns the directory for the given INODE, of which
   it takes ownership.  Returns a null pointer on failure. */
struct dir *
dir_open (struct inode *inode) 
{
  struct dir *dir = calloc (1, sizeof *dir);
  if (inode != NULL && dir != NULL)
    {
      dir->inode = inode;
      dir->pos = 0;
      return dir;
    }
  else
    {
      inode_close (inode);
      free (dir);
      return NULL; 
    }
}

/* Opens the root directory and returns a directory for it.
   Return true if successful, false on failure. */
struct dir *
dir_open_root (void)
{
  return dir_open (inode_open (ROOT_DIR_SECTOR));
}

/* Opens and returns a new directory for the same inode as DIR.
   Returns a null pointer on failure. */
struct dir *
dir_reopen (struct dir *dir) 
{
  return dir_open (inode_reopen (dir->inode));
}

/* Destroys DIR and frees associated resources. */
void
dir_close (struct dir *dir) 
{
  if (dir != NULL)
    {
      inode_close (dir->inode);
      free (dir);
    }
}

/* Returns the inode encapsulated by DIR. */
struct inode *
dir_get_inode (struct dir *dir) 
{
  return dir->inode;
}

/* Searches DIR for a file with the given NAME.
   If successful, returns true, sets *EP to the directory entry
   if EP is non-null, and sets *OFSP to the byte offset of the
   directory entry if OFSP is non-null.
   otherwise, returns false and ignores EP and OFSP. */
static bool
lookup (const struct dir *dir, const char *name,
        struct dir_entry *ep, off_t *ofsp) 
{
  struct dir_entry e;
  size_t ofs;
  
  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) 
    if (e.in_use && !strcmp (name, e.name)) 
      {
        if (ep != NULL)
          *ep = e;
        if (ofsp != NULL)
          *ofsp = ofs;
        return true;
      }
  return false;
}

/* Searches DIR for a file with the given NAME
   and returns true if one exists, false otherwise.
   On success, sets *INODE to an inode for the file, otherwise to
   a null pointer.  The caller must close *INODE. */
bool
dir_lookup (const struct dir *dir, const char *name,
            struct inode **inode) 
{
  struct dir_entry e;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  if (lookup (dir, name, &e, NULL))
    *inode = inode_open (e.inode_sector);
  else
    *inode = NULL;

  return *inode != NULL;
}

bool
dir_is_dir (const struct dir *dir, const char *name) 
{
  struct dir_entry e;
	bool is_dir=false;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  if (lookup (dir, name, &e, NULL)){
		is_dir = e.is_dir;
	}

  return is_dir;
}

/* Adds a file named NAME to DIR, which must not already contain a
   file by that name.  The file's inode is in sector
   INODE_SECTOR.
   Returns true if successful, false on failure.
   Fails if NAME is invalid (i.e. too long) or a disk or memory
   error occurs. */
bool
dir_add (struct dir *dir, const char *name, block_sector_t inode_sector, bool is_dir)
{
  struct dir_entry e;
  off_t ofs;
  bool success = false;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Check NAME for validity. */
  if (*name == '\0' || strlen (name) > NAME_MAX)
    return false;

  /* Check that NAME is not in use. */
  if (lookup (dir, name, NULL, NULL))
    goto done;

  /* Set OFS to offset of free slot.
     If there are no free slots, then it will be set to the
     current end-of-file.
     
     inode_read_at() will only return a short read at end of file.
     Otherwise, we'd need to verify that we didn't get a short
     read due to something intermittent such as low memory. */
  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) 
    if (!e.in_use)
      break;

  /* Write slot. */
  e.in_use = true;
  strlcpy (e.name, name, sizeof e.name);
  e.inode_sector = inode_sector;
	if (is_dir){
		e.is_dir = is_dir;
	}

  success = inode_write_at (dir->inode, &e, sizeof e, ofs) == sizeof e;

 done:
  return success;
}


bool
dir_add_file (struct dir *dir, const char *name, block_sector_t inode_sector)
{
	return dir_add(dir, name, inode_sector, false);
}

/* Removes any entry for NAME in DIR.
   Returns true if successful, false on failure,
   which occurs only if there is no file with the given NAME. */
bool
dir_remove (struct dir *dir, const char *name) 
{
  struct dir_entry e;
  struct inode *inode = NULL;
  bool success = false;
  off_t ofs;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Find directory entry. */
  if (!lookup (dir, name, &e, &ofs))
    goto done;

  /* Open inode. */
  inode = inode_open (e.inode_sector);
  if (inode == NULL)
    goto done;

  /* Erase directory entry. */
  e.in_use = false;
  if (inode_write_at (dir->inode, &e, sizeof e, ofs) != sizeof e) 
    goto done;

  /* Remove inode. */
  inode_remove (inode);
  success = true;

 done:
  inode_close (inode);
  return success;
}

/* Reads the next directory entry in DIR and stores the name in
   NAME.  Returns true if successful, false if the directory
   contains no more entries. */
bool
dir_readdir (struct dir *dir, char name[NAME_MAX + 1])
{
  struct dir_entry e;

  while (inode_read_at (dir->inode, &e, sizeof e, dir->pos) == sizeof e) 
    {
      dir->pos += sizeof e;
      if (e.in_use)
        {
          strlcpy (name, e.name, NAME_MAX + 1);
          return true;
        } 
    }
  return false;
}


struct dir *
dir_open_recursive (const char* path) {
	int token_count = count_token(path, DIR_DELIMIT_STR);
	if (token_count == 0)
		return NULL;

	struct dir* curr=NULL;
	if (is_absolute(path))
		curr = dir_open_root();
 	else 
		curr = dir_open(inode_open(thread_current()->cwd_sector));

#ifdef INFO7
	printf("dir_open_recursive: curr %d, token_count %d\n", inode_to_sector(dir_to_inode(curr)), token_count);
#endif
	
	if (token_count == 1)
		return curr;

  char temp[TOKEN_MAX];
  memcpy (temp, path, strlen(path)+1);

	int cnt = 0;
  char *token, *save_ptr;
  for (token = strtok_r (temp, DIR_DELIMIT_STR, &save_ptr); token != NULL;
 	 	token = strtok_r (NULL, DIR_DELIMIT_STR, &save_ptr)) {

#ifdef INFO7
	printf("dir_open_recursive: token %s, cnt %d\n", token, cnt);
#endif
		struct inode *inode=NULL;
		bool dir_find = dir_lookup(curr, token, &inode);
		dir_close(curr);

		if (!dir_find)
			return NULL;

		curr = dir_open(inode);
		cnt++;
	
		if (cnt == token_count-1)
			break;
  }

#ifdef INFO7
	printf("dir_open_recursive in final: curr %d\n", inode_to_sector(dir_to_inode(curr)));
#endif

	return curr;
}


bool 
is_absolute(const char* path) {
	if (strlen(path) <= 0 )
		PANIC("empty path at is absolute_path\n");

	return path[0] == DIR_DELIMIT;
}


int
count_token(const char* str, const char* delimiter) {
  char temp[TOKEN_MAX];
  memcpy (temp, str, strlen(str)+1);

  int cnt=0;

  char *token, *save_ptr;
  for (token = strtok_r (temp, delimiter, &save_ptr); token != NULL;
 	 	token = strtok_r (NULL, delimiter, &save_ptr)) {
	  cnt++;
  }
    
  return cnt;
}


char*
get_name_from_end(const char* path) {
  char temp[TOKEN_MAX];
  memcpy (temp, path, strlen(path)+1);

  char *token, *save_ptr, *prev;
  for (token = strtok_r (temp, DIR_DELIMIT_STR, &save_ptr); token != NULL;
 	 	token = strtok_r (NULL, DIR_DELIMIT_STR, &save_ptr)) {
		prev = token;
  }
   
	char* ret = malloc (strlen(prev)+1);
	memcpy(ret, prev, strlen(prev)+1);
 
  return ret;
}

struct inode*
dir_to_inode(struct dir* dir){
	return dir->inode;
}
