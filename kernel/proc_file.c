/*
 * Interface functions between file system and kernel/processes. added @lab4_1
 */

#include "proc_file.h"

#include "hostfs.h"
#include "pmm.h"
#include "process.h"
#include "ramdev.h"
#include "rfs.h"
#include "riscv.h"
#include "spike_interface/spike_file.h"
#include "spike_interface/spike_utils.h"
#include "util/functions.h"
#include "util/string.h"

//
// initialize file system
//
void fs_init(void) {
  // initialize the vfs
  vfs_init();

  // register hostfs and mount it as the root
  if( register_hostfs() < 0 ) panic( "fs_init: cannot register hostfs.\n" );
  struct device *hostdev = init_host_device("HOSTDEV");
  vfs_mount("HOSTDEV", MOUNT_AS_ROOT);

  // register and mount rfs
  if( register_rfs() < 0 ) panic( "fs_init: cannot register rfs.\n" );
  struct device *ramdisk0 = init_rfs_device("RAMDISK0");
  rfs_format_dev(ramdisk0);
  vfs_mount("RAMDISK0", MOUNT_DEFAULT);
}

//
// initialize a proc_file_management data structure for a process.
// return the pointer to the page containing the data structure.
//
proc_file_management *init_proc_file_management(void) {
  proc_file_management *pfiles = (proc_file_management *)alloc_page();
  pfiles->cwd = vfs_root_dentry; // by default, cwd is the root
  pfiles->nfiles = 0;

  for (int fd = 0; fd < MAX_FILES; ++fd)
    pfiles->opened_files[fd].status = FD_NONE;

  sprint("FS: created a file management struct for a process.\n");
  return pfiles;
}

//
// reclaim the open-file management data structure of a process.
// note: this function is not used as PKE does not actually reclaim a process.
//
void reclaim_proc_file_management(proc_file_management *pfiles) {
  free_page(pfiles);
  return;
}

//
// get an opened file from proc->opened_file array.
// return: the pointer to the opened file structure.
//
struct file *get_opened_file(int fd) {
  struct file *pfile = NULL;

  // browse opened file list to locate the fd
  for (int i = 0; i < MAX_FILES; ++i) {
    pfile = &(current->pfiles->opened_files[i]);  // file entry
    if (i == fd) break;
  }
  if (pfile == NULL) panic("do_read: invalid fd!\n");
  return pfile;
}

//
// open a file named as "pathname" with the permission of "flags".
// return: -1 on failure; non-zero file-descriptor on success.
//
int do_open(char *pathname, int flags) {
  struct file *opened_file = NULL;
  //sprint("pathname:%s", pathname);
  if ((opened_file = vfs_open(pathname, flags)) == NULL) return -1;

  int fd = 0;
  if (current->pfiles->nfiles >= MAX_FILES) {
    panic("do_open: no file entry for current process!\n");
  }
  struct file *pfile;
  for (fd = 0; fd < MAX_FILES; ++fd) {
    pfile = &(current->pfiles->opened_files[fd]);
    if (pfile->status == FD_NONE) break;
  }

  // initialize this file structure
  memcpy(pfile, opened_file, sizeof(struct file));

  ++current->pfiles->nfiles;
  return fd;
}

//
// read content of a file ("fd") into "buf" for "count".
// return: actual length of data read from the file.
//
int do_read(int fd, char *buf, uint64 count) {
  struct file *pfile = get_opened_file(fd);

  if (pfile->readable == 0) panic("do_read: no readable file!\n");

  char buffer[count + 1];
  int len = vfs_read(pfile, buffer, count);
  buffer[count] = '\0';
  strcpy(buf, buffer);
  return len;
}

//
// write content ("buf") whose length is "count" to a file "fd".
// return: actual length of data written to the file.
//
int do_write(int fd, char *buf, uint64 count) {
  struct file *pfile = get_opened_file(fd);

  if (pfile->writable == 0) panic("do_write: cannot write file!\n");

  int len = vfs_write(pfile, buf, count);
  return len;
}

//
// reposition the file offset
//
int do_lseek(int fd, int offset, int whence) {
  struct file *pfile = get_opened_file(fd);
  return vfs_lseek(pfile, offset, whence);
}

//
// read the vinode information
//
int do_stat(int fd, struct istat *istat) {
  struct file *pfile = get_opened_file(fd);
  return vfs_stat(pfile, istat);
}

//
// read the inode information on the disk
//
int do_disk_stat(int fd, struct istat *istat) {
  struct file *pfile = get_opened_file(fd);
  return vfs_disk_stat(pfile, istat);
}

//
// close a file
//
int do_close(int fd) {
  struct file *pfile = get_opened_file(fd);
  return vfs_close(pfile);
}

//
// open a directory
// return: the fd of the directory file
//
int do_opendir(char *pathname) {
  struct file *opened_file = NULL;
  if ((opened_file = vfs_opendir(pathname)) == NULL) return -1;

  int fd = 0;
  struct file *pfile;
  for (fd = 0; fd < MAX_FILES; ++fd) {
    pfile = &(current->pfiles->opened_files[fd]);
    if (pfile->status == FD_NONE) break;
  }
  if (pfile->status != FD_NONE)  // no free entry
    panic("do_opendir: no file entry for current process!\n");

  // initialize this file structure
  memcpy(pfile, opened_file, sizeof(struct file));
  ++current->pfiles->nfiles;
  return fd;
}

//
// read a directory entry
//
int do_readdir(int fd, struct dir *dir) {
  struct file *pfile = get_opened_file(fd);
  return vfs_readdir(pfile, dir);
}

//
// make a new directory
//
int do_mkdir(char *pathname) {
  return vfs_mkdir(pathname);
}

//
// close a directory
//
int do_closedir(int fd) {
  struct file *pfile = get_opened_file(fd);
  return vfs_closedir(pfile);
}

//
// create hard link to a file
//
int do_link(char *oldpath, char *newpath) {
  return vfs_link(oldpath, newpath);
}

//
// remove a hard link to a file
//
int do_unlink(char *path) {
  return vfs_unlink(path);
}


//
// from path get direct path and store it in char* ult
// added @lab4_challenge1
void get_ult_path(const char* path, char* ult){
  // path = "./ramfile"
  int i = 0; // multiple ..  support
  //char sub_str[MAX_PATH_LEN] = {0};
  while(1){ // can add multiple ../../, but this exp doesn't need it
    //sprint("substr: %s", sub_str);
    if(path[0] == '.' && path[1] == '/'){ // ./ format
      strcpy(ult, current->pfiles->cwd->name); // get current working dir
      //sprint("curpath: %s", current->pfiles->cwd->name);
      int cur_index = strlen(ult);
      int i = 1;
      if(strcmp(current->pfiles->cwd->name, "/") == 0){ // dir is /, doesn't need '/' added
        i = 2;
      }
      for(i;path[i] != 0;++i){
        ult[cur_index++] = path[i];
      }
      //sprint("ultpath2: %s", ult);
    }
    else if(path[0] == '.' && path[1] == '.') { // ../ format
      int reverse_layer = 0; // record the number of ".."
      int len_path = strlen(path);
      for(int i = 0;i < len_path;){
        if(path[i] == '.' && path[i + 1] == '.'){
          reverse_layer++;
          i += 3;
        }
        else break;
      }
      //sprint("cur_path: %s", current->pfiles->cwd->name);
      char* cur_path = current->pfiles->cwd->name; // direct path
      int len = strlen(cur_path);
      int index = 0, count = 0;
      for(int i = len - 1;i >= 0;--i){ // find parent path by find '/'
        if(cur_path[i] == '/'){
          if(++count == reverse_layer) {
            index = i;
            break;
          }
        }
      }
      memcpy(ult, cur_path, index + 1);
      if(strcmp(ult, "/") != 0){
        ult[strlen(ult) - 1] = 0;
      }
      //sprint("ult: %s", ult);
    }
    break;
  }
}

//
// find cuurent's path name, store it in para path
// 
int do_pwd(char* path){
  // get dir from cwd
  memcpy(path, current->pfiles->cwd->name, MAX_PATH_LEN);
  return 0;
}

//
// cd (relative ?)
// 
int do_cd(char* path){
  int len = strlen(path);
  //sprint("path's length: %d", len);
 
  if(path[0] == '.'){ // using relative path 
    char ult_path[MAX_PATH_LEN] = {0};
    get_ult_path(path, ult_path);
    memcpy(current->pfiles->cwd, ult_path, MAX_PATH_LEN);
  }
  else{ // use direct path
    memcpy(current->pfiles->cwd, path, MAX_PATH_LEN);
  }
  return 0;
}
