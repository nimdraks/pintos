#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/malloc.h"

static struct semaphore fileSema;

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();

  if (format) 
    do_format ();

  free_map_open ();

	sema_init(&fileSema, 1);
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size) 
{
  block_sector_t inode_sector = 0;
  struct dir *dir;

	dir = dir_open_recursive(name);

	char* name_end = get_name_from_end(name);

#ifdef INFO7
	printf("filesys_create: dir %p\n", dir);
	printf("filesys_create: parent sector %d, name_end %s\n", inode_to_sector(dir_to_inode(dir)), name_end);
#endif

  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size)
                  && dir_add_file (dir, name_end, inode_sector));

  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  dir_close (dir);

	free(name_end);

  return success;
}

bool 
filesys_create_dir (const char* name) {
  block_sector_t inode_sector = 0;

	struct dir* parent = dir_open_recursive(name);
	char* name_end = get_name_from_end(name);

#ifdef INFO7
	printf("filesys_create_dir: parent sector %d, name_end %s\n", inode_to_sector(dir_to_inode(parent)), name_end);
#endif

	bool success = (parent != NULL
									&& free_map_allocate(1, &inode_sector)
									&& dir_create(inode_sector, 16, inode_to_sector(dir_to_inode(parent)))
                  && dir_add (parent, name_end, inode_sector, true));

  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
	dir_close(parent);
	free(name_end);

	return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
	sema_down(&fileSema);
  struct dir *dir;
  struct inode *inode = NULL;
	bool is_dir=false;

#ifdef INFO9
	printf("filesys_open: name %s\n", name);
#endif
	if (strcmp(name, "/") == 0){
		sema_up(&fileSema);
		return file_open(inode_open(ROOT_DIR_SECTOR), true);
	}

	dir = dir_open_recursive(name);

	char* name_end = get_name_from_end(name);

  if (dir != NULL){
    dir_lookup (dir, name_end, &inode);
		is_dir = dir_is_dir (dir, name_end);
	}
  dir_close (dir);

	free(name_end);

	sema_up(&fileSema);
  return file_open (inode, is_dir);
}

bool
filesys_change_dir(const char *name){

	struct dir* dir;
	struct inode* inode = NULL;
	bool success = false;

	if (strcmp(name, "/") == 0){
		thread_current()->cwd_sector=ROOT_DIR_SECTOR;
		thread_current()->cwd_is_removed=false;
		return true;
	}

	dir = dir_open_recursive(name);
	char* name_end = get_name_from_end(name);

  if (dir != NULL)
    dir_lookup (dir, name_end, &inode);
#ifdef INFO8
//	printf("change succeed at dir %d, target %d, name_end %s\n", inode_to_sector(dir_to_inode(dir)), inode_to_sector(inode), name_end);
#endif
  dir_close (dir);

	if (inode != NULL){
		thread_current()->cwd_sector=inode_to_sector(inode);
		thread_current()->cwd_is_removed=false;
		success=true;
	}

	free(name_end);	
	return success;
}


/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  struct dir *dir;

	dir = dir_open_recursive(name);

	char* name_end = get_name_from_end(name);

  bool success = dir != NULL && dir_remove (dir, name_end);

  dir_close (dir); 
	free(name_end);

  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create_root (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}
