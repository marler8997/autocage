#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>

#include "log.h"

#include "inode.h"

#define REXFS_MAGIC 0xceeabc8f // just a random number (not sure if this is necessary)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jonathan Marler");

struct rexfs_inode {
  struct inode ai_inode;
  // TODO more members
};

struct inode *rexfs_alloc_inode(struct super_block *sb)
{
  struct rexfs_inode *inode;
  devlog("alloc_inode");
  // TODO: probably want to use a kmem_cache
  inode = (struct rexfs_inode*)kzalloc(sizeof(struct rexfs_inode), GFP_KERNEL);
  if (!inode) {
    err("kmalloc(%lu, GFP_KERNEL) failed", sizeof(struct rexfs_inode));
    return NULL;
  }
  return &inode->ai_inode;
}

void rexfs_destroy_inode(struct inode *inode)
{
  devlog("destroy_inode");
  kfree(inode);
}

void rexfs_put_super(struct super_block *sb)
{
  devlog("put_super");
  // free superblock that was allocated in rexfs_mount
  // currently it uses mount_nodev, which uses "sget"
  drop_super(sb);
}

static struct super_operations const rexfs_super_ops = {
  //.alloc_inode = rexfs_alloc_inode,
  //.destroy_inode = rexfs_destroy_inode,
  //.put_super = rexfs_put_super,
};

static int rexfs_fill_super(struct super_block *sb, void *data, int flags)
{
  devlog("fill_super +");
  sb->s_magic = REXFS_MAGIC; // not sure if this is necessary
  sb->s_op = &rexfs_super_ops;
  sb->s_maxbytes = MAX_LFS_FILESIZE; // kernel indicates this is needed or bad things will happen
  sb->s_blocksize = PAGE_SIZE; // not sure if this is needed
  sb->s_blocksize_bits = PAGE_SHIFT; // sure sure if this is needed
  sb->s_time_gran = 1; // not sure if this is needed

  {
    //struct inode *rexfs_make_inode(struct super_block *sb, const struct inode *dir, umode_t mode);
    struct inode *root_inode = new_inode(sb);
    if (!root_inode) {
      err("new_node failed");
      return -ENOMEM;
    }
    {
      int result = rexfs_init_inode(root_inode, NULL, S_IFDIR | (S_IRUSR | S_IXUSR) |
                     (S_IRGRP | S_IXGRP) | (S_IROTH | S_IXOTH));
      if (result)
        return result;
    }
    devlog("root inode is %lu", root_inode->i_ino);
    sb->s_root = d_make_root(root_inode);
    if (!sb->s_root) {
      // iput(root_inode)?
      err("root inode creation failed");
      return -ENOMEM;
    }
  }
  devlog("fill_super -");
  return 0;
}

struct dentry *rexfs_mount(struct file_system_type * fs_type, int flags,
                         const char *dev, void *data)
{
  struct dentry *entry;
  devlog("mount");
  // mount_ns
  // mount_bdev NO
  // mount_nodev
  // mount_single
  // mount_fs
  // TODO: check if rexfs has already been mounted, if so, return the existing root entry
  //entry = mount_nodev(fs_type, flags, data, rexfs_fill_super);
  // mount_single: a filesystem which shares the instance between all mounts
  // NOTE: mount_single may already lookup the existing superblock for me
  entry = mount_single(fs_type, flags, data, rexfs_fill_super);
  if (IS_ERR(entry)) {
    err("mount (flags=0x%x) failed", flags);
  } else {
    devlog("mounted");
  }
  return entry;
}

void rexfs_kill_sb(struct super_block *sb)
{
  devlog("kill_sb");
  // free superblock that was allocated in rexfs_mount
  // currently it uses mount_nodev, which uses "sget"
  //kill_anon_super(sb);
  kill_litter_super(sb);
}

static struct file_system_type rexfs_file_system_type = {
  .name = "rexfs",
  .owner = THIS_MODULE,
  .fs_flags = FS_USERNS_MOUNT, // FS_HAS_SUBTYPE? FS_RENAME_DOES_D_MOVE? FS_NO_DCACHE?
  .mount = rexfs_mount,
  .kill_sb = rexfs_kill_sb,
};

#define INIT_STATE_INITIAL             0
#define INIT_STATE_MOUNT_POINT_CREATED 1
#define INIT_STATE_FS_REGISTERED       2

static unsigned char init_state = INIT_STATE_INITIAL;

int __init init_module(void)
{
  devlog("--------------------------------------------------------------------------------");
  devlog("init_module");
  {
    int ret;
    devlog("- create_mount_point");
    ret = sysfs_create_mount_point(fs_kobj, "rex");
    if (ret) {
      err("sysfs_create_mount_point failed (e=%d)", ret);
      return ret;
    }
  }
  init_state++;
  {
    int ret;
    devlog("- register_filesystem");
    ret = register_filesystem(&rexfs_file_system_type);
    if (ret) {
      err("register_filesystem failed (e=%d)", ret);
      return ret;
    }
  }
  init_state++;
  return 0;
}

void __exit cleanup_module(void)
{
  devlog("cleanup_module");
  switch (init_state) {
  case INIT_STATE_MOUNT_POINT_CREATED:
    devlog("- remove mount point");
    sysfs_remove_mount_point(fs_kobj, "rex");
    // continue to next state
  case INIT_STATE_FS_REGISTERED:
    devlog("- unregister_filesystem");
    {
      int ret = unregister_filesystem(&rexfs_file_system_type);
      if (ret) {
        err("unregister_filesystem failed (e=%d)", ret);
      }
    }
    // continue to next state
  case INIT_STATE_INITIAL:
    break;
  }
}
