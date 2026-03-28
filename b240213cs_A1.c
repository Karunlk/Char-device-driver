#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/version.h>
#include <linux/string.h>
#include <linux/sched.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("YOUR NAME");
MODULE_DESCRIPTION("Assignment 1 - Character Device Driver with waitqueue and timer");

// constants
#define DEVICE_NAME  "mydev"
#define BUF_SIZE     256

// module parameters
static int kernel_version[2];
static unsigned int kv_argc = 0;
module_param_array(kernel_version, int, &kv_argc, 0644);
MODULE_PARM_DESC(kernel_version, "Kernel version to validate e.g. kernel_version=6,5");

static int timer_val = 30;
module_param(timer_val, int, 0644);
MODULE_PARM_DESC(timer_val, "Time limit in seconds for completing read then write");

// globals
static int  major_number;
static char kernel_buf[BUF_SIZE];
static int  buf_len = 0;

// state flags
static int read_done  = 0;
static int write_done = 0;
static int timed_out  = 0;

// waitqueue
static DECLARE_WAIT_QUEUE_HEAD(wq);

// timer
static struct timer_list my_timer;

// forward declarations 
static int     device_open   (struct inode *, struct file *);
static int     device_release(struct inode *, struct file *);
static ssize_t device_read   (struct file *, char __user *, size_t, loff_t *);
static ssize_t device_write  (struct file *, const char __user *, size_t, loff_t *);

// file operations dispatch table
static struct file_operations fops = {
    .owner   = THIS_MODULE,
    .open    = device_open,
    .release = device_release,
    .read    = device_read,
    .write   = device_write,
};

// Version validation
static int check_kernel_version(void)
{
    int expected;

    if (kv_argc != 2) {
        printk(KERN_ERR "mydev: kernel_version needs exactly 2 values (major,minor)\n");
        return -EINVAL;
    }

    expected = KERNEL_VERSION(kernel_version[0], kernel_version[1], 0);

    if ((LINUX_VERSION_CODE & ~0xFF) != (expected & ~0xFF)) {
        printk(KERN_ERR
               "mydev: version mismatch! built for %d.%d, got %d.%d\n",
               (LINUX_VERSION_CODE >> 16) & 0xFF,
               (LINUX_VERSION_CODE >> 8)  & 0xFF,
               kernel_version[0], kernel_version[1]);
        return -EPERM;
    }

    printk(KERN_INFO "mydev: kernel version %d.%d verified OK\n",
           kernel_version[0], kernel_version[1]);
    return 0;
}

// Timer callback — fires when time runs out
static void timer_callback(struct timer_list *t)
{
    timed_out = 1;
    printk(KERN_WARNING "mydev: timer expired after %d seconds\n", timer_val);
    wake_up(&wq);
}

// File operations
static int device_open(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "mydev: device opened\n");
    return 0;
}

static int device_release(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "mydev: device closed\n");
    return 0;
}

static ssize_t device_read(struct file *filep, char __user *user_buf,
                            size_t count, loff_t *offset)
{
    int bytes_to_send;
    int not_copied;
    const char *msg = "mydev: ready — write your username to this device\n";

    // if kernel_buf is empty, sending a default prompt so cat has output
    
     if (*offset > 0)
        return 0;   // EOF — tells cat we are done writing

    if (buf_len == 0) {
        bytes_to_send = strlen(msg);
        not_copied = copy_to_user(user_buf, msg, bytes_to_send);
    } else {
        bytes_to_send = min((int)count, buf_len);
        not_copied = copy_to_user(user_buf, kernel_buf, bytes_to_send);
        buf_len = 0;
    }

    if (not_copied != 0) {
        printk(KERN_WARNING "mydev: read — failed to copy %d bytes\n", not_copied);
        return -EFAULT;
    }
*offset += bytes_to_send;

    printk(KERN_INFO "mydev: sent %d bytes to userspace\n", bytes_to_send);

    if (!read_done) {
        read_done = 1;
        printk(KERN_INFO "mydev: READ action completed — waiting for write\n");
    }

    return bytes_to_send;
}

static ssize_t device_write(struct file *filep, const char __user *user_buf,
                              size_t count, loff_t *offset)
{
    int not_copied;
    int to_copy = min((int)count, BUF_SIZE - 1);

    not_copied = copy_from_user(kernel_buf, user_buf, to_copy);
    if (not_copied != 0) {
        printk(KERN_WARNING "mydev: write — failed to copy %d bytes\n", not_copied);
        return -EFAULT;
    }

    buf_len = to_copy;
    kernel_buf[buf_len] = '\0';

    // strip trailing newline that echo adds 
    if (buf_len > 0 && kernel_buf[buf_len - 1] == '\n') {
        kernel_buf[--buf_len] = '\0';
    }

    printk(KERN_INFO "mydev: received %d bytes: '%s'\n", buf_len, kernel_buf);

    if (!read_done) {
        printk(KERN_WARNING
               "mydev: WRITE before READ — order violated, not counted\n");
    } else if (!write_done) {
        write_done = 1;
        printk(KERN_INFO "mydev: WRITE action completed\n");
        wake_up(&wq);
    }

    return to_copy;
}

// Module init
 static int __init mymodule_init(void)
{
    int ret;

    printk(KERN_INFO "mydev: loading — timer=%d seconds\n", timer_val);

    ret = check_kernel_version();
    if (ret != 0)
        return ret;

    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0) {
        printk(KERN_ERR "mydev: register_chrdev failed: %d\n", major_number);
        return major_number;
    }

    printk(KERN_INFO "mydev: registered — major=%d minor=0 timer=%d\n",
           major_number, timer_val);
    printk(KERN_INFO "mydev: run: sudo mknod /dev/mydev c %d 0\n", major_number);

    timer_setup(&my_timer, timer_callback, 0);
    mod_timer(&my_timer, jiffies + (unsigned long)timer_val * HZ);
    printk(KERN_INFO "mydev: timer started — %d seconds\n", timer_val);

    return 0;
}

// Module exit — verdict
 
 static void __exit mymodule_exit(void)
{
    printk(KERN_INFO "mydev: rmmod called — checking completion...\n");

    wait_event_timeout(wq,
                       (read_done && write_done) || timed_out,
                       (long)timer_val * HZ);

    del_timer_sync(&my_timer);

    if (read_done && write_done && !timed_out) {
        printk(KERN_INFO
               "mydev: Successfully completed the actions within time. "
               "Username: %s\n", kernel_buf);
    } else if (!read_done) {
        printk(KERN_ERR
               "mydev: Failure — read action was never performed\n");
    } else if (!write_done) {
        printk(KERN_ERR
               "mydev: Failure — write action was never performed\n");
    } else {
        printk(KERN_ERR
               "mydev: Failure — actions not completed within %d seconds\n",
               timer_val);
    }

    unregister_chrdev(major_number, DEVICE_NAME);
    printk(KERN_INFO "mydev: unloaded\n");
}

module_init(mymodule_init);
module_exit(mymodule_exit);
