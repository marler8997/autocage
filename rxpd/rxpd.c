/*
For now this is going to use a character device, but
I'm not sure I'll keep this interface.  I might use sysfs entries instead, not sure.
 */
//#include <linux/init.h>
//#include <linux/slab.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>

#include "log.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jonathan Marler");

struct file_operations fops = {
  .owner = THIS_MODULE,
};


static dev_t devnum = 0;
static struct cdev cdev;
static bool cdev_added = 0;
//static struct class *class = NULL;
//static struct device *device = NULL;
int __init init_module(void)
{
  devlog("rxpd: init");
  {
    int result = alloc_chrdev_region(&devnum, 0, 1, "rxpd");
    if (result < 0) {
      err("alloc_chrdev_region failed %d", result);
      return result;
    }
  }
  devlog("rxpd: major=%d minor=%d", MAJOR(devnum), MINOR(devnum));
  cdev_init(&cdev, &fops);
  {
    int result = cdev_add(&cdev, devnum, 1);
    if (result < 0) {
      err("cdev_add failed %d", result);
      return result;
    }
  }
  cdev_added = 1;
  /*
  class = class_create(THIS_MODULE, "rxpd");
  if (class == NULL) {
    err("class_created failed");
    return -ENODEV; // TODO: not a good error number
  }
  device = device_create(class, NULL, major, NULL, "rxpd");
  if (device == NULL) {
    err("device_create failed");
    return -ENODEV;
  }
  */
  return 0;
}

void __exit cleanup_module(void)
{
  devlog("rxpd: cleanup");
  /*
  if (device) {
    device_destroy(class, major);
    if (class) {
      class_destroy(class);
    }
  }
  */
  if (cdev_added) {
    cdev_del(&cdev);
  }
  if (devnum != 0) {
    unregister_chrdev_region(devnum, 1);
  }
}
