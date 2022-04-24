#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>

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
static struct cdev *fib_cdev;
static struct class *fib_class;
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

char *string_number_add(char *a, char *b, char *out)
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
    buf = (char *) kmalloc(sizeof(char) * size_a + 1, GFP_KERNEL);

    reverse(data_a);
    reverse(data_b);

    memset(buf, 0, sizeof(char) * strlen(buf) + 1);
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

    if (carry) {
        // allocate a extra byte for 'carry'
        buf = (char *) krealloc(buf, sizeof(char) * size_a + 2, GFP_KERNEL);
        out = (char *) krealloc(out, sizeof(char) * size_a + 2, GFP_KERNEL);
        buf[i] = '0' + carry;
        i++;
    }
    // printk("Variable i : %d\n", i);

    reverse(buf);
    buf[i] = 0;

    /* Restore the original string */
    reverse(data_a);
    reverse(data_b);

    if (out) {
        memmove(out, buf, strlen(buf) + 1);
    }
    kfree(buf);
    return out;
}
// multiplication
// char *string_number_mul(char *a, char *b, char *out)
// {
//     size_t size_a = strlen(a), size_b = strlen(b);
//     int i = 0, j = 0, carry;
//     unsigned short *buf = (unsigned short *) kmalloc(
//         sizeof(short) * (size_a * size_b + 1), GFP_KERNEL);
//     char *result =
//         (char *) kmalloc(sizeof(char) * (size_a * size_b + 1), GFP_KERNEL);
//     memset(buf, 0, sizeof(short) * (size_a * size_b + 1));
//     memset(result, 0, sizeof(char) * (size_a * size_b + 1));
//     for (; i < size_a; i++) {
//         carry = 0;
//         for (j = 0; j < size_b; j++) {
//             *(buf + i + j) +=
//                 ((*(a + i) - '0') * (*(b + j) - '0') + carry) % 10;
//             carry = ((*(a + i) - '0') * (*(b + j) - '0') + carry) / 10;
//         }
//         *(buf + i + j) += carry;
//     }
//     if (!carry) {
//         *(buf + i + j) = carry;
//         carry = 0;
//     }

//     for (i = 0; *(buf + i) != 0 || carry != 0; i++) {
//         *(result + i) = (*(buf + i) + carry) % 10 + '0';
//         carry = (*(buf + i) + carry) / 10;
//     }

//     kfree(buf);
//     return result;
// }
static long long fib_sequence(long long k, char *buf)
{
    /* FIXME: C99 variable-length array (VLA) is not allowed in Linux kernel. */
    ktime_t kt;
    ssize_t retval = 0;
    char *num1 = (char *) kmalloc(sizeof(char) * 2, GFP_KERNEL);
    char *num2 = (char *) kmalloc(sizeof(char) * 2, GFP_KERNEL);
    char *ans = (char *) kmalloc(sizeof(char) * 2, GFP_KERNEL);
    memset(num1, 0, sizeof(char) * 2);
    memset(num2, 0, sizeof(char) * 2);

    num1[0] = '0';
    num2[0] = '1';

    if (k)
        memmove(ans, num2, 2);
    else
        memmove(ans, num1, 2);

    for (int i = 2; i <= k; i++) {
        printk("Fib : %d\n", i);
        memset(ans, 0, sizeof(char) * strlen(ans) + 1);
        ans = string_number_add(num1, num2, ans);

        num1 = (char *) krealloc(num1, sizeof(char) * strlen(num2) + 1,
                                 GFP_KERNEL);
        if (!num1)
            return -1;
        memmove(num1, num2, strlen(num2) + 1);

        num2 =
            (char *) krealloc(num2, sizeof(char) * strlen(ans) + 1, GFP_KERNEL);
        if (!num2)
            return -1;
        memmove(num2, ans, strlen(ans) + 1);
    }

    kfree(num1);
    kfree(num2);
    kt = ktime_get();  // the time that copy started
    retval = _copy_to_user(buf, ans, strlen(ans) + 1);
    kt = ktime_sub(ktime_get(), kt);  // the time that copy finished
    kfree(ans);

    // EFAULT means bad address
    if (retval < 0)
        return EFAULT;
    return (long long) ktime_to_ns(kt);
}

// static long long fib_fast_doubling(long long k, char *buf)
// {
//     return -1;
// }

static int fib_open(struct inode *inode, struct file *file)
{
    if (!mutex_trylock(&fib_mutex)) {
        printk(KERN_ALERT "fibdrv is in use");
        return -EBUSY;
    }
    return 0;
}

static int fib_release(struct inode *inode, struct file *file)
{
    mutex_unlock(&fib_mutex);
    return 0;
}

/* calculate the fibonacci number at given offset */
static ssize_t fib_read(struct file *file,
                        char *buf,
                        size_t size,
                        loff_t *offset)
{
    ktime_t kt;
    kt = ktime_get();  // start calculating fib sequence
    fib_sequence(*offset, buf);
    kt = ktime_sub(ktime_get(), kt);  // finish calculating fib sequence
    return (ssize_t) ktime_to_us(kt);
}
/* write operation is skipped */
static ssize_t fib_write(struct file *file,
                         const char *buf,
                         size_t size,
                         loff_t *offset)
{
    return 1;
}

static loff_t fib_device_lseek(struct file *file, loff_t offset, int orig)
{
    loff_t new_pos = 0;
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
    return new_pos;
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
    return rc;
failed_device_create:
    class_destroy(fib_class);
failed_class_create:
    cdev_del(fib_cdev);
failed_cdev:
    unregister_chrdev_region(fib_dev, 1);
    return rc;
}

static void __exit exit_fib_dev(void)
{
    mutex_destroy(&fib_mutex);
    device_destroy(fib_class, fib_dev);
    class_destroy(fib_class);
    cdev_del(fib_cdev);
    unregister_chrdev_region(fib_dev, 1);
}

module_init(init_fib_dev);
module_exit(exit_fib_dev);
