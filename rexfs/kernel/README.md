# The rexfs (Rex Filesystem) kernel module

rexfs stands for "Restricted EXecution Filesystem".  Processes can view the entire filesystem through the rexfs mount point and configure how much of it to expose. This makes it great for setting up a restricted rootfs to "chroot" to.

The standard mount location for rexfs is at `/sys/fs/rex`.  The rexfs module will create this mount point directory when it is installed, however, the module does not automatically mount it (for now).  That could be done by running `mount -t rexfs "" /sys/fs/rex`.

```
/sys/fs/rex              - the mount point of rexfs
/sys/fs/rex/<pid>/root   - the rootfs when restricted for the process <pid>
/sys/fs/rex/<pid>/config - the configuration for <pid>
/sys/fs/rex/self         - a link to <pid> where pid is the current pid
```

One way a process could configure its rex rootfs would be for it to write contents to `/sys/fs/rex/<pid>/config`.  Let's take the following example:
```
rex --read=hello.txt cat hello.txt
```

Then when rex is setting up the execution of the cat command, it would do something like:
```
auto fd = open("/sys/fs/rex/self/config", "w");
auto result = write(fd, "read:hello.txt\n");
// todo: check result
execve(argv);
```
