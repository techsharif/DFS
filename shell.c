#include "filesys.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

/*
  Various functions for testing the filesystem.
*/

void cgs_d()
{
  // call format() to format the virtualdisk.
  format();

  // Name the disk.
  diskblock_t block;
  for(int i=0; i<BLOCKSIZE; i++) block.data[i] = '\0';
  memcpy(block.data, "Dylan Filesystem", sizeof("Dylan Filesystem"));
  writeblock(&block, 0, 0);

  // write the virtual disk to a file (call it "virtualdiskD3_D1").
  writedisk("virtualdiskD3_D1");
}

void cgs_c()
{
  // Create test file.
  MyFILE * fp = myfopen("testfile.txt", "w");

  // Write character to test file.
  fp = myfopen("testfile.txt", "a");
  char ch[4 * BLOCKSIZE];

  // Assign 4096 bytes to the file.
  //int cur = 0;
  for(int i=0; i<(4*BLOCKSIZE); i++){
    ch[i] = 'a';
    //ch[i] = "ABCDEFGHIJKLMNOPQRSTUVWXWZ"[cur++ % 26];
  }
  for(int i=0; i<(4*BLOCKSIZE); i++) {
    myfputc(fp, ch[i]);
  }
  printf("\n");

  // Read out file.
  fp = myfopen("testfile.txt", "r");
  char current;
  while((current = myfgetc(fp)) != EOF) {
    printf("%c", current);
  }
  printf("\n");

  // Write the file to a real file on the hard disk.
  fp = myfopen("testfile.txt", "r");
  
  if(fp) {
    fp->blockno++;
    FILE *file = fopen("testfileC3_C1_copy.txt", "w");
    char content;
    while(1) {
      content = myfgetc(fp);
      if(content == EOF) break;
      fprintf(file, "%c", content);
    }
    fclose(file);
  }

  // print the blocks belonging to this file, for clarity.
  //print_file("testfile.txt");

  // Close the file.
  myfclose(fp);

  // Write virtualdisk to real file.
  writedisk("virtualdiskC3_C1");
}

void cgs_b()
{
  // create a directory "/myfirstdir/myseconddir/mythirddir" in the virtual disk.
  char *pathname = malloc(sizeof(char) * strlen("/myfirstdir/myseconddir/mythirddir"));
  strcpy(pathname, "/myfirstdir/myseconddir/mythirddir"); // otherwise mymkdir segfaults.
  mymkdir(pathname);

  // call mylistdir("/myfirstdir/myseconddir"): print out the list of strings returned by this function.
  char **file_list = alloc_2d_char_array(MAXDIRCONTENTS, MAXNAME);
  printf("Contents of '/myfirstdir/myseconddir':\n");
  file_list = mylistdir("/myfirstdir/myseconddir"); // was "/"
  
  // print the results of mylistdir().
  for(int i=0; i<MAXDIRCONTENTS; i++) {
    if(strcmp(file_list[i], "") == 0) break;
    printf("\t> %s\n", file_list[i]);
  }
  free(file_list);

  // write out virtual disk to "virtualdiskB3_B1_a".
  writedisk("virtualdiskB3_B1_a");

  // create a file "/myfirstdir/myseconddir/testfile.txt" in the virtual disk.
  MyFILE *file = myfopen("/myfirstdir/myseconddir/testfile.txt", "w");
  myfclose(file);

  // call mylistdir("/myfirstdir/myseconddir"): print out the list of strings returned by this function.
  file_list = alloc_2d_char_array(MAXDIRCONTENTS, MAXNAME);
  printf("Contents of '/myfirstdir/myseconddir':\n");
  file_list = mylistdir("/myfirstdir/myseconddir"); // was "/"
  for(int i=0; i<MAXDIRCONTENTS; i++) {
    if(strcmp(file_list[i], "") == 0) break;
    printf("\t> %s\n", file_list[i]);
  }
  free(file_list);

  // write out virtual disk to "virtualdiskB3_B1_b".
  writedisk("virtualdiskB3_B1_b");
}

void cgs_a()
{
  // Format for clarity, so that we are using a fresh drive after the previous parts.
  format();

  // Create a directory "/firstdir/seconddir" in the virtual disk.
  mymkdir("/firstdir/seconddir");

  // call myfopen("firstdir/seconddir/testfile1.txt").
  MyFILE *file =  myfopen("firstdir/seconddir/testfile1.txt", "w");

  // you may write something into the file.
  char text[] = "First file for CGS A";
  for(int i=0; i<strlen(text); i++) {
    myfputc(file, text[i]);
  }

  // close the file.
  myfclose(file);

  // call mylistdir("/firstdir/seconddir"): print out the list of strings returned by this function.
  char **file_list = alloc_2d_char_array(MAXDIRCONTENTS, MAXNAME);
  file_list = mylistdir("/firstdir/seconddir");
  printf("Contents of '/firstdir/seconddir':\n");
  // print the results of mylistdir().
  for(int i=0; i<MAXDIRCONTENTS; i++) {
    if(strcmp(file_list[i], "") == 0) break;
    printf("\t> %s\n", file_list[i]);
  }
  free(file_list);

  // change to directory "/firstdir/seconddir".
  mychdir("/firstdir/seconddir");

  // call mylistdir("/firstdir/seconddir") or mylistdir(".") to list the current dir, 
  //print the list of strings returned by this function.
  file_list = mylistdir("/firstdir/seconddir");
  printf("Contents of '/firstdir/seconddir':\n");
  // print the results of mylistdir().
  for(int i=0; i<MAXDIRCONTENTS; i++) {
    if(strcmp(file_list[i], "") == 0) break;
    printf("\t> %s\n", file_list[i]);
  }
  free(file_list);

  // call myfopen("testfile2.txt", "w");
  file = myfopen("testfile2.txt", "w");

  // you may write something into the file.
  char text_2[] = "Second file for CGS A";
  for(int i=0; i<strlen(text_2); i++) {
    myfputc(file, text_2[i]);
  }

  // close the file.
  myfclose(file);

  // create directory "thirddir".
  mymkdir("thirddir");

  // call myfopen("thirddir/testfile3.txt", "w").
  file = myfopen("thirddir/testfile3.txt", "w");

  // you may write something into the file.
  char text_3[] = "Third file for CGS A";
  for(int i=0; i<strlen(text_3); i++) {
    myfputc(file, text_3[i]);
  }

  // close the file.
  myfclose(file);

  // write out virtual disk to "virtualdiskA5_A1_a".
  writedisk("virtualdiskA5_A1_a");

  // call myremove("testfile1.txt").
  myremove("testfile1.txt");

  // call myremove("testfile2.txt").
  myremove("testfile2.txt");

  // write out virtual disk to "virtualdiskA5_A1_b"
  writedisk("virtualdiskA5_A1_b");

  // call mychdir("thirddir").
  mychdir("thirddir");

  // call myremove("testfile3.txt").
  myremove("testfile3.txt");

  // write out virtual disk to "virtualdiskA5_A1_c".
  writedisk("virtualdiskA5_A1_c");

  // call mychdir("/firstdir/seconddir") or mychdir("..").
  mychdir("..");

  // call myrmdir("thirddir").
  myrmdir("thirddir");

  // call mychdir("/firstdir").
  mychdir("/firstdir");

  // call myrmdir("seconddir").
  myrmdir("seconddir");

  // call mychdir("/") or mychdir("..").
  mychdir("..");

  // call myrmdir("firstdir").
  myrmdir("firstdir");

  // write out virtual disk to "virtualdiskA5_A1_d".
  writedisk("virtualdiskA5_A1_d");
}


int main()
{
  // NOTE:  Add comments to choose which functions to run, if you want to run the tests individually.
  cgs_d();
  cgs_c();
  cgs_b();
  cgs_a();
  return 0;
}
