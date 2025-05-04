/*
 * Group Name: TheThreeStooges
 * Group Members: Joshua Martin, Jacob Brashear, Nicholas Christman
 * Course: SCIA 360 - Operating System Security
 * Project: Linux Kernel Module for Real-Time Health Monitoring
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/mm.h>
#include <linux/sysinfo.h>
#include <linux/moduleparam.h>
#include <linux/sched/loadavg.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/hdreg.h>

#define MODULE_NAME "sys_health_monitor"
#define PROC_NAME "sys_health"
#define TIMER_INTERVAL (5 * HZ) // 5 seconds

// Module parameters
static int mem_threshold = 100;  // in MB
module_param(mem_threshold, int, 0644);
MODULE_PARM_DESC(mem_threshold, "Memory usage threshold in MB");

static int cpu_load_threshold = 80; // Percentage
module_param(cpu_load_threshold, int, 0644);
MODULE_PARM_DESC(cpu_load_threshold, "CPU load threshold in percentage");

// Global variables to store metrics
static unsigned long current_total_mem = 0;
static unsigned long current_free_mem = 0;
static unsigned long current_cpu_load = 0;
static unsigned long current_disk_io_reads = 0;
static unsigned long current_disk_io_writes = 0;
static unsigned long previous_disk_io_reads = 0;
static unsigned long previous_disk_io_writes = 0;

static struct timer_list health_timer;
static struct proc_dir_entry *proc_entry;

// --- Forward Declarations ---
static void collect_metrics(struct timer_list *t);
static int proc_show(struct seq_file *m, void *v);
static int proc_open(struct inode *inode, struct file *file);
static void get_disk_io(void); // Function to collect disk I/O

// --- /proc File Operations ---
static const struct proc_ops proc_file_ops = {
    .proc_open    = proc_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

// --- Timer Callback Function ---
static void collect_metrics(struct timer_list *t)
{
    struct sysinfo info;
    unsigned long used_mem_mb;

    // 1. Collect Memory Usage
    si_meminfo(&info);
    current_total_mem = info.totalram >> 10; // in MB
    current_free_mem = info.freeram >> 10;   // in MB
    used_mem_mb = current_total_mem - current_free_mem;

    // 2. Collect CPU Load
    current_cpu_load = (avenrun[0] * 100) >> FSHIFT; // Scale down to percentage

    // 3. Collect Disk I/O
    get_disk_io(); // Call the function to get disk I/O stats

    // 4. Check Memory Threshold
    if (used_mem_mb > mem_threshold) {
        printk(KERN_WARNING "[TheThreeStooges] Alert: Memory usage exceeded threshold (%luMB used > %dMB)\n",
               used_mem_mb, mem_threshold);
    }

    // 5. Check CPU Load
    if (current_cpu_load > cpu_load_threshold) {
        printk(KERN_WARNING "[TheThreeStooges] Alert: CPU load exceeded threshold (%lu%% > %d%%)\n",
               current_cpu_load, cpu_load_threshold);
    }

    // 6. Rearm the timer
    mod_timer(&health_timer, jiffies + TIMER_INTERVAL);
}

// --- /proc File Functions ---
static int proc_show(struct seq_file *m, void *v)
{
    seq_printf(m, "=== Real-Time System Health Metrics ===\n");
    seq_printf(m, "Group: TheThreeStooges\n");
    seq_printf(m, "Members: Joshua Martin, Jacob Brashear, Nicholas Christman\n\n");
    seq_printf(m, "Total Memory: %lu MB\n", current_total_mem);
    seq_printf(m, "Free Memory:  %lu MB\n", current_free_mem);
    seq_printf(m, "Memory Used:  %lu MB\n", current_total_mem - current_free_mem);
    seq_printf(m, "CPU Load:     %lu %%\n", current_cpu_load);
    seq_printf(m, "Disk Reads:   %lu\n", current_disk_io_reads);
    seq_printf(m, "Disk Writes:  %lu\n", current_disk_io_writes);
    return 0;
}

static int proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, proc_show, NULL);
}

// --- Disk I/O Collection Function ---
static void get_disk_io(void)
{
    struct block_device *bdev;
    struct gendisk *gd;
    unsigned long reads = 0, writes = 0;

    // Iterate through all block devices
    for (unsigned int i = 0; i < MAX_DEVICES; i++) {
        bdev = bdget(i);
        if (bdev) {
            gd = bdev->bd_disk;
            if (gd) {
                // Accessing read/write statistics
                reads += gd->part0.nr_sects_read;  // Placeholder for actual read statistics
                writes += gd->part0.nr_sects_written; // Placeholder for actual write statistics
            }
            bdput(bdev);
        }
    }

    // Update disk I/O counts
    previous_disk_io_reads = current_disk_io_reads;
    previous_disk_io_writes = current_disk_io_writes;
    current_disk_io_reads = reads;
    current_disk_io_writes = writes;
}

// --- Module Initialization and Exit ---
static int __init sys_health_init(void)
{
    printk(KERN_INFO "[TheThreeStooges] SCIA 360: Module loaded successfully. Team Members: Joshua Martin, Jacob Brashear, Nicholas Christman\n");

    // Create /proc entry
    proc_entry = proc_create(PROC_NAME, 0444, NULL, &proc_file_ops);
    if (!proc_entry) {
        printk(KERN_ERR "[TheThreeStooges] Failed to create /proc/%s entry\n", PROC_NAME);
        return -ENOMEM;
    }

    // Initialize and start the timer
    timer_setup(&health_timer, collect_metrics, 0);
    mod_timer(&health_timer, jiffies + TIMER_INTERVAL);

    return 0;
}

static void __exit sys_health_exit(void)
{
    remove_proc_entry(PROC_NAME, NULL);
    del_timer_sync(&health_timer);
    printk(KERN_INFO "[TheThreeStooges] SCIA 360: Module unloaded. Team Members: Joshua Martin, Jacob Brashear, Nicholas Christman\n");
}

module_init(sys_health_init);
module_exit(sys_health_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("TheThreeStooges");
MODULE_DESCRIPTION("Real-Time System Health Monitor for SCIA 360");
