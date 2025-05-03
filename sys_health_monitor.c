// sys_health_monitor.c
// Your Group Name: TheThreeStooges
// Team Member Names: Joshua Martin, Jacob Brashear, Nicholas Christman

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/sched.h>          // For get_avenrun
#include <linux/sysinfo.h>        // For si_meminfo
#include <linux/fs.h>             // For struct block_device
#include <linux/blkdev.h>         // For blk_stat
#include <linux/slab.h>           // For kmalloc, kfree
#include <linux/uaccess.h>        // For copy_to_user

// Module Parameters
static int mem_threshold = 100; // Default threshold (in MB)
module_param(mem_threshold, int, 0644);
MODULE_PARM_DESC(mem_threshold, "Memory usage threshold in MB");

// Structure to hold metrics
struct system_metrics {
    unsigned long cpu_load;
    unsigned long memory_used_mb;
    unsigned long disk_reads;
    unsigned long disk_writes;
    // Add other metrics here
};

// Global Variables
static struct proc_dir_entry *proc_entry;
static struct timer_list my_timer;
static struct system_metrics current_metrics;
static char *proc_buffer;
static unsigned int proc_buffer_size;

// Function Prototypes
static void my_timer_callback(struct timer_list *t);
static ssize_t sys_health_read(struct file *file, char __user *buf, size_t count, loff_t *pos);

// /proc file operations
static const struct proc_ops sys_health_fops = {
    .proc_read = sys_health_read,
};

// --------------------- Data Collection Functions ---------------------

static void collect_cpu_load(void) {
    unsigned long load[3];
    get_avenrun(load, FIXED_1, 0); // Get load averages for 1, 5, and 15 minutes
    current_metrics.cpu_load = load[0]; // Use the 1-minute load average
}

static void collect_memory_usage(void) {
    struct sysinfo info;
    si_meminfo(&info);
    current_metrics.memory_used_mb = (info.totalram - info.freeram) / (1024 * 1024); // Convert to MB
}

static void collect_disk_io(void) {
    struct block_device *bdev;
    unsigned long reads, writes;

    // Assuming you want to monitor the first block device (e.g., /dev/sda)
    bdev = blkdev_get_by_path("/dev/sda", 0, NULL); // Get the block device for /dev/sda
    if (bdev) {
        reads = blk_stat(bdev, READ);
        writes = blk_stat(bdev, WRITE);

        current_metrics.disk_reads = reads;
        current_metrics.disk_writes = writes;
        blkdev_put(bdev, 0); // Release the block device
    }
}

// --------------------- Alerting and Threshold Checks ---------------------

static void check_thresholds(void) {
    // Example: CPU Load Check
    if (current_metrics.cpu_load > 80) { // Example threshold
        printk(KERN_WARNING "TheThreeStooges Alert: CPU load exceeded threshold (%lu > 80)\n", current_metrics.cpu_load);
    }

    // Example: Memory Usage Check
    if (current_metrics.memory_used_mb > mem_threshold) {
        printk(KERN_WARNING "TheThreeStooges Alert: Memory usage exceeded threshold (%lu MB > %d MB)\n",
               current_metrics.memory_used_mb, mem_threshold);
    }

    // Example: Disk I/O Check (Simple example, adjust as needed)
    if (current_metrics.disk_reads > 10000000) {  // Example: High read count
        printk(KERN_WARNING "TheThreeStooges Alert: Disk read count high (%lu)\n", current_metrics.disk_reads);
    }
    if (current_metrics.disk_writes > 10000000) { // Example: High write count
        printk(KERN_WARNING "TheThreeStooges Alert: Disk write count high (%lu)\n", current_metrics.disk_writes);
    }
}

// --------------------- Timer Callback Function ---------------------

static void collect_metrics(void) {
    collect_cpu_load(); // Collect CPU load
    collect_memory_usage(); // Collect memory usage
    collect_disk_io(); // Collect disk I/O

    // Log the collected metrics
    printk(KERN_INFO "TheThreeStooges Metrics: CPU Load: %lu, Memory Used: %lu MB, Disk Reads: %lu, Disk Writes: %lu\n",
           current_metrics.cpu_load, current_metrics.memory_used_mb, current_metrics.disk_reads, current_metrics.disk_writes);
}

static void my_timer_callback(struct timer_list *t) {
    collect_metrics();
    check_thresholds();

    // Rearm the timer
    mod_timer(&my_timer, jiffies + msecs_to_jiffies(5000)); // Run every 5 seconds
}

// --------------------- /proc File Read Function ---------------------

static ssize_t sys_health_read(struct file *file, char __user *buf, size_t count, loff_t *pos) {
    ssize_t bytes_read = 0;
    char *output_string;
    size_t output_size;

    // 1. Gather Metrics (again, in case they changed since the timer ran)
    collect_metrics();

    // 2. Format the output string
    output_size = 256; // Adjust the size as needed
    output_string = kmalloc(output_size, GFP_KERNEL);
    if (!output_string) {
        printk(KERN_ERR "TheThreeStooges Error: Failed to allocate memory for proc output\n");
        return -ENOMEM;
    }

    snprintf(output_string, output_size,
             "CPU Load: %lu%%\nMemory Used: %lu MB\nDisk Reads: %lu\nDisk Writes: %lu\nMemory Threshold: %d MB\n",
             current_metrics.cpu_load, current_metrics.memory_used_mb, current_metrics.disk_reads, current_metrics.disk_writes, mem_threshold);

    // 3. Copy the output string to the user space buffer
    bytes_read = strlen(output_string);
    if (*pos >= bytes_read) {
        kfree(output_string);
        return 0; // End of file
    }
    if (count > bytes_read - *pos)
        count = bytes_read - *pos;
    if (copy_to_user(buf, output_string + *pos, count)) {
        kfree(output_string);
        return -EFAULT;
    }
    *pos += count;
    kfree(output_string);
    return count;
}

// --------------------- Module Initialization ---------------------

static int __init sys_health_init(void) {
    printk(KERN_INFO "TheThreeStooges sys_health module loaded. Group: %s, Members: %s\n",
           "TheThreeStooges", "Joshua Martin, Jacob Brashear, Nicholas Christman");

    // Create /proc entry
    proc_entry = proc_create("sys_health", 0444, NULL, &sys_health_fops);
    if (!proc_entry) {
        printk(KERN_ERR "TheThreeStooges Error: Could not create /proc entry\n");
        return -ENOMEM;
    }

    // Initialize the timer
    timer_setup(&my_timer, my_timer_callback, 0);
    mod_timer(&my_timer, jiffies + msecs_to_jiffies(5000)); // Start the timer

    return 0;
}

// --------------------- Module Exit ---------------------

static void __exit sys_health_exit(void) {
    printk(KERN_INFO "[CyberGuardians] sys_health module unloading. Group: %s, Members: %s\n",
           "TheThreeStooges", "Joshua Martin, Jacob Brashear, Nicholas Christman");

    // Remove the /proc entry
    proc_remove(proc_entry);

    // Clean up the timer
    del_timer_sync(&my_timer);
}

// --------------------- Module Registration ---------------------

module_init(sys_health_init);
module_exit(sys_health_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Joshua Martin, Jacob Brashear, Nicholas Christman");
MODULE_DESCRIPTION("Kernel module for system health monitoring");
```

