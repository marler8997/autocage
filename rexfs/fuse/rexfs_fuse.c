#include <stdio.h>

#include <fuse.h>

#define errorf(fmt,...) fprintf(stderr, "Error: " fmt "\n", ##__VA_ARGS__)

int rexfs_getattr(const char *path, struct stat *statbuf)
{
  errorf("getattr not implemented");
  return 1; // error
}
int rexfs_readlink(const char *path, char *link, size_t size)
{
  errorf("readlink not implemented");
  return 1; // error
}
/*
int rexfs_open(const char *path, struct fuse_file_info *fi)
{
  errorf("open not implemented");
  return 1; // error
}
int rexfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
  errorf("read not implemented");
  return 1; // error
}
*/

struct fuse_operations ops = {
  .getattr = rexfs_getattr,
  .readlink = rexfs_readlink,
  //.open = rexfs_open
  //.read = rexfs_read
};

int main(int argc, char *argv[])
{
  return fuse_main(argc, argv, &ops, NULL);
}
