/* filesys.h
 * 
 * describes FAT structures
 * http://www.c-jump.com/CIS24/Slides/FAT/lecture.html#F01_0020_fat
 * http://www.tavi.co.uk/phobos/fat.html
 */

#ifndef FILESYS_H
#define FILESYS_H

#include <time.h>

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define MAXBLOCKS     1024
#define BLOCKSIZE     1024
#define FATENTRYCOUNT (BLOCKSIZE / sizeof(fatentry_t))
#define DIRENTRYCOUNT ((BLOCKSIZE - (2*sizeof(int)) ) / sizeof(direntry_t))
#define MAXNAME       256
#define MAXPATHLENGTH 1024
#define MAXDIRCONTENTS 50 // added by me - directories cannot hold more than 50 items...

#define UNUSED        -1
#define ENDOFCHAIN     0
#define EOF           -1

#define TYPE_DATA 0
#define TYPE_FAT  1
#define TYPE_DIR  2


typedef unsigned char Byte;

/* create a type fatentry_t, we set this currently to short (16-bit)
 */
typedef short fatentry_t;


// a FAT block is a list of 16-bit entries that form a chain of disk addresses

//const int   fatentrycount = (blocksize / sizeof(fatentry_t));

typedef fatentry_t fatblock_t [ FATENTRYCOUNT ];


/* create a type direntry_t
 */

typedef struct direntry {
  int         entrylength; // length of this entry. Can be used with a variable-length name member to determine this entry's size. (which would differ depending on the size of 'name').
  Byte        isdir; // This is actually redundant - dirblock_t will already tell you it's a directory.
  Byte        unused;
  time_t      modtime;
  int         filelength;
  fatentry_t  firstblock;
  char   name [MAXNAME];
} direntry_t;

// a directory block is an array of directory entries

//const int   direntrycount = (blocksize - (2*sizeof(int)) ) / sizeof(direntry_t);

typedef struct dirblock {
  int isDir;
  int nextEntry; // the next unused index in the entrylist.
  direntry_t entrylist [ DIRENTRYCOUNT ]; // the first two integer are marker and endpos
} dirblock_t;



// a data block holds the actual data of a filelength, it is an array of 8-bit (byte) elements

typedef Byte datablock_t [ BLOCKSIZE ];


// a diskblock can be either a directory block, a FAT block or actual data

typedef union block {
  datablock_t data;
  dirblock_t  dir ;
  fatblock_t  fat ;
} diskblock_t;

// finally, this is the disk: a list of diskblocks
// the disk is declared as extern, as it is shared in the program
// it has to be defined in the main program filelength

extern diskblock_t virtualDisk [ MAXBLOCKS ];


// when a file is opened on this disk, a file handle has to be
// created in the opening program

typedef struct filedescriptor {
  int         pos;           // byte within a block
  char        mode[3];
  Byte        writing;
  fatentry_t  blockno;
  fatentry_t  first_block;
  diskblock_t buffer;
} MyFILE;



void copyFAT(fatentry_t *FAT);
void format();
void writedisk ( const char *filename);
void printBlock(int blockIndex, int type);
void writeblock ( diskblock_t *block, int block_address, int type);
void readblock(diskblock_t *block, int block_address, int type);
MyFILE * myfopen(const char *filename, const char *mode);
void init_block(diskblock_t *block, int type);
int next_free_fat();
int file_index(const char *filename);
char myfgetc(MyFILE *file);
int myfputc(MyFILE *file, const char ch);
void myfclose(MyFILE *file);
int file_block_length(const char *filename);
void print_FAT();
void add_file(fatentry_t dir_index, direntry_t *entry, int type);
void add_dir(const char *folder_name);
void print_dir_contents(fatentry_t dir_index);
void ls_current_dir();
int next_free_dir_entry(dirblock_t *folder);
int dir_index(const char *filename);
void print_file(const char *filename);
void cd_dir(const char *dirname);
void mymkdir(char *path);
void change_dir(int dir_index);
void mychdir(const char *path);
void myremove(const char *path);
void delete_file(diskblock_t *directory, const char *filename);
char ** parse_path(char *path);
char ** mylistdir(char *path);
char **alloc_2d_char_array(int max_x, int max_y);
int get_path_dir_no(char **dirs);
void delete_dir(const char *dirname);
void myrmdir(const char *path);
char *get_dir_name(int dir_index);


#endif

/*
#define NUM_TYPES (sizeof types / sizeof types[0])
static* int types[] = { 
    1,
    2, 
    3, 
    4 };
*/
