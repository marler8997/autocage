#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/mount.h>

#include <linux/limits.h>

#include "common.h"
#include "clean.h"

static const char *root; // the root directory we will chroot to
static size_t root_length;
static const char *user_cd_option = NULL;
static int forward_argc = 0;
static const char **forward_argv;

const char *get_opt_arg(int argc, const char *argv[], int *arg_index)
{
  (*arg_index)++;
  if (*arg_index >= argc) {
    errf("option '%s' requires an argument", argv[(*arg_index) - 1]);
    exit(1);
  }
  return argv[*arg_index];
}

unsigned get_dir_length(const char *file)
{
  const char * s = strrchr(file, '/');
  if (s == NULL) {
    return 0;
  }
  return s - file;
}

char *realpath2(const char *path)
{
  char temp[PATH_MAX];
  char *result = realpath(path, temp);
  return result ? strdup(result) : NULL;
}
char *malloc_getcwd()
{
  char temp[PATH_MAX];
  char *result = getcwd(temp, sizeof(temp));
  if (result == NULL) {
    errnof("getcwd failed");
    return NULL;
  }
  return strdup(result);
}

static int loggy_mkdir(const char *dir, mode_t mode)
{
  logf("mkdir -m %o %s", mode, dir);
  if (-1 == mkdir(dir, mode)) {
    errnof("mkdir '%s' failed", dir);
    return -1;
  }
  return 0;
}

static int loggy_mount(const char *source, const char *target,
                const char *filesystemtype, const char *options)
{
  logf("mount%s%s%s%s %s %s",
       filesystemtype ? " -t " : "",
       filesystemtype ? filesystemtype : "",
       options ? " -o " : "", options ? options : "",
       source, target);
  if (-1 == mount(source, target, filesystemtype, 0, options)) {
    errnof("mount failed");
    return -1; // fail
  }
  return 0; // success
}

static int loggy_bind_mount(const char *source, const char *target)
{
  logf("mount --bind %s %s", source, target);
  if (-1 == mount(source, target, NULL, MS_BIND, NULL)) {
    errnof("bind mount failed");
    return -1; // fail
  }
  return 0; // success
}

char *make_work_dir(const char *upper)
{
  int upperlen = strlen(upper);
  char *template = malloc(upperlen + 13); // 13: 6 for '.work.', 6 for XXXXXX, 1 for '\0'
  memcpy(template +        0 + 0, upper, upperlen);
  memcpy(template + upperlen + 0, ".work.", 6);
  memset(template + upperlen + 6, 'X', 6);
  template[upperlen + 12] = '\0';
  return mkdtemp(template);
}

// returns: 0 on success
err_t mkdirs_helper(char *dir, size_t length)
{
  if (dir[length] != '\0') {
    errf("code bug: mkdirs was called with a string that did not end in null");
    return 1;
  }
  //logf("[DEBUG] mkdirs '%s'", dir);
  {
    struct stat dir_stat;
    if (0 == stat(dir, &dir_stat)) {
      if (S_ISDIR(dir_stat.st_mode)) {
        return 0; // success
      }
      errf("'%s' exists but is not a directory", dir);
      return 1;
    }
  }
  {
    unsigned parent_dir_length = get_dir_length(dir);
    if (parent_dir_length == length) {
      errf("failed to create directory '%s'", dir);
      return 1;
    }
    dir[parent_dir_length] = '\0';
    int result = mkdirs_helper(dir, parent_dir_length);
    dir[parent_dir_length] = '/';
    if (result)
      return result;
  }
  if (-1 == loggy_mkdir(dir, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)) {
    // error already logged
    return current_error;
  }
  return 0; // success
}
err_t mkdirs(char *dir)
{
  return mkdirs_helper(dir, strlen(dir));
}

struct dir
{
  const char *arg;
  const char *source;
  const char *target_relative;
  char *private_target_absolute;
};

char *dir_get_absolute_target(struct dir *dir)
{
  if (!dir->private_target_absolute) {
    if (dir->target_relative == NULL) {
      size_t dir_length = strlen(dir->source);
      size_t total_length = root_length + dir_length;
      dir->private_target_absolute = malloc(root_length + dir_length + 1);
      if (dir->private_target_absolute == NULL) {
        errnof("malloc failed");
        return NULL;
      }
      memcpy(dir->private_target_absolute +           0, root, root_length);
      memcpy(dir->private_target_absolute + root_length, dir->source, dir_length);
      dir->private_target_absolute[total_length] = '\0';
    } else {
      errf("non-empty target not implemented");
      return NULL;
    }
  }
  return dir->private_target_absolute;
}

static unsigned char is_root_mount(struct dir *dir)
{
  return dir->target_relative != NULL && dir->target_relative[0] == '\0';
}

static err_t doit2(unsigned char have_upper, struct dir *dirs, int dir_count)
{
  unsigned non_root_mounts = 0;

  // make the mount point directories
  for (int i = 0; i < dir_count; i++) {
    struct dir *dir = &dirs[i];
    if (is_root_mount(dir))
      continue;

    non_root_mounts++;
    char *target_dir = dir_get_absolute_target(dir);
    if (target_dir == NULL) {
      // error already printed
      return 1;
    }
    {
      err_t result = mkdirs(target_dir);
      if (result)
        return result;
    }
  }

  // create the root mount overlay (do this before
  // mounting anything inside this directory)
  if (non_root_mounts < dir_count)
  {
    size_t lower_dirs_size = 0;
    for (int i = 0; i < dir_count; i++) {
      // TODO: handle upper dir properly
      struct dir *dir = &dirs[i];
      if (!is_root_mount(dir))
        continue;
      lower_dirs_size += 1 + strlen(dir->source);
    }

    // lowerdir=<lower_dirs>
    // TODO: support upperdir
    const int LOWERDIR_PREFIX_SIZE = 9;
    size_t options_size = LOWERDIR_PREFIX_SIZE + root_length + lower_dirs_size;
    char *options = malloc(options_size + 1);
    if (!options) {
      errnof("malloc failed");
      return 1;
    }
    // TODO: do not add the rootfs as a lowerdir if there are no non-root mounts
    size_t offset = 0;
    memcpy(options + offset, "lowerdir=", LOWERDIR_PREFIX_SIZE);
    offset += LOWERDIR_PREFIX_SIZE;
    memcpy(options + offset, root, root_length);
    offset += root_length;

    for (int i = 0; i < dir_count; i++) {
      // TODO: handle upper dir properly
      struct dir *dir = &dirs[i];
      if (!is_root_mount(dir))
        continue;
      options[offset++] = ':';
      size_t len = strlen(dir->source);
      memcpy(options + offset, dir->source, len);
      offset += len;
    }
    if (offset != options_size) {
      errf("code bug: options_size %lu != offset %lu", options_size, offset);
      return 1;
    }
    options[offset] = '\0';
    logf("options = '%s'", options);

    if (-1 == loggy_mount("none", root, "overlay", options)) {
      // error already logged
      return 1;
    }
    free(options);
  }

  // now mount the non-root mounts
  for (int i = 0; i < dir_count; i++) {
    struct dir *dir = &dirs[i];
    if (is_root_mount(dir))
      continue;

    char *target_dir = dir_get_absolute_target(dir);
    if (target_dir == NULL) {
      // error already printed
      return 1;
    }
    if (-1 == loggy_bind_mount(dir->source, target_dir)) {
      // error already printed
      return 1; // fail
    }

    // remount it as readonly
    // see https://lwn.net/Articles/281157/
    // it looks like if you want bind mounts to be readonly, you need to mount
    // them as writeable first, and then remount them as readonly
    // mount -o remount,ro <mount_point>
    /* NOT WORKING
       if (!have_upper || i > 0) {
       if (-1 == loggy_mount(NULL, target_dir, NULL, "remount,ro")) {
       // error already logged
       free(target_dir);
       return 1; // fail
        }
        }
    */
  }

  char *original_cwd = malloc_getcwd();
  if (original_cwd == NULL) {
    // error already printed
    return 1;
  }

  const char *cd_abs_postfix;
  if (user_cd_option == NULL) {
    cd_abs_postfix = original_cwd;
  } else {
    cd_abs_postfix = user_cd_option;
  }
  char *cd_full = malloc(root_length + strlen(cd_abs_postfix) + 1);
  if (cd_full == NULL) {
    errnof("malloc failed");
    return 1;
  }
  memcpy(cd_full, root, root_length);
  strcpy(cd_full + root_length, cd_abs_postfix);
  logf("cd '%s'", cd_full);
  if (-1 == chdir(cd_full)) {
    errnof("chdir '%s' failed", cd_full);
    return 1;
  }

  logf("chroot '%s'", root);
  if (-1 == chroot(root)) {
    errnof("chroot '%s' failed", root);
    return 1;
  }

  // at this point we CANNOT cleanup directories
  logf("execvp '%s'", forward_argv[0]);
  execvp(forward_argv[0], (char *const*)forward_argv);
  errnof("execvp returned");
  exit(1);
  // do not return
}

err_t doit(unsigned char have_upper, struct dir *dirs, int dir_count)
{
  // mount a new tmpfs? If I do, then I'll need to add the unmount
  // to the cleanup

  char *workdir = NULL;
  if (have_upper) {
    // create a work directory
    workdir = make_work_dir(dirs[0].source);
    if (!workdir) {
      errnof("failed to create work directory");
      return current_error;
    }
    logf("[DEBUG] workdir '%s'", workdir);
  }

  int result = doit2(have_upper, dirs, dir_count);
  if (workdir) {
    loggy_rmtree(workdir);
  }
  return result;
}

void usage()
{
  printf("Usage: rex [-options] <dirs>... -- <program> <args>...\n");
  printf("Options:\n");
  printf("  --cd|-c <dir>       The directory to change to (defaults to CWD)\n");
  // TODO: remove the "--upper" option
  //       intead, make a way to make a writeable directory
  //       i.e. rex -w .
  printf("  --upper|-u <dir>    The upper directory\n");
  // remap
  // <dir>:<target_dir>
  // so a sysroot
  // <dir>:
}


err_t main(int argc, const char *argv[])
{
  argc--;
  argv++;

  const char *upper = NULL;
  {
    int old_argc = argc;
    argc = 0;
    int arg_index = 0;
    for (; arg_index < old_argc; arg_index++) {
      const char *arg = argv[arg_index];
      if (arg[0] != '-') {
        argv[argc++] = arg;
      } else if (0 == strcmp(arg, "-c") || 0 == strcmp(arg, "--cd")) {
        user_cd_option = get_opt_arg(old_argc, argv, &arg_index);
      } else if (0 == strcmp(arg, "-u") || 0 == strcmp(arg, "--upper")) {
        upper = get_opt_arg(old_argc, argv, &arg_index);
      } else if (0 == strcmp(arg, "--")) {
        forward_argc = old_argc - arg_index - 1;
        forward_argv = &argv[arg_index + 1];
        if (forward_argc == 0) {
          errf("need arguments after '--'");
          return 1;
        }
        break;
      } else {
        errf("unknown option '%s'", arg);
        return 1;
      }
    }
  }
  if (forward_argc == 0) {
    usage();
    return 1;
  }

  int dir_count = argc + (upper ? 1 : 0);
  struct dir *dirs = (struct dir*)malloc(sizeof(struct dir) * dir_count);
  if (!dirs) {
    errnof("malloc failed");
    return 1;
  }

  for (int dir_index = 0; dir_index < dir_count; dir_index++) {
    struct dir *dir = &dirs[dir_index];
    if (upper)
      dir->arg = (dir_index == 0) ? upper : argv[dir_index - 1];
    else
      dir->arg = argv[dir_index];

    const char *colon_str = strchr(dir->arg, ':');
    const char *arg_source;
    if (colon_str) {
      arg_source = strndup(dir->arg, colon_str - dir->arg);
      dir->target_relative = colon_str + 1;
    } else {
      arg_source = dir->arg;
      dir->target_relative = NULL;
    }
    struct stat path_stat;
    if (-1 == stat(arg_source, &path_stat)) {
      errnof("'%s'", arg_source);
      return current_error;
    }
    dir->source = realpath2(arg_source);
    if (dir->source == NULL) {
      errnof("realpath('%s') failed", arg_source);
      return current_error;
    }
    logf("source '%s' target '%s'", dir->source, dir->target_relative);
  }

  // if we have any sub-directories to mount, we can create a tmpfs, make the subdirectories
  // and then remount the tmpfs as readonly before mounting the final overlay
  #define TMP_REX_DIR "/tmp/.rex"
  if (-1 == loggy_mkdir(TMP_REX_DIR, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)) {
    if (errno != EEXIST) {
      // errlr already logged
      return current_error;
    }
  }

  // for now we're just going to construct the new rootfs in /tmp
  char tmp_name[] = TMP_REX_DIR "/XXXXXX";
  root = mkdtemp(tmp_name);
  if (root == NULL) {
    errnof("mkdtemp failed");
    return 1;
  }
  logf("root is '%s'", root);
  root_length = strlen(root);

  int result = doit(upper != NULL, dirs, dir_count);
  loggy_rmtree(root);
  return result;
}
