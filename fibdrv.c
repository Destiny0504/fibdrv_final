#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#include "fibdrv.h"

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
MODULE_VERSION("0.1");

#define DEV_FIBONACCI_NAME "fibonacci"

/* MAX_LENGTH is set to 92 because
 * ssize_t can't fit the number > 92
 */
#define MAX_LENGTH 100000
#define MAX(a, b)          \
    ({                     \
        typeof(a) _a = a;  \
        typeof(b) _b = b;  \
        _a > _b ? _a : _b; \
    })

static dev_t fib_dev = 0;
static spinlock_t fib_lock;
static struct cdev *fib_cdev;  // character device
static struct class *fib_class;
static struct workqueue_struct *fibdrv_wq;  // the workqueue for fib driver
static struct list_head supervisor;         // maintain the list of kfib
static DEFINE_MUTEX(fib_mutex);

void char_swap(char *a, char *b)
{
    *a = *a + *b;
    *b = *a - *b;
    *a = *a - *b;
}

void reverse(char *a)
{
    int counter = 0, len = strlen(a) - 1;

    if (len % 2) {
        for (; counter <= len / 2; counter++) {
            char_swap(a + counter, a + len - counter);
        }
    } else {
        for (; counter < len / 2; counter++) {
            char_swap(a + counter, a + len - counter);
        }
    }
}

char *string_number_add(char *a, char *b)
{
    char *data_a, *data_b, *buf;
    size_t size_a, size_b;
    int i, carry = 0;
    int sum;
    /*
     * Make sure the string length of 'a' is always greater than
     * the one of 'b'.
     */
    if (strlen(a) < strlen(b)) {
        void *tmp = a;
        a = b;
        b = (char *) tmp;
    }

    size_a = strlen(a);
    size_b = strlen(b);

    data_a = a;
    data_b = b;
    buf = (char *) kmalloc(size_a + 2, GFP_KERNEL);

    reverse(data_a);
    reverse(data_b);

    memset(buf, 0, size_a + 2);
    /*
     * The next two for-loop are calcuating the sum of a + b
     */
    for (i = 0; i < size_b; i++) {
        sum = (data_a[i] - '0') + (data_b[i] - '0') + carry;
        buf[i] = '0' + sum % 10;
        carry = sum / 10;
    }

    for (i = size_b; i < size_a; i++) {
        sum = (data_a[i] - '0') + carry;
        buf[i] = '0' + sum % 10;
        carry = sum / 10;
    }

    if (carry)
        *(buf + i) = '0' + carry;
    // printk("Variable i : %d\n", i);

    buf = krealloc(buf, strlen(buf) + 1, GFP_KERNEL);

    reverse(buf);
    /* Restore the original string */
    reverse(data_a);
    reverse(data_b);

    return buf;
}
// multiplication
char *string_number_mul(char *a, char *b)
{
    size_t size_a = strlen(a), size_b = strlen(b);
    int i = 0, j, carry;
    unsigned short *buf = (unsigned short *) kzalloc(
        sizeof(short) * (size_a + size_b + 4), GFP_KERNEL);
    char *result = (char *) kzalloc(size_a + size_b + 4, GFP_KERNEL);

    reverse(a);
    if (strcmp(a, b) != 0) {
        reverse(b);
    }

    for (; i < size_a; i++) {
        for (j = 0; j < size_b; j++) {
            *(buf + i + j) +=
                ((*(a + i) - '0') * (*(b + j) - '0') + carry) % 10;
            carry = ((*(a + i) - '0') * (*(b + j) - '0') + carry) / 10;
        }
        *(buf + i + j) += carry;
        carry = 0;
    }

    carry = 0;

    for (i = 0; i < size_a + size_b; i++) {
        *(result + i) = (*(buf + i) + carry) % 10 + '0';
        carry = (*(buf + i) + carry) / 10;
    }
    for (i = size_a + size_b - 1; *(result + i) == '0' && i > 0; i--)
        *(result + i) = 0;

    reverse(a);
    if (strcmp(a, b) != 0) {
        reverse(b);
    }
    reverse(result);


    if (strlen(result) < 1) {
        *result = '0';
    }

    result = (char *) krealloc(result, strlen(result) + 1, GFP_KERNEL);

    kfree(buf);
    return result;
}

static long long fib_sequence(long long k, char *buf)
{
    /* FIXME: C99 variable-length array (VLA) is not allowed in Linux kernel. */
    ktime_t kt;
    ssize_t retval = 0;
    char *num1 = (char *) kzalloc(sizeof(char) * 2, GFP_KERNEL);
    char *num2 = (char *) kzalloc(sizeof(char) * 2, GFP_KERNEL);
    char *ans = (char *) kmalloc(sizeof(char) * 2, GFP_KERNEL);

    num1[0] = '0';
    num2[0] = '1';

    if (k)
        memmove(ans, num2, 2);
    else
        memmove(ans, num1, 2);

    for (int i = 2; i <= k; i++) {
        printk("Fib : %d\n", i);
        kfree(ans);
        ans = string_number_add(num1, num2);

        kfree(num1);

        num1 = num2;
        num2 = (char *) kmalloc(strlen(ans) + 1, GFP_KERNEL);
        memmove(num2, ans, strlen(ans) + 1);
    }

    kfree(num1);
    kfree(num2);
    kt = ktime_get();  // the time that copy started
    retval = copy_to_user(buf, ans, strlen(ans) + 1);
    kt = ktime_sub(ktime_get(), kt);  // the time that copy finished
    kfree(ans);

    // EFAULT means bad address
    if (retval < 0)
        return EFAULT;
    return (long long) ktime_to_ns(kt);
}


char *string_number_min(char *a, char *b)
{
    size_t size_a = strlen(a), size_b = strlen(b);
    int i = 0, carry = 0;
    char *buf = (char *) kmalloc(size_a + 1, GFP_KERNEL);
    memset(buf, 0, size_a + 1);

    reverse(a);
    reverse(b);

    for (; i < size_b; i++) {
        if (((*(a + i) - '0') - (*(b + i) - '0') + carry) < 0) {
            *(buf + i) = *(a + i) - *(b + i) + '0' + carry + 10;
            carry = -1;
        } else {
            *(buf + i) = *(a + i) - *(b + i) + '0' + carry;
            carry = 0;
        }
    }

    for (; i < size_a; i++) {
        if (((*(a + i) - '0') + carry) < 0) {
            *(buf + i) = *(a + i) + carry + 10;
            carry = -1;
        } else {
            *(buf + i) = *(a + i) + carry;
            carry = 0;
        }
    }

    // remove the leading zero
    if (*(buf + i - 1) == '0' && strlen(buf) > 1)
        *(buf + i - 1) = 0;

    reverse(a);
    reverse(b);
    reverse(buf);

    buf = (char *) krealloc(buf, strlen(buf) + 1, GFP_KERNEL);
    return buf;
}


static long long fib_fast_doubling(long long k, char *buf)
{
    char *fib_0, *fib_1, *two = "2\0";
    long long i;
    ssize_t retval = 0;

    fib_0 = (char *) kmalloc(2, GFP_KERNEL);
    fib_1 = (char *) kmalloc(2, GFP_KERNEL);

    memset(fib_0, 0, 2);
    memset(fib_1, 0, 2);

    *fib_0 = '0';
    *fib_1 = '1';

    // datatype long long has 64 bits
    for (i = 1u << (64 - __builtin_clz(k)); i > 0; i >>= 1) {
        char *a, *b, *tmp_0, *tmp_1;
        // calcualting a
        tmp_0 = string_number_mul(two, fib_1);
        tmp_1 = string_number_min(tmp_0, fib_0);
        a = string_number_mul(tmp_1, fib_0);
        kfree(tmp_0);
        kfree(tmp_1);

        // calculating b
        tmp_0 = string_number_mul(fib_0, fib_0);
        tmp_1 = string_number_mul(fib_1, fib_1);
        b = string_number_add(tmp_0, tmp_1);
        kfree(tmp_0);
        kfree(tmp_1);

        kfree(fib_0);
        kfree(fib_1);
        if (i & k) {
            fib_0 = (char *) kmalloc(strlen(b) + 1, GFP_KERNEL);
            memmove(fib_0, b, strlen(b) + 1);
            fib_1 = string_number_add(a, b);
        } else {
            fib_0 = (char *) kmalloc(strlen(a) + 1, GFP_KERNEL);
            fib_1 = (char *) kmalloc(strlen(b) + 1, GFP_KERNEL);
            memmove(fib_0, a, strlen(a) + 1);
            memmove(fib_1, b, strlen(b) + 1);
        }
        kfree(a);
        kfree(b);
    }
    kfree(fib_1);
    if (strlen(fib_0) |
        ((int) ((20898764025 * k - 34948500216) / 100000000000) + 1)) {
        retval = copy_to_user(buf, fib_0, strlen(fib_0) + 1);
    }
    kfree(fib_0);
    printk("retern val : %lu", retval);
    if (retval < 0)
        return EFAULT;
    return 0;
}

static int fib_open(struct inode *inode, struct file *file)
{
    return 0;
}

static int fib_release(struct inode *inode, struct file *file)
{
    return 0;
}

/* work function*/
void work_fn(struct work_struct *work)
{
    struct kfib *worker = container_of(work, struct kfib, fib_work);

    printk(KERN_INFO "Executing Workqueue Function\n");
    fib_fast_doubling(worker->offset, worker->buffer);
    kfree(worker);
}

static struct work_struct *create_work(long long data, char *buf)
{
    struct kfib *work;

    if (!(work = kmalloc(sizeof(struct kfib), GFP_KERNEL)))
        return NULL;

    work->offset = data;
    work->buffer = buf;

    INIT_WORK(&work->fib_work, work_fn);

    return &work->fib_work;
}

/* calculate the fibonacci number at given offset */
static ssize_t fib_read(struct file *file,
                        char *buf,
                        size_t size,
                        loff_t *offset)
{
    // ktime_t kt;
    // // ssize_t retval = 0;
    // char *test_copy = (char *) kmalloc(size, GFP_KERNEL);
    // memset(test_copy, '1', *offset);
    // *(test_copy + *offset - 1) = 0;
    // kt = ktime_get();  // start calculating fib sequence
    // fib_sequence(*offset, buf);
    // fib_fast_doubling(*offset, buf);
    // // if (access_ok(buf, *offset))
    // //     retval = _copy_to_user(buf, test_copy, *offset);
    // kt = ktime_sub(ktime_get(), kt);  // finish calculating fib sequence
    // kfree(test_copy);
    // return (ssize_t) ktime_to_ns(kt);

    /* for work queue testing */
    struct work_struct *work = NULL;
    struct fib_worker *l, *tar;
    int pid = task_pid_nr(current);
    long long tmp = -1;
    list_for_each_entry_safe (tar, l, &supervisor, list) {
        if (unlikely(tar->pid == pid)) {
            tmp = tar->offset;
            printk("Thread %ld, target %d, its pid = %d", size, tar->offset,
                   tar->pid);
            spin_lock_irq(&fib_lock);
            __list_del_entry(&tar->list);
            spin_unlock_irq(&fib_lock);
            kfree(tar);
            break;
        }
    }
    if (unlikely(tmp < 0)) {
        printk(KERN_ALERT "Didn't assign the value for reading.");
        return -1;
    }
    // printk("start work %lld", tmp);
    while (1) {
        if (unlikely(!(work = create_work(tmp, buf)))) {
            printk(KERN_ERR DEV_FIBONACCI_NAME
                   ": can't create work\nstart a next try");
            continue;
        }
        printk("%s", queue_work(fibdrv_wq, work) ? "true" : "false");
        // flush_work(work);
        // mdelay(200);
        printk("finish work %lld", tmp);
        return -1;
    }
    // if (retval < 0)
    //     return -1;
}
/* write operation is skipped */
static ssize_t fib_write(struct file *file,
                         const char *buf,
                         size_t size,
                         loff_t *offset)
{
    ssize_t retval = 0;
    ktime_t kt;
    char *test_copy = (char *) kmalloc(size, GFP_KERNEL);
    kt = ktime_get();
    if (access_ok(buf, size))
        retval = _copy_from_user(test_copy, buf, size);

    kt = ktime_sub(ktime_get(), kt);
    printk("%ld", strlen(test_copy));
    kfree(test_copy);

    if (retval < 0)
        return -1;
    return (ssize_t) ktime_to_ns(kt);
}

static loff_t fib_device_lseek(struct file *file, loff_t offset, int orig)
{
    loff_t new_pos = 0;
    struct fib_worker *work;
    struct fib_worker *l, *tar;
    work = kzalloc(sizeof(struct fib_worker), GFP_KERNEL);

    switch (orig) {
    case 0: /* SEEK_SET: */
        new_pos = offset;
        break;
    case 1: /* SEEK_CUR: */
        new_pos = file->f_pos + offset;
        break;
    case 2: /* SEEK_END: */
        new_pos = MAX_LENGTH - offset;
        break;
    }

    if (new_pos > MAX_LENGTH)
        new_pos = MAX_LENGTH;  // max case
    if (new_pos < 0)
        new_pos = 0;        // min case
    file->f_pos = new_pos;  // This is what we'll use now

    work->pid = task_pid_nr(current);
    work->offset = new_pos;
    list_for_each_entry_safe (tar, l, &supervisor, list) {
        printk("Entry in supervisor %d, its pid = %d", tar->offset, tar->pid);
        if (work->pid == tar->pid) {
            printk(KERN_ALERT
                   "There is a value assigned before. Please read it first");
            kfree(work);
            return -1;
        }
    }
    spin_lock_irq(&fib_lock);
    list_add_tail(&work->list, &supervisor);
    spin_unlock_irq(&fib_lock);
    printk("Add pid : %d, target : %d", work->pid, work->offset);
    return new_pos;
}
static void free_worker(void)
{
    struct fib_worker *l, *tar;
    list_for_each_entry_safe (tar, l, &supervisor, list) {
        kfree(tar);
    }
}

const struct file_operations fib_fops = {
    .owner = THIS_MODULE,
    .read = fib_read,
    .write = fib_write,
    .open = fib_open,
    .release = fib_release,
    .llseek = fib_device_lseek,
};

static int __init init_fib_dev(void)
{
    int rc = 0;

    mutex_init(&fib_mutex);
    INIT_LIST_HEAD(&supervisor);
    spin_lock_init(&fib_lock);

    // Let's register the device
    // This will dynamically allocate the major number
    rc = alloc_chrdev_region(&fib_dev, 0, 1, DEV_FIBONACCI_NAME);

    if (rc < 0) {
        printk(KERN_ALERT
               "Failed to register the fibonacci char device. rc = %i",
               rc);
        return rc;
    }

    fib_cdev = cdev_alloc();
    if (fib_cdev == NULL) {
        printk(KERN_ALERT "Failed to alloc cdev");
        rc = -1;
        goto failed_cdev;
    }
    fib_cdev->ops = &fib_fops;
    rc = cdev_add(fib_cdev, fib_dev, 1);

    if (rc < 0) {
        printk(KERN_ALERT "Failed to add cdev");
        rc = -2;
        goto failed_cdev;
    }

    fib_class = class_create(THIS_MODULE, DEV_FIBONACCI_NAME);

    if (!fib_class) {
        printk(KERN_ALERT "Failed to create device class");
        rc = -3;
        goto failed_class_create;
    }

    if (!device_create(fib_class, NULL, fib_dev, NULL, DEV_FIBONACCI_NAME)) {
        printk(KERN_ALERT "Failed to create device");
        rc = -4;
        goto failed_device_create;
    }
    fibdrv_wq = alloc_workqueue(DEV_FIBONACCI_NAME, 0, 0);
    if (!fibdrv_wq) {
        printk("Failed to create workqueue");
        rc = -ENOMEM;
        goto failed_cworkqueue;
    }
    return rc;
failed_device_create:
    class_destroy(fib_class);
failed_class_create:
    cdev_del(fib_cdev);
failed_cdev:
    unregister_chrdev_region(fib_dev, 1);
failed_cworkqueue:
    destroy_workqueue(fibdrv_wq);
    return rc;
}

static void __exit exit_fib_dev(void)
{
    mutex_destroy(&fib_mutex);
    device_destroy(fib_class, fib_dev);
    class_destroy(fib_class);
    cdev_del(fib_cdev);
    unregister_chrdev_region(fib_dev, 1);
    free_worker();  // free the whole list
    destroy_workqueue(fibdrv_wq);
}

module_init(init_fib_dev);
module_exit(exit_fib_dev);
