/* filesys.c
 * 
 * provides interface to virtual disk
 * 
 */
#include "filesys.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>


diskblock_t  virtualDisk [MAXBLOCKS];           // define our in-memory virtual, with MAXBLOCKS blocks
fatentry_t   FAT         [MAXBLOCKS];           // define a file allocation table with MAXBLOCKS 16-bit entries
fatentry_t   rootDirIndex            = 0;       // rootDir will be set by format
direntry_t *currentDir              = NULL;
fatentry_t   currentDirIndex         = 0;


/* --------  DISK FUNCTIONS ---------------

  Interface functions for the virtual disk.
  ------------------------------------
*/

void writedisk ( const char * filename )
{
   FILE * dest = fopen( filename, "w" );
   if ( fwrite ( virtualDisk, sizeof(virtualDisk), 1, dest ) < 0 )
   {
      fprintf ( stderr, "write virtual disk to disk failed\n" );
   }
   //write(dest, virtualDisk, sizeof(virtualDisk) );
   fclose(dest);
   
}

void readdisk ( const char * filename )
{
   FILE * dest = fopen( filename, "r" );
   if ( fread ( virtualDisk, sizeof(virtualDisk), 1, dest ) < 0 )
   {
      fprintf ( stderr, "write virtual disk to disk failed\n" );
   }
   //write( dest, virtualDisk, sizeof(virtualDisk) );
   fclose(dest);
}


/* --------  BLOCK FUNCTIONS ---------------

  Functions for handling blocks.
  ------------------------------------
*/

// Write a diskblock to the virtual disk.
void writeblock ( diskblock_t *block, int block_address, int type )
{
   if(type == TYPE_DATA)
   {
      memmove ( virtualDisk[block_address].data, block->data, BLOCKSIZE );
   }
   else if(type == TYPE_FAT)
   {
      memmove ( virtualDisk[block_address].fat, block->fat, BLOCKSIZE ); 
   }
   else if(type == TYPE_DIR)
   {
      memmove ( virtualDisk[block_address].data, block->data, BLOCKSIZE );
   }
}

// Copy data from virtual disk into a diskblock.
void readblock(diskblock_t *block, int block_address, int type)
{
   if(type == TYPE_DATA) memmove(block->data, virtualDisk[block_address].data, BLOCKSIZE);
   else if(type == TYPE_FAT) memmove(block->fat, virtualDisk[block_address].fat, BLOCKSIZE);
   else if(type == TYPE_DIR) memmove(block->data, virtualDisk[block_address].data, BLOCKSIZE);
}

// Empties and initialises a block for neatness. (No junk memory data).
void init_block(diskblock_t *block, int type)
{
  if(type == TYPE_DATA) for(int i=0; i<BLOCKSIZE; i++) block->data[i] = '\0';

  if(type == TYPE_DIR) {
   direntry_t *entry = malloc(sizeof(direntry_t));
   for(int i=0; i<BLOCKSIZE; i++) block->data[i] = '\0';
   block->dir.isDir = TRUE;
   block->dir.nextEntry = 0;
   for(int i=0; i<DIRENTRYCOUNT; i++) {
     entry->isdir = FALSE;
     entry->unused = TRUE;
     //strcpy(entry->name, "[empty]");
     block->dir.entrylist[i] = *entry;
   }
   //free(entry);
  }
}

// Print contents of block at given index (and of given type) -> depending on type, it prints them differently.
void printBlock ( int blockIndex, int type )
{
   if(type == TYPE_DATA) {
    printf("virtualDisk[%d] = \n", blockIndex);
    for(int i=0; i< BLOCKSIZE; i++) {
      printf("%c", virtualDisk[blockIndex].data[i]);
    }
    printf("\n");
   }
   else if(type == TYPE_FAT)
   {
      printf("virtualdisk[%d] = ", blockIndex);
      for(int i=0; i<FATENTRYCOUNT; i++) printf("%d", virtualDisk[blockIndex].fat[i]);
     printf("\n");
   }
   else if(type == TYPE_DIR) {
      printf("virtualdisk[%d] = directory block (isDir: %d, nextEntry: %d) => [", blockIndex, virtualDisk[blockIndex].dir.isDir, virtualDisk[blockIndex].dir.nextEntry);
      for(int i=0; i < DIRENTRYCOUNT; i++) {
        printf(" %s ", virtualDisk[blockIndex].dir.entrylist[i].name);
      }
      printf("]\n");
   }
}

// Main function for formatting the disk initially. 
// Initialises FAT, names the drive at position 0, and initialises root directory, then sets the rootDirIndex.
void format()
{
  int fatblocksneeded =  (MAXBLOCKS / FATENTRYCOUNT);
  int root_dir_index = fatblocksneeded + 1;

  // Give the drive a name, and store this at block 0 (reserved).
  diskblock_t block;
  init_block(&block, TYPE_DATA);
  memcpy(block.data, "Dylans_Drive", sizeof("Dylans_Drive"));
  writeblock(&block, 0, 0);

	// prepare FAT table.
	// write FAT blocks to virtual disk.
  for(int i=0; i<MAXBLOCKS; i++) FAT[i] = UNUSED;
  FAT[0] = ENDOFCHAIN;
  FAT[1] = 2;
  FAT[2] = ENDOFCHAIN;
  FAT[3] = ENDOFCHAIN; // The root directory.
  copyFAT(FAT);

	// prepare root directory.
	// write root directory block to virtual disk.
  diskblock_t rootblock;
  init_block(&rootblock, TYPE_DIR);

  rootblock.dir.isDir = TRUE;
  rootblock.dir.nextEntry = 0;
  rootDirIndex = root_dir_index; // Set the rootDirIndex global.

  // Initialise the root's entrylist.
  for(int i=0; i<3; i++) {
    direntry_t entry;
    entry.isdir = FALSE;
    entry.unused = TRUE;
    //strcpy(entry.name, "[empty]");
    rootblock.dir.entrylist[i] = entry;
  }
  writeblock(&rootblock, root_dir_index, TYPE_DIR); // account for space taken by FAT.

  // Update current directory.
  currentDirIndex = rootDirIndex;
}


/* --------  FILE FUNCTIONS ---------------

  Functions for handling files.
  ------------------------------------
*/

// Opens and creates files given a path and mode.
// Returns a 'MyFILE' file descriptor pointer.
MyFILE * myfopen(const char *path, const char *mode)
{
  if(strlen(path) > MAXPATHLENGTH) {
    printf("(myfopen) Pathname was too large (must be shorter than %d). Aborting.\n", MAXPATHLENGTH);
    return NULL;
  }

  // Copy const path into temporary variables to avoid segfaults.
  char *temp_p = malloc(sizeof(char) * MAXPATHLENGTH);
  char *temp_x = malloc(sizeof(char) * MAXPATHLENGTH);
  strcpy(temp_x, ""); // avoid garbage memory content.
  strcpy(temp_p, path);

  // Break down the path into a set of directories.
  char *filename = malloc(sizeof(char) * MAXNAME);
  strcpy(filename, ""); // avoid garbage memory content.
  char *dir_name = malloc(sizeof(char) * MAXNAME);
  char **directories = parse_path(temp_p);
  fatentry_t prev_dir_index = currentDirIndex;

  // Split filename and path into separate strings.
  int i = 0;
  int num_dirs = get_path_dir_no(directories);
  if(num_dirs == 1) {
    strcpy(temp_x, "/");
    strcpy(filename, directories[0]);
  }
  else {
    while(1) {
      if(strcmp(directories[i], "") == 0) break;
      strcpy(filename, directories[i]);
      if(i < num_dirs) {
        strcat(temp_x, "/");
        strcat(temp_x, directories[i]);
      }
      i++;
    }
  }

  // Call mymkdir to deal with the path (create any unexisting directories etc.).
  if(strcmp(temp_x, "/") == 0) {
    // if path is just "/", no need to do anything.
  }
  else {
    mymkdir(temp_x); // Otherwise call mymkdir to make sure the directories are created.
  
    // Get the last directory from the path and CD into it.
    i = 0;
    while(1) {
      if(strcmp(directories[i], "") == 0) break;
      strcpy(filename, directories[i]);
      if(i>0) strcpy(dir_name, directories[i-1]);
      i++;
    }
    change_dir(dir_index(dir_name)); // CD into the last directory before creating/opening the file.
  }

  // READ MODE.
  if(*mode == 'r') { // Open a file for reading. The file must exist.
    MyFILE *file = malloc(sizeof(MyFILE));

    // Find existing file.
    if(file_index(filename) >= 0) // file_index searches the current directory and returns -1 if file not found.
    {
      diskblock_t block = virtualDisk[file_index(filename)];
      file->pos = 0;
      memcpy(file->mode, "r", sizeof("r"));
      file->writing = 0;
      file->blockno = file_index(filename);
      file->buffer = block;

      //free(directories);
      //free(dir_name);
      //free(temp_p);
      change_dir(prev_dir_index);
      return file;
    }
    else // File doesn't exist, and in readmode, so do nothing.
    {
      //free(directories);
      //free(dir_name);
      //free(temp_p);
      change_dir(prev_dir_index);
      return NULL;
    }
  } 

  // WRITE MODE.
  else if(*mode == 'w') { // Create an empty file for writing. If a file with the same name already exists its content is erased and the file is considered as a new empty file.
    MyFILE *file = malloc(sizeof(MyFILE));

    // Find existing file.
    if(file_index(filename) >= 0)
    {
      diskblock_t block = virtualDisk[file_index(filename)];
      file->pos = 0;
      memcpy(file->mode, "w", sizeof("w"));
      file->writing = 1;
      file->blockno = file_index(filename);
      file->buffer = block;

      //free(directories);
      //free(dir_name);
      //free(temp_p);
      change_dir(prev_dir_index);
      return file;
    }
    else
    {
      // New file.
      diskblock_t block;
      init_block(&block, TYPE_DATA);
    
      // Initialise the file.
      MyFILE *file = malloc(sizeof(MyFILE));
      file->pos = 0;
      memcpy(file->mode, "w", sizeof("w"));
      file->writing = 1;
      file->blockno = next_free_fat();
      file->first_block = file->blockno;
      file->buffer = block;
      writeblock(&file->buffer, file->blockno, 0);

      // Update blockchain on FAT.
      FAT[file->blockno] = ENDOFCHAIN;
      copyFAT(FAT);

      // Update directory.
      direntry_t *newEntry = malloc(sizeof(direntry_t));
      newEntry->entrylength = MAXNAME;
      newEntry->isdir = FALSE;
      newEntry->unused = FALSE;
      newEntry->filelength = file_block_length(filename);
      newEntry->firstblock = file->first_block;
      strcpy(newEntry->name, filename);
      add_file(currentDirIndex, newEntry, TYPE_DATA);

      //free(directories);
      //free(dir_name);
      //free(temp_p);
      change_dir(prev_dir_index);
      return file;
    }
  }

  // APPEND MODE.
  else if(*mode == 'a') { // Append to a file. Writing operations append data at the end of the file. The file is created if it does not exist.
    MyFILE *file = malloc(sizeof(MyFILE));

    // Find existing file.
    if(file_index(filename) >= 0)
    {
      diskblock_t block = virtualDisk[file_index(filename)];
      file->blockno = file_index(filename);
      file->pos = 0;
      memcpy(file->mode, "a", sizeof("a"));
      file->writing = 1;

      // Get last block of file, and set pos to end of that block.
      // This is to start appending from the end of the file.
      while(1) {
        if(FAT[file->blockno] == ENDOFCHAIN) break;
        file->blockno = FAT[file->blockno];
      }
      block = virtualDisk[file->blockno];
      file->buffer = block;
      for(int i=0; i<BLOCKSIZE; i++){
        if(block.data[file->pos] == '\0') break;
        else file->pos++;
      }

      //free(directories);
      //free(dir_name);
      //free(temp_p);
      change_dir(prev_dir_index);
      return file;
    }
    else
    {

      // New file.
      diskblock_t block;
      init_block(&block, TYPE_DATA);
    
      // Initialise the file.
      MyFILE *file = malloc(sizeof(MyFILE));
      file->pos = 0;
      memcpy(file->mode, "a", sizeof("a"));
      file->writing = TRUE;
      file->blockno = next_free_fat();
      file->first_block = file->blockno;
      file->buffer = block;
      writeblock(&file->buffer, file->blockno, 0);

      // Update blockchain on FAT.
      FAT[file->blockno] = ENDOFCHAIN;
      copyFAT(FAT);

      // Update directory.
      direntry_t *newEntry = malloc(sizeof(direntry_t));
      newEntry->entrylength = MAXNAME;
      newEntry->isdir = FALSE;
      newEntry->unused = FALSE;
      newEntry->filelength = file_block_length(filename);
      newEntry->firstblock = file->first_block;
      strcpy(newEntry->name, filename);
      add_file(currentDirIndex, newEntry, TYPE_DATA);

      //free(directories);
      //free(dir_name);
      //free(temp_p);
      change_dir(prev_dir_index);
      return file;
    }
  }

  else { // Mode didn't match "a", "w", or "r".
      
      //free(directories);
      //free(dir_name);
      //free(temp_p);
    change_dir(prev_dir_index);
    return NULL;
  }
}

// Reads character from file at it's current pos pointer.
char myfgetc(MyFILE *file)
{
  char c;

  if(file->pos >= BLOCKSIZE) { // If the position reaches end of block, get the next one.
    if(FAT[file->blockno] == ENDOFCHAIN) { // Reached end of file.
      return EOF;
    }
    else { // Load the next block in the FAT chain into the buffer.
    file->buffer = virtualDisk[FAT[file->blockno]];
    file->blockno = FAT[file->blockno];
    file->pos = 0;
    c = file->buffer.data[file->pos++];
    }
  }
  else { // pos is still inside the buffer, so keep reading.
    file->buffer = virtualDisk[file->blockno];
    c = file->buffer.data[file->pos++];
  }

  return c;
}

// Writes character to file at it's current pos pointer.
int myfputc(MyFILE *file, const char ch)
{
  if(strcmp(file->mode, "r") == 0) {
    printf("(myfputc) write rejected: file was in read mode.\n");
    return 1;
  }
  if(file->pos >= BLOCKSIZE) { // If the pos has reached end of buffer.
    if(FAT[file->blockno] == ENDOFCHAIN) { // If this is the end of chain, create new block and extend the chain.
      //printf("Allocating new block\n");

      diskblock_t block;
      init_block(&block, TYPE_DATA);

      file->pos = 0;
      int next = next_free_fat();
      FAT[file->blockno] = next;
      file->blockno = next;
      file->buffer = block;
      file->buffer.data[file->pos] = ch;
      file->pos++;

      writeblock(&file->buffer, file->blockno, TYPE_DATA);
      FAT[file->blockno] = ENDOFCHAIN;
      copyFAT(FAT);
    }
    else { // There is still another block in the chain, so move to that one.
      file->buffer = virtualDisk[FAT[file->blockno]];
      file->pos = 0;
      file->blockno = FAT[file->blockno];
    }
  }
  else { // pos is still within buffer, so keep writing.
    file->buffer.data[file->pos] = ch;
    file->pos++;
  }

  // Write block to update the file.
  // Update FAT if it's a new block.
  if(FAT[file->blockno] == UNUSED){
    init_block(&file->buffer, TYPE_DATA);
    FAT[file->blockno] = ENDOFCHAIN;
    copyFAT(FAT);
  }
  writeblock(&file->buffer, file->blockno, TYPE_DATA);
  return 0;
}

// Removes a file at the given path.
// Doesn't clear the block immediately, but sets direntry.unused = TRUE so the block can be re-used by the filesystem.
void myremove(const char *path)
{
  if(strlen(path) > MAXPATHLENGTH) {
    printf("(myremove) Pathname was too large (must be shorter than %d). Aborting.\n", MAXPATHLENGTH);
    return;
  }

  // Copy path to temp variables avoid segfaults.
  char *temp_p = malloc(sizeof(char) * MAXPATHLENGTH);
  char *temp_x = malloc(sizeof(char) * MAXPATHLENGTH);
  strcpy(temp_x, ""); // avoid garbage text from somewhere...
  strcpy(temp_p, path);

  // Break down the path into set of directories.
  char *filename = malloc(sizeof(char) * MAXNAME);
  strcpy(filename, ""); // avoid garbage text from somewhere...
  char *dir_name = malloc(sizeof(char) * MAXNAME);
  char **directories = parse_path(temp_p);
  fatentry_t prev_dir_index = currentDirIndex;

  // Cut filename, and call mymkdir to deal with the path (create any unexisting directories etc.).
  int i = 0;
  int num_dirs = get_path_dir_no(directories);
  if(num_dirs == 1) {
    strcpy(temp_x, "/"); // root folder.
    strcpy(filename, directories[0]); // the filename.
  }
  else {
    // Check each directory, and make sure it exists.
    while(1) {
      if(strcmp(directories[i], "") == 0) break;
      if(dir_index(directories[i]) == -1) {
        printf("(myremove) no such file or directory %s\n", path);
        return;
      }
      strcpy(filename, directories[i]);
      if(i < num_dirs) {
        strcat(temp_x, "/");
        strcat(temp_x, directories[i]);
      }
      i++;
    }
  }

  if(strcmp(temp_x, "/") == 0) { // delete a file inside the root directory.
    diskblock_t *block = malloc(sizeof(diskblock_t)); // delete_file needs a block to load the parent directory information.
    delete_file(block, filename); // delete_file sets that files entry to unused.
    printf("(myremove) deleted file %s in root.\n", filename);
  }
  else {
    mymkdir(temp_x);
  
    // Get the last directory from the path and CD to it.
    i = 0;
    while(1) {
      if(strcmp(directories[i], "") == 0) break;
      strcpy(filename, directories[i]);
      if(i>0) strcpy(dir_name, directories[i-1]);
      i++;
    }

    // Set the file's entry to unused.
    diskblock_t *block = malloc(sizeof(diskblock_t));
    readblock(block, dir_index(dir_name), TYPE_DIR);
    change_dir(dir_index(dir_name));
    delete_file(block, filename);
    printf("(myremove) deleted file %s in %s.\n", filename, dir_name);
    change_dir(prev_dir_index);
  }

}

// Close the file descriptor and free the pointer.
void myfclose(MyFILE *file)
{
  free(file);
}

// Given a dirblock, and a filename, sets that file's entry to be unused so the filesystem can reclaim the space.
void delete_file(diskblock_t *directory, const char *filename)
{
  for(int i=0; i<DIRENTRYCOUNT; i++) {
    if(strcmp(directory->dir.entrylist[i].name, filename) == 0) {
      directory->dir.entrylist[i].unused = TRUE;
      //strcpy(directory->dir.entrylist[i].name, "[empty]");
      FAT[directory->dir.entrylist[i].firstblock] = UNUSED;
      writeblock(directory, currentDirIndex, TYPE_DIR);
      return;
    }
  }
}

// Get index of the first block belonging to a file. (within the current directory).
int file_index(const char *filename)
{
  diskblock_t directory = virtualDisk[currentDirIndex];
  for(int i=0; i<DIRENTRYCOUNT; i++) {
    if(directory.dir.entrylist[i].unused == FALSE) {
      if(memcmp(directory.dir.entrylist[i].name, filename, strlen(filename) + 1) == 0) {
        return directory.dir.entrylist[i].firstblock;
      }
    }
  }
  return -1; // file not found.
}

// Print contents of all the blocks belonging to a file in order.
void print_file(const char *filename)
{
  int start = file_index(filename);
  int end = (start + (file_block_length(filename)));
  for(int i=start; i<end; i++) printBlock(i, TYPE_DATA);
}

// Return number of blocks allocated to a file.
int file_block_length(const char *filename) {
  int count = 1;
  int cur = file_index(filename);
  while((cur = FAT[cur]) != ENDOFCHAIN) count++;

  return count;
}


/* --------  FAT FUNCTIONS ---------------

  Functions for dealing with the FAT.
  ------------------------------------
*/

// Write the FAT to the virtual disk.
void copyFAT(fatentry_t *FAT)
{
   diskblock_t block;
   int y = 0;
   for(int i=0; i<(MAXBLOCKS /(BLOCKSIZE / sizeof(fatentry_t))); i++) // in this case == 2 -> we need 2 blocks to store FAT.
   {
      // new block.
      for(int x=0; x<(BLOCKSIZE / sizeof(fatentry_t)); x++) // in this case == 512 -> each block can store 512 FAT entries.
      {
         block.fat[x] = FAT[y++];
      }
      writeblock(&block, i+1, 1);
   }
}

// Return the next unused FAT position.
int next_free_fat()
{
  int i;
  for(i=0; i<MAXBLOCKS; i++) {
    if(FAT[i] == UNUSED) {
      FAT[i] = ENDOFCHAIN;
      return i;
    }
  }
  return -1;
}

// Print contents of FAT.
void print_FAT()
{
   printf("The FAT:\n");
   for(int i=0; i<1024; i++) printf("%d", FAT[i]);
   printf("\n");
}


/* --------  DIRECTORY FUNCTIONS ---------------

  Making, deleting, and filling directories.
  ------------------------------------------
*/

// Add file to directory at the given index.
void add_file(fatentry_t dir_index, direntry_t *entry, int type) {
  if(type == TYPE_DATA) {

    // Get info about the directory and copy file into it's entrylist.
    diskblock_t *temp_block = malloc(sizeof(diskblock_t));
    readblock(temp_block, dir_index, type);
    int next_free = next_free_dir_entry(&temp_block->dir);
    temp_block->dir.entrylist[next_free] = *entry;
    writeblock(temp_block, dir_index, type);

    //free(temp_block);
    return;
  }
  else if(type == TYPE_FAT) {
    return;
  }
  else if(type == TYPE_DIR) {
    return;
  }
  else {
    return;
  }
}

// Creates a directory at given path.
void mymkdir(char *path)
{
  if(strlen(path) > MAXPATHLENGTH) {
    printf("(mymkdir) Pathname was too large. Returning.\n");
    return;
  }

  // Break the path down into a list of directories.
  char **dir_list = parse_path(path);
  int index = 0;
  int original_dir_index = currentDirIndex;
  int num_dirs = 0;

  // Count how many directories are in the path.
  int i=0;
  while(1) {
    if(strcmp(dir_list[i], "") == 0) break;
    i++;
    num_dirs++;
  }

  printf("(mymkdir) adding %s \n", path);

  // Absolute path.
  if(path[0] == '/') {
    // Start at root.
    currentDirIndex = rootDirIndex;

    // Iterate through the path.
    for(int i=0; i<num_dirs; i++) {
      index = dir_index(dir_list[i]);

      // Check if directory exists (create if not).
      if(index == -1) {
        add_dir(dir_list[i]);
      }
      else {
        // it was found.
      }

      // Then change currentDirIndex to it and continue.
      currentDirIndex = dir_index(dir_list[i]);
    }
  }

  // Relative path.
  else {
    // do the same as above, but starting at currentDirIndex not root...

    // Iterate through the path.
    for(int i=0; i<num_dirs; i++) {
      index = dir_index(dir_list[i]);

      // Check if directory exists (create if not).
      if(index == -1) {
        add_dir(dir_list[i]);
      }
      else {
        // it was found.
      }

      // Then change currentDirIndex to it and continue.
      currentDirIndex = dir_index(dir_list[i]);
    }
  }

  // Set currentDirIndex back to original value.
  currentDirIndex = original_dir_index;
  //free(dir_list);
  //free(parent);
  return;
}

// Scans a pathname and tokenizes into a list of directories.
char ** parse_path(char *path)
{
  char **directories = alloc_2d_char_array(200, MAXNAME);

  int i = 0;
  char str[MAXPATHLENGTH];
  strcpy(str, path); // this prevents a segfault.
  char *token = strtok(str, "/");

  while(token != NULL) {
    strcpy(directories[i++], token);
    token = strtok(NULL, "/");
  }

  return directories;
}

// Lists the contents of the directory at given path.
char ** mylistdir(char *path)
{
  int original_dir_index = currentDirIndex;
  char **file_list = alloc_2d_char_array(MAXDIRCONTENTS, MAXNAME);
  diskblock_t temp;
  
  if(strcmp(path, "/") == 0) {
    readblock(&temp, rootDirIndex, TYPE_DIR);
    for(int i=0; i<DIRENTRYCOUNT; i++) {
      file_list[i] = temp.dir.entrylist[i].name; 
    }
  }
  else {
    char **directories = parse_path(path);
    // loop through each entry, find the index, load that block, and load it's entrylist.
    char *name = malloc(sizeof(char) * MAXNAME);
    int i = 0;
    
    // Get the last dir.
    while(1) {
      if(strcmp(directories[i], "") == 0) break;
      strcpy(name, directories[i++]);
    }

    // If the directory doesn't exist.
    if(dir_index(name) == -1) {
    }

    // If it does exist, retrieve it.
    else {
      readblock(&temp, dir_index(name), TYPE_DIR);
      for(int i=0; i<DIRENTRYCOUNT; i++) {
        strcpy(file_list[i], temp.dir.entrylist[i].name);
      }
    }
    //free(name);
    //free(directories);
  }

  // Set currentDirIndex back to original.
  currentDirIndex = original_dir_index;
  return file_list;
}

// Add a new directory to the current dir.
void add_dir(const char *folder_name) {

  // Get parent dir.
  diskblock_t *parent = malloc(sizeof(diskblock_t));
  readblock(parent, currentDirIndex, TYPE_DIR);

  // Check for free entrylist slot.
  int free_entry_index = next_free_dir_entry(&parent->dir);
  int next_index = next_free_fat();

  // Create the new directory block.
  diskblock_t *newDir = malloc(sizeof(diskblock_t));
  init_block(newDir, TYPE_DIR);
  
  // Create the new dir's entry, for adding to parent.
  //direntry_t *newEntry = malloc(sizeof(direntry_t));
  direntry_t *newEntry = malloc(sizeof(direntry_t));
  newEntry->isdir = TRUE;
  newEntry->unused = FALSE;
  newEntry->firstblock = next_index;
  strcpy(newEntry->name, folder_name);

  // Unless root, add parent info to the new dir's entrylist.
  direntry_t *parentEntry = malloc(sizeof(direntry_t));
  parentEntry->isdir = TRUE;
  parentEntry->unused = FALSE;
  parentEntry->firstblock = currentDirIndex;
  strcpy(parentEntry->name, "..");
  newDir->dir.entrylist[0] = *parentEntry;

  // Add the entry to the parent and write to disk.
  parent->dir.entrylist[free_entry_index] = *newEntry;
  writeblock(parent, currentDirIndex, TYPE_DIR);
  writeblock(newDir, next_index, TYPE_DIR);
}

// Returns index of the first block belonging to a directory (-1 if not found).
int dir_index(const char *dirname)
{
  int found = FALSE;
  int index = -1;
  diskblock_t *temp = malloc(sizeof(diskblock_t));

  // Search the virtual disk for the folder.
  // Load in each block as a directory, and search it's entrylist.
  for(int y=0; y<MAXBLOCKS; y++) {
    readblock(temp, y, TYPE_DIR);
    if(temp->dir.isDir == TRUE) {
      for(int i=0; i<DIRENTRYCOUNT; i++) {
        if(temp->dir.entrylist[i].unused == FALSE) {
          if(strcmp(temp->dir.entrylist[i].name, dirname) == 0) { // found it.
            found = TRUE; // temp now points to the first dir in the path.
            index = temp->dir.entrylist[i].firstblock;
          }
        }
      }
    }
    if(found == TRUE) break;
  }

  //free(name);
  //free(temp);
  return index;
}

// Returns the name of the directory at given index.
char *get_dir_name(int dir_index)
{
  diskblock_t *temp = malloc(sizeof(diskblock_t));
  char *ch = "None";
  for(int i=0; i<MAXBLOCKS; i++) {
    readblock(temp, i, TYPE_DIR);
    if(temp->dir.isDir == TRUE) {
      for(int y=0; y<DIRENTRYCOUNT; y++) {
        if(temp->dir.entrylist[y].firstblock == dir_index) {
          return temp->dir.entrylist[y].name;
        }
      }
    }
  }
  return ch;
}

// Returns the number of directories in a list of directories.
int get_path_dir_no(char **dirs)
{
  int i = 0;
  while(1) {
    if(strcmp(dirs[i], "") == 0) break;
    i++;
  }
  return i;
}

// Returns the next free entry in a directory's entrylist.
// If there are no free entries, then it allocates another dirblock_t to the directory
// and updates the FAT accordingly.
int next_free_dir_entry(dirblock_t *folder) {
  int free_index = -1;
  for(int i=0; i<DIRENTRYCOUNT; i++) {
    if(folder->entrylist[i].unused == TRUE) {
      free_index = i;
      break;
    }
  }

  if(free_index == -1) {
    // run out of space, so allocate new dirblock to the directory.
    free_index = next_free_fat();
    diskblock_t new_block;
    init_block(&new_block, TYPE_DIR);
    writeblock(&new_block, free_index, TYPE_DIR);
    FAT[currentDirIndex] = free_index;
    //free_index = 0; // pos 0 of the new dirblock's entrylist.
  }

  return free_index;
}

// Prints the entrylist of a given directory.
void print_dir_contents(fatentry_t dir_index) {
  diskblock_t temp;
  readblock(&temp, dir_index, TYPE_DIR);
  printf("Current directory contents:\n");
  for(int i=0; i<DIRENTRYCOUNT; i++) {
    printf("%s\n", temp.dir.entrylist[i].name);
  }
}

// Prints the entrylist of the current directory.
void ls_current_dir() {
  diskblock_t temp;
  readblock(&temp, currentDirIndex, TYPE_DIR);
  printf("Current directory contents:\n");
  for(int i=0; i<DIRENTRYCOUNT; i++) {
    printf("%s\n", temp.dir.entrylist[i].name);
  }
}

// Sets currentDirIndex to given index.
void change_dir(int dir_index)
{
  currentDirIndex = dir_index;
}

// Changes directory to the given path.
void mychdir(const char *path)
{
  if(strlen(path) > MAXPATHLENGTH) {
    printf("(mychdir) Pathname was too large (must be shorter than %d). Aborting.\n", MAXPATHLENGTH);
    return;
  }

  // Break down the path into a list of directory names.
  char *dir_name = malloc(sizeof(char) * MAXNAME);
  char *temp_p = malloc(sizeof(char) * MAXNAME); // to preserve const path from parse_path(), which uses strtok().
  strcpy(temp_p, path);
  char **directories = parse_path(temp_p);

  // Check if root.
  if(strcmp(path, "/") == 0) {
    printf("(mychdir) changed directory to root\n");
    change_dir(rootDirIndex);
  }
  else {
    // Detect if any of the directories in the path are missing.
    int i = 0;
    while(1) {
      if(strcmp(directories[i], "") == 0) break;
      strcpy(dir_name, directories[i]);
      if(dir_index(dir_name) == -1) {
        printf("(mychdir) no such directory %s\n", path);
        return;
      }
      i++;
    }

    // Get the last directory from the path.
    i = 0;
    while(1) {
      if(strcmp(directories[i], "") == 0) break;
      strcpy(dir_name, directories[i]);
      i++;
    }
    printf("(mychdir) changed directory to %s\n", dir_name);
    change_dir(dir_index(dir_name));
    //printf("currentDirIndex is now at %d (%s)\n", currentDirIndex, dir_name);
  }
}

// Delete the directory with given name, by setting it's entry to unused.
void delete_dir(const char *dirname)
{
  diskblock_t *temp_b = malloc(sizeof(diskblock_t));
  readblock(temp_b, currentDirIndex, TYPE_DIR);
  for(int i=0; i<DIRENTRYCOUNT; i++) {
    if(temp_b->dir.entrylist[i].unused == FALSE) {
      if(temp_b->dir.entrylist[i].isdir == TRUE && (strcmp(temp_b->dir.entrylist[i].name, dirname) == 0)) {
        temp_b->dir.entrylist[i].unused = TRUE;
        //strcpy(temp_b->dir.entrylist[i].name, "[empty]");
        FAT[temp_b->dir.entrylist[i].firstblock] = UNUSED;
        writeblock(temp_b, currentDirIndex, TYPE_DIR);
        return;
      }
    }
  }
}

// Removes a directory, if it is empty.
void myrmdir(const char *path)
{
  if(strlen(path) > MAXPATHLENGTH) {
    printf("(myrmdir) Pathname was too large (must be shorter than %d). Aborting.\n", MAXPATHLENGTH);
    return;
  }

  // Break down the path.
  char *dir_name = malloc(sizeof(char) * MAXNAME);
  char *temp_p = malloc(sizeof(char) * MAXNAME); // to preserve const path from parse_path(), which uses strtok().
  strcpy(temp_p, path);
  char **directories = parse_path(temp_p);
  fatentry_t prev_dir_index = currentDirIndex;

  // Check if root.
  if(strcmp(path, "/") == 0) {
    change_dir(rootDirIndex);
  }
  else {
    // Detect if any of the directories in the path are missing.
    int i = 0;
    while(1) {
      if(strcmp(directories[i], "") == 0) break;
      strcpy(dir_name, directories[i]);
      if(dir_index(dir_name) == -1) {
        printf("(myrmdir) no such directory %s\n", path);
        return;
      }
      i++;
    }

    // Get the last directory from the path.
    i = 0;
    while(1) {
      if(strcmp(directories[i], "") == 0) break;
      strcpy(dir_name, directories[i]);
      i++;
    }
    // Delete it by setting unused to true.
    printf("(myrmdir) deleted directory %s\n", path);
    diskblock_t *current = malloc(sizeof(diskblock_t));
    readblock(current, dir_index(dir_name), TYPE_DIR);
    change_dir(current->dir.entrylist[0].firstblock);
    delete_dir(dir_name);
    change_dir(prev_dir_index);
  }
}

/* --------  UTILITY FUNCTIONS ---------------

  Misc. utility functions for tidier code.
  ------------------------------------------
*/

// Allocates a 2-D array of strings, with given dimensions.
char **alloc_2d_char_array(int max_x, int max_y)
{
  char **file_list = malloc(max_x * max_y * sizeof(char));
  
  // malloc.
  for(int i=0; i<max_x; i++) {
    file_list[i] = malloc(sizeof(**file_list) * max_y);
  }

  // initialise.
  for(int i=0; i<max_x; i++) {
    for(int y=0; y<max_y; y++) {
      file_list[i][y] = '\0';
    }
  }
  return file_list;
}
