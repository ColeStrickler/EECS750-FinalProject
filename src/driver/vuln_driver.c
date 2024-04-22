
#include <linux/init.h> /* Needed for the macros */ 
#include <linux/module.h> /* Needed by all modules */ 
#include <linux/printk.h> /* Needed for pr_info() */ 
#include <linux/mutex.h> 
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/cdev.h> 
#include <linux/slab.h> // Include for kmalloc

#define IOCTL_COMMAND_TRAIN         _IO('k', 1)
#define IOCTL_COMMAND_HOLD          _IO('k', 2)
#define IOCTL_COMMAND_TRANSMIT      _IO('k', 3)
#define IOCTL_COMMAND_RELEASE       _IO('k', 4)
#define THRESHOLD 250
#define L3_CACHE_LINE_SIZE 4096*4
#define flush(p) \
    asm volatile ("clflush 0(%0)\n" \
      : \
      : "c" (p) \
      : "rax")


static inline unsigned long probe_timing(char *adrs) {
    volatile unsigned long time;

    asm __volatile__(
        "    mfence             \n"
        "    lfence             \n"
        "    rdtsc              \n"
        "    lfence             \n"
        "    movl %%eax, %%esi  \n"
        "    movl (%1), %%eax   \n"
        "    lfence             \n"
        "    rdtsc              \n"
        "    subl %%esi, %%eax  \n"
        "    clflush 0(%1)      \n"
        : "=a" (time)
        : "c" (adrs)
        : "%esi", "%edx"
    );
    return time;
}


typedef void (*callback_t)(int* arg);
typedef struct victim_struct
{
    callback_t callback;
} victim_struct;

struct ioctl_data {
    uint64_t address;
    uint64_t offset;
    char* buf;
};

#define L3_CACHE_LINE_SIZE 4096*4
static DEFINE_MUTEX(lock); 

static char* DEVICE_NAME = "vuln_driver";
static char* SECRET = "This is the secret message we will leak from kernel space";
static int MAJOR_NUMBER;
static struct class *cls; 
static int major = -1;
static struct cdev mycdev;
static struct class *myclass = NULL;
static struct device *dev;
char* buf;

void flush_buf(void)
{
    flush(&lock);
}


char spectre_read(void)
{
    char c = 0x0;
    for (int j = 0; j < 255; j++)
    {
        unsigned long timing;
        if ((timing = probe_timing(&buf[j * L3_CACHE_LINE_SIZE])) < THRESHOLD)
        {
            c = j;
            flush(&buf[j * L3_CACHE_LINE_SIZE]);
            //printk("Hit timing %d --> j == %c\n", timing, j);
            // printf("j: %d\n", j);
            break;
        }

        //printk("Miss timing %d\n", timing);
    }

    return c;
}

void leak_secret(char *c)
{
    char d = *c;
    buf[*c * L3_CACHE_LINE_SIZE] = 1;
}

void train_mutex(void)
{
    for (int i = 0; i < 1000000; i++)
    {
        mutex_trylock(&lock);
        mutex_unlock(&lock);
    }
}

static long ioctl_dispatch(struct file *file, unsigned int cmd, unsigned long arg) {
    int ret = 0;
    switch (cmd) {
        case IOCTL_COMMAND_TRAIN:
        {
            printk("IOCTL_COMMAND_TRAIN\n");
            train_mutex();
            break;
        }
        case IOCTL_COMMAND_HOLD:
        {
            printk("IOCTL_COMMAND_HOLD\n");
            mutex_lock(&lock);
            flush_buf();
            break;
        }
        case IOCTL_COMMAND_TRANSMIT:
        {
            struct ioctl_data data;
            
            int err;
            if ((err = copy_from_user(&data, (struct ioctl_data *)arg, sizeof(struct ioctl_data)))) 
                return -EFAULT; // Error copying data
            if (likely(mutex_trylock(&lock) == 1))
            {
                //leak_secret(data.address + data.offset);
                leak_secret(SECRET + data.offset);
            }
            ret = spectre_read();
            break;
        }
        case IOCTL_COMMAND_RELEASE:
        {
            mutex_unlock(&lock);
            break;
        }
        default:
            return -ENOTTY;  // Not a valid IOCTL command
    }
    return ret;
}

static struct file_operations fops = {
    .unlocked_ioctl = ioctl_dispatch
};

 
static int __init driver_entry(void) 
{ 

    printk("driver_entry()\n");

    MAJOR_NUMBER = register_chrdev(0, DEVICE_NAME, &fops);
    if (MAJOR_NUMBER < 0)
    {
        printk(KERN_ERR "Failed to register character device\n");
        return MAJOR_NUMBER;
    }

    cls = class_create(DEVICE_NAME);
    dev = device_create(cls, NULL, MKDEV(MAJOR_NUMBER, 0), NULL, DEVICE_NAME);
    if (IS_ERR(dev)) {
        class_destroy(cls);
        unregister_chrdev(MAJOR_NUMBER, DEVICE_NAME);
        printk(KERN_ERR "Failed to create device\n");
        return PTR_ERR(dev);
    }

    buf = kmalloc(255*L3_CACHE_LINE_SIZE+1,GFP_KERNEL);
    memset(buf, 'x', 255*L3_CACHE_LINE_SIZE);
    if (!buf)
    {
        printk(KERN_ERR "Failed to allocate memory\n");
        return -1;
    }


    printk("Major number assignment: %d", MAJOR_NUMBER);
    printk("Device created on /dev/%s\n", DEVICE_NAME); 
    printk(KERN_INFO "VULN_DRIVER initialized.\n");
    return 0; 
} 
 
static void __exit driver_exit(void) 
{ 

    device_destroy(cls, MKDEV(MAJOR_NUMBER, 0));
    class_destroy(cls);
    unregister_chrdev(MAJOR_NUMBER, DEVICE_NAME);
    kfree(buf);
    printk("VULN_DRIVER exit.\n"); 
} 
 
module_init(driver_entry); 
module_exit(driver_exit); 



MODULE_LICENSE("GPL");