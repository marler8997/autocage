

# Rex File System (rexfs)

This filesystem restrict access based on process id and each process will inherit the access of the parent, but can override any configuration as well. Each process will need to have some way of mounting this VFS and configuring it.