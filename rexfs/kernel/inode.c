#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/namei.h>

#include "log.h"

MODULE_LICENSE("GPL");

// Note: keep loop logic in sync with fill_rexfs_path
unsigned get_rexfs_path_size(struct dentry *dentry)
{
  unsigned size;
  struct dentry *parent = dentry->d_parent;
  if (parent == dentry) {
    BUG_ON(dentry->d_name.len != 1);
    return 1;
  }
  size = 0;
  for (;;) {
    size += 1 + dentry->d_name.len;
    dentry = parent;
    parent = dentry->d_parent;
    if (parent == dentry)
      return size;
  }
}
// Note: keep loop logic in sync with get_rexfs_path_size
void fill_rexfs_path(struct dentry *dentry, char *buffer, unsigned size)
{
  struct dentry *parent = dentry->d_parent;
  buffer[size] = '\0';
  if (parent == dentry) {
    BUG_ON(dentry->d_name.len != 1);
    BUG_ON(size != 1);
    buffer[0] = '/';
    return;
  }

  for (;;) {
    {
      unsigned name_len = dentry->d_name.len;
      size -= name_len;
      memcpy(buffer + size, dentry->d_name.name, name_len);
      size--;
      buffer[size] = '/';
    }
    dentry = parent;
    parent = dentry->d_parent;
    if (parent == dentry) {
      BUG_ON(size != 0);
      return;
    }
  }
}
char *kmalloc_rexfs_path(struct dentry *dentry)
{
  unsigned rexfs_path_size;
  char *rexfs_path;
  // TODO: other functions like d_path do an rcu_read_lock before
  //       going through dentries
  rexfs_path_size = get_rexfs_path_size(dentry);
  rexfs_path = kmalloc(rexfs_path_size + 1, GFP_KERNEL);
  if (!rexfs_path)
    return ERR_PTR(-ENOMEM);

  fill_rexfs_path(dentry, rexfs_path, rexfs_path_size);
  //devlog("kmalloc_rexfs_path(inode %lu) '%s'", dentry->d_inode->i_ino, rexfs_path);
  devlog("kmalloc_rexfs_path '%s'", rexfs_path);
  return rexfs_path;
}

int big_heirarchy_kern_path(struct dentry *dentry, struct path *path)
{
  char *rexfs_path;

  // TODO: alloca instead?
  rexfs_path = kmalloc_rexfs_path(dentry);
  if (IS_ERR(rexfs_path))
    return PTR_ERR(rexfs_path);

  {
    int ret = kern_path(rexfs_path, 0, path);
    kfree(rexfs_path);
    return ret;
  }
}
struct file *big_heirarchy_open(struct dentry *dentry, int flags, umode_t mode)
{
  char *rexfs_path;

  // TODO: alloca instead?
  rexfs_path = kmalloc_rexfs_path(dentry);
  if (IS_ERR(rexfs_path))
    return (struct file*)rexfs_path;
  {
    struct file *ret = filp_open(rexfs_path, flags, mode);
    kfree(rexfs_path);
    return ret;
  }
}



static int create(struct inode *inode, struct dentry *dentry, umode_t mode, bool what_is_this)
{
  err("create not implemented");
  return -ENOSYS; // fail
}
static int link(struct dentry *dentry1, struct inode *inode, struct dentry *dentry2)
{
  err("link not implemented");
  return -ENOSYS; // fail
}
static int unlink(struct inode *inode, struct dentry *dentry)
{
  err("unlink not implemented");
  return -ENOSYS; // fail
}
static int symlink(struct inode *inode, struct dentry *dentry, const char *what_is_this)
{
  err("symlink not implemented");
  return -ENOSYS; // fail
}
static int mkdir(struct inode *inode, struct dentry *dentry, umode_t mode)
{
  err("mkdir not implemented");
  return -ENOSYS; // fail
}
static int rmdir(struct inode *inode, struct dentry *dentry)
{
  err("rmdir not implemented");
  return -ENOSYS; // fail
}
static int mknod(struct inode *inode, struct dentry *dentry, umode_t mode, dev_t what_is_this)
{
  err("mknod not implemented");
  return -ENOSYS; // fail
}
static int rename(struct inode *inode1, struct dentry *dentry1, struct inode *inode2, struct dentry *dentry2, unsigned int what_is_this)
{
  err("rename not implemented");
  return -ENOSYS; // fail
}
static int readlink(struct dentry *dentry, char __user *what_is_this1, int what_is_this2)
{
  err("readlink not implemented");
  return -ENOSYS; // fail
}
static const char *get_link(struct dentry *dentry, struct inode *inode, struct delayed_call *what_is_this)
{
  err("get_link not implemented");
  return NULL; // fail
}
static int permission(struct inode *inode, int desired)
{
  if (inode->i_sb->s_root->d_inode == inode) {
    int denied;
    int allowed = MAY_NOT_BLOCK | MAY_EXEC | MAY_READ | MAY_ACCESS | MAY_OPEN | MAY_CHDIR;

    denied = desired & ~allowed;
    if (denied) {
      devlog("permission(inode=%lu) denied (desired=0x%x, allowed=0x%x, denied=0x%x)",
             inode->i_ino, desired, allowed, denied);
      return -EPERM;
    }
    devlog("permission(inode=%lu) granted (desired=0x%x, allowed=0x%x)", inode->i_ino, desired, allowed);
    return 0; // success
  }
  err("permission(inode=%lu) not implemented (desired=0x%x)", inode->i_ino, desired);
  return -ENOSYS; // fail
}
/*
int get_acl(struct inode *inode, int)
{
  err("get_acl not implemented");
  return -ENOSYS; // fail
}
*/

/*
int setattr(struct dentry *dentry, struct iattr *);
int getattr(const struct path *, struct kstat *, u32, unsigned int);
ssize_t listxattr(struct dentry *dentry, char *, size_t);
void update_time(struct inode *inode, struct timespec *, int);
*/
/*
int tmpfile(struct inode *inode, struct dentry *dentry, umode_t);
*/


static struct dentry *dir_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags)
{
  //char *rexfs_path;

  devlog("dir_lookup(inode=%lu, flags=0x%x)", dir->i_ino, flags);
  if (dir->i_sb->s_root->d_inode != dir) {
    err("dir_lookup, inode %lu is not root, not sure what to do", dir->i_ino);
    return NULL;
  }

  // dir
  {
    struct path path;
    int result = big_heirarchy_kern_path(dentry, &path);
    if (result)
      return ERR_PTR(result);
    return path.dentry;
  }

  /*
  rexfs_path = kmalloc_rexfs_path(dentry);
  if (IS_ERR(rexfs_path)) {
    err("kmalloc_rexfs_path failed (e=%lu)", PTR_ERR(rexfs_path));
    return (struct dentry*)rexfs_path;
  }

  devlog("dir_lookup: dentry='%s' (not implemented)", rexfs_path);
  dump_stack();
  kfree(rexfs_path);
  return ERR_PTR(-ENOSYS);
  */
}

/*
static int dir_atomic_open(struct inode *dir, struct dentry *dentry, struct file *file,
                       unsigned open_flag, umode_t create_mode, int *opened)
{
  if (dir->i_sb->s_root->d_inode != dir) {
    err("dir_atomic_open, inode %lu is not root, not sure what to do", dir->i_ino);
    return NULL;
  }


  err("dir_atomic_open not implemented (file->private_data=%p)", file->private_data);
  dump_stack();
  return -ENOSYS; // fail
}
*/
static struct inode_operations rexfs_dir_inode_ops = {
  .create = create,
  .lookup = dir_lookup,
  .link = link,
  .unlink = unlink,
  .symlink = symlink,
  .mkdir = mkdir,
  .rmdir = rmdir,
  .mknod = mknod,
  .rename = rename,
  .readlink = readlink,
  .get_link = get_link,
  .permission = permission,
  //.atomic_open = dir_atomic_open,
};
static struct inode_operations rexfs_file_inode_ops = {
  /*
  .create = create,
  .lookup = lookup,
  .link = link,
  .unlink = unlink,
  .symlink = symlink,
  .mkdir = mkdir,
  .rmdir = rmdir,
  .mknod = mknod,
  .rename = rename,
  .readlink = readlink,
  .get_link = get_link,
  .permission = permission,
  .atomic_open = atomic_open,
  */
};
static struct inode_operations rexfs_link_inode_ops = {
  .create = create,
  //.lookup = lookup,
  .link = link,
  .unlink = unlink,
  .symlink = symlink,
  .mkdir = mkdir,
  .rmdir = rmdir,
  .mknod = mknod,
  .rename = rename,
  .readlink = readlink,
  .get_link = get_link,
  .permission = permission,
  //.atomic_open = atomic_open,
};


/*
#define BIG_HEIARCHY_ROOT_INODE 2
int get_big_heiarchy_by_name(struct dentry *dentry, struct dentry **big_heiarchy_parent)
{
  if (dentry->d_parent == dentry) {
    // return the root of the big heiarchy

  }
}
*/


int dir_open(struct inode *inode, struct file *file)
{
  devlog("dir_open(inode=%lu)", inode->i_ino);
  if (inode == inode->i_sb->s_root->d_inode) {
    file->private_data = big_heirarchy_open(file->f_path.dentry, O_DIRECTORY, 0);
    if (IS_ERR(file->private_data))
      return PTR_ERR(file->private_data);

    //devlog("private_data=%p", file->private_data);
    return 0;
  } else {
    err("inode is not root, not sure what to do");
    return -ENOSYS;
  }
  /*
  from fs/libfs.c: (file->private_data = d_alloc_cursor(file->f_path.dentry))
                   return file->private_data ? 0 : -ENOMEM;
  maybe my impl:   file->private_data = make_dir_iterator(file->f_path.dentry);
  */
}
int dir_release(struct inode *inode, struct file *file)
{
  devlog("dir_release(inode=%lu)", inode->i_ino);

  BUG_ON(file->private_data == 0 || IS_ERR(file->private_data));
  return filp_close(file->private_data, NULL);
  /*
  from fs/libfs.c: dput(file->private_data);
  */
}
loff_t dir_llseek(struct file *file, loff_t offset, int whence)
{
  err("dir_lseek not implemented");
  return -ENOSYS;
}
int dir_iterate_shared(struct file *file, struct dir_context *ctx)
{
  // TODO: can f_op be NULL?
  if (!((struct file*)file->private_data)->f_op->iterate_shared) {
    err("underlying dir does not have an interate_shared function");
    return -ENOSYS;
  }
  return ((struct file*)file->private_data)->f_op->iterate_shared(file->private_data, ctx);
}
static const struct file_operations rexfs_dir_file_ops = {
  .open = dir_open,
  .release = dir_release,
  .llseek = dir_llseek,
  .read = generic_read_dir, // just returns -EISDIR
  .iterate_shared = dir_iterate_shared,
  //.fsync = dir_fsync,
};
static const struct file_operations rexfs_file_file_ops = {
};

int rexfs_init_inode(struct inode *inode, const struct inode *parent, umode_t mode)
{
  inode->i_ino = get_next_ino();
  inode_init_owner(inode, parent, mode);
  inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(inode);
  switch (mode & S_IFMT) {
  case S_IFREG:
    inode->i_op  = &rexfs_file_inode_ops;
    inode->i_fop = &rexfs_file_file_ops;
    break;
  case S_IFDIR:
    inode->i_op  = &rexfs_dir_inode_ops;
    inode->i_fop = &rexfs_dir_file_ops;
    inc_nlink(inode);
    break;
  case S_IFLNK:
    inode->i_op  = &rexfs_link_inode_ops;
    //inode->i_fop = &rexfs_file_ops;
    break;
  default:
    err("unknown inode mode 0x%x", mode & S_IFMT);
    return -EINVAL;
  }
  return 0;
}
