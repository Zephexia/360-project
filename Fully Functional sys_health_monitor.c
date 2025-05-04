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

#define MODULE_NAME "sys_health_monitor"
#define PROC_NAME "sys_health"
#define TIMER_INTERVAL (5 * HZ) // 5 seconds

static struct timer_list health_timer;
static struct proc_dir_entry *proc_entry;

// Existing memory threshold parameter
static int mem_threshold = 100;  // in MB
module_param(mem_threshold, int, 0644);
MODULE_PARM_DESC(mem_threshold, "Memory usage threshold in MB");

// --- ADDED: CPU and Disk I/O thresholds ---
static int cpu_threshold = 70;  // in percent
module_param(cpu_threshold, int, 0644);
MODULE_PARM_DESC(cpu_threshold, "CPU load threshold (percentage)");

static int io_threshold = 500;  // in jiffies delta
module_param(io_threshold, int, 0644);
MODULE_PARM_DESC(io_threshold, "Disk I/O activity threshold (jiffies)");

static unsigned long last_total_io = 0;

static unsigned long current_free_mem = 0;
static unsigned long current_total_mem = 0;
static unsigned long current_cpu_load = 0;
static unsigned long current_disk_io = 0;

static void collect_metrics(struct timer_list *t)
{
    struct sysinfo info;
    unsigned long used_mem_mb;

    si_meminfo(&info);
    current_total_mem = info.totalram >> 10; // in MB
    current_free_mem = info.freeram >> 10;   // in MB
    used_mem_mb = current_total_mem - current_free_mem;

    // Check memory threshold
    if (used_mem_mb > mem_threshold) {
        printk(KERN_WARNING "[TheThreeStooges] Alert: Memory usage exceeded threshold (%luMB used > %dMB)\n",
               used_mem_mb, mem_threshold);
    }

    // CPU Load: average over 1 minute scaled by 65536
    current_cpu_load = (avenrun[0] * 100) >> FSHIFT; // scale down

    // Check CPU load threshold
    if (current_cpu_load > cpu_threshold) {
        printk(KERN_WARNING "[TheThreeStooges] Alert: CPU load exceeded threshold (%lu%% > %d%%)\n",
               current_cpu_load, cpu_threshold);
    }

    // Disk I/O — simplified approximation from I/O jiffies
    current_disk_io = jiffies - last_total_io;
    last_total_io = jiffies;

    // Check Disk I/O threshold
    if (current_disk_io > io_threshold) {
        printk(KERN_WARNING "[TheThreeStooges] Alert: Disk I/O exceeded threshold (delta %lu jiffies > %d)\n",
               current_disk_io, io_threshold);
    }

    // Rearm the timer
    mod_timer(&health_timer, jiffies + TIMER_INTERVAL);
}

static int proc_show(struct seq_file *m, void *v)
{
    seq_printf(m, "=== Real-Time System Health Metrics ===\n");
    seq_printf(m, "Group: TheThreeStooges\n");
    seq_printf(m, "Members: Joshua Martin, Jacob Brashear, Nicholas Christman\n\n");

    seq_printf(m, "Total Memory: %lu MB\n", current_total_mem);
    seq_printf(m, "Free Memory:  %lu MB\n", current_free_mem);
    seq_printf(m, "Memory Used:  %lu MB\n", current_total_mem - current_free_mem);
    seq_printf(m, "CPU Load:     %lu %%\n", current_cpu_load);
    seq_printf(m, "Disk I/O (delta ticks): %lu\n", current_disk_io);

    // Show thresholds in /proc output
    seq_printf(m, "Thresholds:\n");
    seq_printf(m, "  Memory:  %d MB\n", mem_threshold);
    seq_printf(m, "  CPU:     %d %%\n", cpu_threshold);
    seq_printf(m, "  Disk I/O: %d jiffies\n", io_threshold);

    return 0;
}

static int proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, proc_show, NULL);
}

static const struct proc_ops proc_file_ops = {
    .proc_open    = proc_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

static int __init sys_health_init(void)
{
    printk(KERN_INFO "[TheThreeStooges] SCIA 360: Module loaded successfully. Team Members: Joshua Martin, Jacob Brashear, Nicholas Christman\n");

    proc_entry = proc_create(PROC_NAME, 0444, NULL, &proc_file_ops);
    if (!proc_entry) {
        printk(KERN_ERR "[TheThreeStooges] Failed to create /proc/%s entry\n", PROC_NAME);
        return -ENOMEM;
    }

    // Initialize and start the timer
    timer_setup(&health_timer, collect_metrics, 0);
    last_total_io = jiffies;
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
