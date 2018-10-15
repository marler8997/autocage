#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>

#include <sys/stat.h>

#include "common.h"
#include "clean.h"
/*
err_t clean_tmp(const char *dir, DIR *dir_handle)
{
  err_t result = 0;
  size_t dir_length = strlen(dir);

  for (;;) {
    errno = 0;
    struct dirent *entry = readdir(dir_handle);
    if (entry == NULL) {
      if (errno) {
        result = 1;
        errnof("readdir '%s' failed", dir);
      }
      break;
    }
    if (0 == strncmp(entry->d_name, "rex.", 4)) {
      printf("%s\n", entry->d_name);
      size_t entry_name_length = dir_length + 1 + strlen(entry->d_name);
      char *entry_name = malloc(entry_name_length + 1);
      if (!entry_name) {
        errnof("malloc failed");
        continue;
      }
      memcpy(entry_name, dir, dir_length);
      entry_name[dir_length] = '/';
      strcpy(entry_name + dir_length + 1, entry->d_name);
      result |= loggy_rmtree(entry_name);
      free(entry_name);
    }
  }
  return result;
}
*/
int main(int argc, char *argv[])
{
  const char *dir = "/tmp/.rex";
  struct stat dir_stat;
  if (-1 == stat(dir, &dir_stat)) {
    if (errno == ENOENT) {
      return 0;
    }
    errnof("stat '%s' failed", dir);
    return 1;
  }
  return loggy_rmtree(dir);
  /*
  const char *dir = "/tmp";
  DIR *dir_handle = opendir(dir);
  if (dir_handle == NULL) {
    errnof("opendir '%s' failed", dir);
    return 1;
  }
  err_t result = clean_tmp(dir, dir_handle);
  closedir(dir_handle);
  return result;
  */
}
