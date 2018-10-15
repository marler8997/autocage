#define devlog(fmt,...) pr_err("rexfs: DEBUG " fmt "\n", ##__VA_ARGS__)
#define err(fmt,...) pr_err("rexfs: Error: " fmt "\n", ##__VA_ARGS__)
