#include <linux/workqueue.h>  // Required for workqueues

struct kfib {
    int offset;
    /* This pointer points to the space we need to store the anser */
    char *buffer;
    /* use for queue work on workqueue */
    struct work_struct fib_work;
    struct list_head list;
    /* Store the pid of the user process or thread for lock-free */
    int pid;
};

struct fib_worker {
    struct list_head list;
    /* Store the pid of the user process or thread for lock-free */
    int pid;
    int offset;
};
